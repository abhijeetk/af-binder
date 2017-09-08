/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
 * Author José Bollo <jose.bollo@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#define NO_PLUGIN_VERBOSE_MACRO

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include <json-c/json.h>
#include <systemd/sd-event.h>

#include "afb-common.h"

#include "afb-ws.h"
#include "afb-msg-json.h"
#include "afb-proto-ws.h"
#include "verbose.h"

struct afb_proto_ws;

/******** implementation of internal binder protocol per api **************/
/*

This protocol is asymetric: there is a client and a server

The client can require the following actions:

  - call a verb

  - ask for description

The server must reply to the previous actions by 

  - answering success or failure of the call

  - answering the required description

The server can also within the context of a call

  - make a subcall

  - subscribe or unsubscribe an event

For the purpose of handling events the server can:

  - create/destroy an event

  - push or brodcast data as an event

*/
/************** constants for protocol definition *************************/

#define CHAR_FOR_CALL             'C'
#define CHAR_FOR_ANSWER_SUCCESS   'T'
#define CHAR_FOR_ANSWER_FAIL      'F'
#define CHAR_FOR_EVT_BROADCAST    '*'
#define CHAR_FOR_EVT_ADD          '+'
#define CHAR_FOR_EVT_DEL          '-'
#define CHAR_FOR_EVT_PUSH         '!'
#define CHAR_FOR_EVT_SUBSCRIBE    'S'
#define CHAR_FOR_EVT_UNSUBSCRIBE  'U'
#define CHAR_FOR_SUBCALL_CALL     'B'
#define CHAR_FOR_SUBCALL_REPLY    'R'
#define CHAR_FOR_DESCRIBE         'D'
#define CHAR_FOR_DESCRIPTION      'd'

/******************* handling subcalls *****************************/

/**
 * Structure on server side for recording pending
 * subcalls.
 */
struct server_subcall
{
	struct server_subcall *next;	/**< next subcall for the client */
	uint32_t subcallid;		/**< the subcallid */
	void (*callback)(void*, int, struct json_object*); /**< callback on completion */
	void *closure;			/**< closure of the callback */
};

/**
 * Structure for sending back replies on client side
 */
struct afb_proto_ws_subcall
{
	struct afb_proto_ws *protows;	/**< proto descriptor */
	void *buffer;
	uint32_t subcallid;		/**< subcallid for the reply */
};

/*
 * structure for recording calls on client side
 */
struct client_call {
	struct client_call *next;	/* the next call */
	struct afb_proto_ws *protows;	/* the proto_ws */
	void *request;
	uint32_t callid;		/* the message identifier */
};

/*
 * structure for a ws request
 */
struct afb_proto_ws_call {
	struct client_call *next;	/* the next call */
	struct afb_proto_ws *protows;	/* the client of the request */
	uint32_t refcount;		/* reference count */
	uint32_t callid;		/* the incoming request callid */
	char *buffer;			/* the incoming buffer */
};

/*
 * structure for recording describe requests
 */
struct client_describe
{
	struct client_describe *next;
	struct afb_proto_ws *protows;
	void (*callback)(void*, struct json_object*);
	void *closure;
	uint32_t descid;
};

/*
 * structure for jobs of describing
 */
struct afb_proto_ws_describe
{
	struct afb_proto_ws *protows;
	uint32_t descid;
};

/******************* proto description for client or servers ******************/

struct afb_proto_ws
{
	/* count of references */
	int refcount;

	/* file descriptor */
	int fd;

	/* resource control */
	pthread_mutex_t mutex;

	/* websocket */
	struct afb_ws *ws;

	/* the client closure */
	void *closure;

	/* the client side interface */
	const struct afb_proto_ws_client_itf *client_itf;

	/* the server side interface */
	const struct afb_proto_ws_server_itf *server_itf;

	/* emitted calls (client side) */
	struct client_call *calls;

	/* pending subcalls (server side) */
	struct server_subcall *subcalls;

	/* pending description (client side) */
	struct client_describe *describes;

	/* on hangup callback */
	void (*on_hangup)(void *closure);
};

/******************* common useful tools **********************************/

/**
 * translate a pointer to some integer
 * @param ptr the pointer to translate
 * @return an integer
 */
static inline uint32_t ptr2id(void *ptr)
{
	return (uint32_t)(((intptr_t)ptr) >> 6);
}

/******************* serialisation part **********************************/

struct readbuf
{
	char *base, *head, *end;
};

#define WRITEBUF_COUNT_MAX  32
struct writebuf
{
	struct iovec iovec[WRITEBUF_COUNT_MAX];
	uint32_t uints[WRITEBUF_COUNT_MAX];
	int count;
};

static char *readbuf_get(struct readbuf *rb, uint32_t length)
{
	char *before = rb->head;
	char *after = before + length;
	if (after > rb->end)
		return 0;
	rb->head = after;
	return before;
}

static int readbuf_char(struct readbuf *rb, char *value)
{
	if (rb->head >= rb->end)
		return 0;
	*value = *rb->head++;
	return 1;
}

static int readbuf_uint32(struct readbuf *rb, uint32_t *value)
{
	char *after = rb->head + sizeof *value;
	if (after > rb->end)
		return 0;
	memcpy(value, rb->head, sizeof *value);
	rb->head = after;
	*value = le32toh(*value);
	return 1;
}

static int readbuf_string(struct readbuf *rb, const char **value, size_t *length)
{
	uint32_t len;
	if (!readbuf_uint32(rb, &len) || !len)
		return 0;
	if (length)
		*length = (size_t)(len - 1);
	return (*value = readbuf_get(rb, len)) != NULL &&  rb->head[-1] == 0;
}

static int readbuf_object(struct readbuf *rb, struct json_object **object)
{
	const char *string;
	struct json_object *o;
	int rc = readbuf_string(rb, &string, NULL);
	if (rc) {
		o = json_tokener_parse(string);
		if (o == NULL && strcmp(string, "null"))
			o = json_object_new_string(string);
		*object = o;
	}
	return rc;
}

static int writebuf_put(struct writebuf *wb, const void *value, size_t length)
{
	int i = wb->count;
	if (i == WRITEBUF_COUNT_MAX)
		return 0;
	wb->iovec[i].iov_base = (void*)value;
	wb->iovec[i].iov_len = length;
	wb->count = i + 1;
	return 1;
}

static int writebuf_char(struct writebuf *wb, char value)
{
	int i = wb->count;
	if (i == WRITEBUF_COUNT_MAX)
		return 0;
	*(char*)&wb->uints[i] = value;
	wb->iovec[i].iov_base = &wb->uints[i];
	wb->iovec[i].iov_len = 1;
	wb->count = i + 1;
	return 1;
}

static int writebuf_uint32(struct writebuf *wb, uint32_t value)
{
	int i = wb->count;
	if (i == WRITEBUF_COUNT_MAX)
		return 0;
	wb->uints[i] = htole32(value);
	wb->iovec[i].iov_base = &wb->uints[i];
	wb->iovec[i].iov_len = sizeof wb->uints[i];
	wb->count = i + 1;
	return 1;
}

static int writebuf_string_length(struct writebuf *wb, const char *value, size_t length)
{
	uint32_t len = (uint32_t)++length;
	return (size_t)len == length && len && writebuf_uint32(wb, len) && writebuf_put(wb, value, length);
}

static int writebuf_string(struct writebuf *wb, const char *value)
{
	return writebuf_string_length(wb, value, strlen(value));
}

static int writebuf_object(struct writebuf *wb, struct json_object *object)
{
	const char *string = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
	return string != NULL && writebuf_string(wb, string);
}

/******************* ws request part for server *****************/

void afb_proto_ws_call_addref(struct afb_proto_ws_call *call)
{
	__atomic_add_fetch(&call->refcount, 1, __ATOMIC_RELAXED);
}

void afb_proto_ws_call_unref(struct afb_proto_ws_call *call)
{
	if (__atomic_sub_fetch(&call->refcount, 1, __ATOMIC_RELAXED))
		return;

	afb_proto_ws_unref(call->protows);
	free(call->buffer);
	free(call);
}

int afb_proto_ws_call_success(struct afb_proto_ws_call *call, struct json_object *obj, const char *info)
{
	int rc = -1;
	struct writebuf wb = { .count = 0 };

	if (writebuf_char(&wb, CHAR_FOR_ANSWER_SUCCESS)
	 && writebuf_uint32(&wb, call->callid)
	 && writebuf_string(&wb, info ?: "")
	 && writebuf_object(&wb, obj)) {
		rc = afb_ws_binary_v(call->protows->ws, wb.iovec, wb.count);
		if (rc >= 0) {
			rc = 0;
			goto success;
		}
	}
	ERROR("error while sending success");
success:
	return rc;
}

int afb_proto_ws_call_fail(struct afb_proto_ws_call *call, const char *status, const char *info)
{
	int rc = -1;
	struct writebuf wb = { .count = 0 };

	if (writebuf_char(&wb, CHAR_FOR_ANSWER_FAIL)
	 && writebuf_uint32(&wb, call->callid)
	 && writebuf_string(&wb, status)
	 && writebuf_string(&wb, info ? : "")) {
		rc = afb_ws_binary_v(call->protows->ws, wb.iovec, wb.count);
		if (rc >= 0) {
			rc = 0;
			goto success;
		}
	}
	ERROR("error while sending fail");
success:
	return rc;
}

int afb_proto_ws_call_subcall(struct afb_proto_ws_call *call, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	int rc = -1;
	struct writebuf wb = { .count = 0 };
	struct server_subcall *sc, *osc;
	struct afb_proto_ws *protows = call->protows;

	sc = malloc(sizeof *sc);
	if (!sc)
		errno = ENOMEM;
	else {
		sc->callback = callback;
		sc->closure = cb_closure;

		pthread_mutex_unlock(&protows->mutex);
		sc->subcallid = ptr2id(sc);
		do {
			sc->subcallid++;
			osc = protows->subcalls;
			while(osc && osc->subcallid != sc->subcallid)
				osc = osc->next;
		} while (osc);
		sc->next = protows->subcalls;
		protows->subcalls = sc;
		pthread_mutex_unlock(&protows->mutex);

		if (writebuf_char(&wb, CHAR_FOR_SUBCALL_CALL)
		 && writebuf_uint32(&wb, sc->subcallid)
		 && writebuf_uint32(&wb, call->callid)
		 && writebuf_string(&wb, api)
		 && writebuf_string(&wb, verb)
		 && writebuf_object(&wb, args)) {
			rc = afb_ws_binary_v(protows->ws, wb.iovec, wb.count);
			if (rc >= 0) {
				rc = 0;
				goto success;
			}
		}
	}
	ERROR("error while sending subcall");
success:
	return rc;
}

int afb_proto_ws_call_subscribe(struct afb_proto_ws_call *call, const char *event_name, int event_id)
{
	int rc = -1;
	struct writebuf wb = { .count = 0 };

	if (writebuf_char(&wb, CHAR_FOR_EVT_SUBSCRIBE)
	 && writebuf_uint32(&wb, call->callid)
	 && writebuf_uint32(&wb, (uint32_t)event_id)
	 && writebuf_string(&wb, event_name)) {
		rc = afb_ws_binary_v(call->protows->ws, wb.iovec, wb.count);
		if (rc >= 0) {
			rc = 0;
			goto success;
		}
	}
	ERROR("error while subscribing event");
success:
	return rc;
}

int afb_proto_ws_call_unsubscribe(struct afb_proto_ws_call *call, const char *event_name, int event_id)
{
	int rc = -1;
	struct writebuf wb = { .count = 0 };

	if (writebuf_char(&wb, CHAR_FOR_EVT_UNSUBSCRIBE)
	 && writebuf_uint32(&wb, call->callid)
	 && writebuf_uint32(&wb, (uint32_t)event_id)
	 && writebuf_string(&wb, event_name)) {
		rc = afb_ws_binary_v(call->protows->ws, wb.iovec, wb.count);
		if (rc >= 0) {
			rc = 0;
			goto success;
		}
	}
	ERROR("error while subscribing event");
success:
	return rc;
}

/******************* client part **********************************/

/* search a memorized call */
static struct client_call *client_call_search(struct afb_proto_ws *protows, uint32_t callid)
{
	struct client_call *call;

	call = protows->calls;
	while (call != NULL && call->callid != callid)
		call = call->next;

	return call;
}

/* free and release the memorizing call */
static void client_call_destroy(struct client_call *call)
{
	struct client_call **prv;

	prv = &call->protows->calls;
	while (*prv != NULL) {
		if (*prv == call) {
			*prv = call->next;
			break;
		}
		prv = &(*prv)->next;
	}
	free(call);
}

/* get event data from the message */
static int client_msg_event_read(struct readbuf *rb, uint32_t *eventid, const char **name)
{
	return readbuf_uint32(rb, eventid) && readbuf_string(rb, name, NULL);
}

/* get event from the message */
static int client_msg_call_get(struct afb_proto_ws *protows, struct readbuf *rb, struct client_call **call)
{
	uint32_t callid;

	/* get event data from the message */
	if (!readbuf_uint32(rb, &callid)) {
		ERROR("Invalid message");
		return 0;
	}

	/* get the call */
	*call = client_call_search(protows, callid);
	if (*call == NULL) {
		ERROR("message not found");
		return 0;
	}

	return 1;
}

/* adds an event */
static void client_on_event_create(struct afb_proto_ws *protows, struct readbuf *rb)
{
	const char *event_name;
	uint32_t event_id;

	if (protows->client_itf->on_event_create && client_msg_event_read(rb, &event_id, &event_name))
		protows->client_itf->on_event_create(protows->closure, event_name, (int)event_id);
}

/* removes an event */
static void client_on_event_remove(struct afb_proto_ws *protows, struct readbuf *rb)
{
	const char *event_name;
	uint32_t event_id;

	if (protows->client_itf->on_event_remove && client_msg_event_read(rb, &event_id, &event_name))
		protows->client_itf->on_event_remove(protows->closure, event_name, (int)event_id);
}

/* subscribes an event */
static void client_on_event_subscribe(struct afb_proto_ws *protows, struct readbuf *rb)
{
	const char *event_name;
	uint32_t event_id;
	struct client_call *call;

	if (protows->client_itf->on_event_subscribe && client_msg_call_get(protows, rb, &call) && client_msg_event_read(rb, &event_id, &event_name))
		protows->client_itf->on_event_subscribe(protows->closure, call->request, event_name, (int)event_id);
}

/* unsubscribes an event */
static void client_on_event_unsubscribe(struct afb_proto_ws *protows, struct readbuf *rb)
{
	const char *event_name;
	uint32_t event_id;
	struct client_call *call;

	if (protows->client_itf->on_event_unsubscribe && client_msg_call_get(protows, rb, &call) && client_msg_event_read(rb, &event_id, &event_name))
		protows->client_itf->on_event_unsubscribe(protows->closure, call->request, event_name, (int)event_id);
}

/* receives broadcasted events */
static void client_on_event_broadcast(struct afb_proto_ws *protows, struct readbuf *rb)
{
	const char *event_name;
	struct json_object *object;

	if (protows->client_itf->on_event_broadcast && readbuf_string(rb, &event_name, NULL) && readbuf_object(rb, &object))
		protows->client_itf->on_event_broadcast(protows->closure, event_name, object);
}

/* pushs an event */
static void client_on_event_push(struct afb_proto_ws *protows, struct readbuf *rb)
{
	const char *event_name;
	uint32_t event_id;
	struct json_object *object;

	if (protows->client_itf->on_event_push && client_msg_event_read(rb, &event_id, &event_name) && readbuf_object(rb, &object))
		protows->client_itf->on_event_push(protows->closure, event_name, (int)event_id, object);
}

static void client_on_reply_success(struct afb_proto_ws *protows, struct readbuf *rb)
{
	struct client_call *call;
	struct json_object *object;
	const char *info;

	if (!client_msg_call_get(protows, rb, &call))
		return;

	if (readbuf_string(rb, &info, NULL) && readbuf_object(rb, &object)) {
		protows->client_itf->on_reply_success(protows->closure, call->request, object, info);
	} else {
		protows->client_itf->on_reply_fail(protows->closure, call->request, "proto-error", "can't process success");
	}
	client_call_destroy(call);
}

static void client_on_reply_fail(struct afb_proto_ws *protows, struct readbuf *rb)
{
	struct client_call *call;
	const char *info, *status;

	if (!client_msg_call_get(protows, rb, &call))
		return;

	if (readbuf_string(rb, &status, NULL) && readbuf_string(rb, &info, NULL)) {
		protows->client_itf->on_reply_fail(protows->closure, call->request, status, info);
	} else {
		protows->client_itf->on_reply_fail(protows->closure, call->request, "proto-error", "can't process fail");
	}
	client_call_destroy(call);
}

/* send a subcall reply */
static int client_send_subcall_reply(struct afb_proto_ws *protows, uint32_t subcallid, int status, json_object *object)
{
	struct writebuf wb = { .count = 0 };
	char ie = status < 0;

	return -!(writebuf_char(&wb, CHAR_FOR_SUBCALL_REPLY)
	 && writebuf_uint32(&wb, subcallid)
	 && writebuf_char(&wb, ie)
	 && writebuf_object(&wb, object)
	 && afb_ws_binary_v(protows->ws, wb.iovec, wb.count) >= 0);
}

/* callback for subcall reply */
int afb_proto_ws_subcall_reply(struct afb_proto_ws_subcall *subcall, int status, struct json_object *result)
{
	int rc = client_send_subcall_reply(subcall->protows, subcall->subcallid, status, result);
	afb_proto_ws_unref(subcall->protows);
	free(subcall->buffer);
	free(subcall);
	return rc;
}

/* received a subcall request */
static void client_on_subcall(struct afb_proto_ws *protows, struct readbuf *rb)
{
	struct afb_proto_ws_subcall *subcall;
	struct client_call *call;
	const char *api, *verb;
	uint32_t subcallid;
	struct json_object *object;

	/* get the subcallid */
	if (!readbuf_uint32(rb, &subcallid))
		return;

	/* if not expected drop it */
	if (!protows->client_itf->on_subcall)
		goto error;

	/* retrieve the message data */
	if (!client_msg_call_get(protows, rb, &call))
		goto error;

	/* allocation of the subcall */
	subcall = malloc(sizeof *subcall);
	if (!subcall)
		goto error;

	/* make the call */
	if (readbuf_string(rb, &api, NULL)
	 && readbuf_string(rb, &verb, NULL)
	 && readbuf_object(rb, &object)) {
		afb_proto_ws_addref(protows);
		subcall->protows = protows;
		subcall->subcallid = subcallid;
		subcall->buffer = rb->base;
		rb->base = NULL;
		protows->client_itf->on_subcall(protows->closure, subcall, call->request, api, verb, object);
		return;
	}
	free(subcall);
error:
	client_send_subcall_reply(protows, subcallid, 1, NULL);
}

static void client_on_description(struct afb_proto_ws *protows, struct readbuf *rb)
{
	uint32_t descid;
	struct client_describe *desc, **prv;
	struct json_object *object;

	if (readbuf_uint32(rb, &descid)) {
		prv = &protows->describes;
		while ((desc = *prv) && desc->descid != descid)
			prv = &desc->next;
		if (desc) {
			*prv = desc->next;
			if (!readbuf_object(rb, &object))
				object = NULL;
			desc->callback(desc->closure, object);
			free(desc);
		}
	}
}

/* callback when receiving binary data */
static void client_on_binary(void *closure, char *data, size_t size)
{
	struct afb_proto_ws *protows;
	struct readbuf rb;

	rb.base = data;
	if (size > 0) {
		rb.head = data;
		rb.end = data + size;
		protows = closure;

		pthread_mutex_lock(&protows->mutex);
		switch (*rb.head++) {
		case CHAR_FOR_ANSWER_SUCCESS: /* success */
			client_on_reply_success(protows, &rb);
			break;
		case CHAR_FOR_ANSWER_FAIL: /* fail */
			client_on_reply_fail(protows, &rb);
			break;
		case CHAR_FOR_EVT_BROADCAST: /* broadcast */
			client_on_event_broadcast(protows, &rb);
			break;
		case CHAR_FOR_EVT_ADD: /* creates the event */
			client_on_event_create(protows, &rb);
			break;
		case CHAR_FOR_EVT_DEL: /* removes the event */
			client_on_event_remove(protows, &rb);
			break;
		case CHAR_FOR_EVT_PUSH: /* pushs the event */
			client_on_event_push(protows, &rb);
			break;
		case CHAR_FOR_EVT_SUBSCRIBE: /* subscribe event for a request */
			client_on_event_subscribe(protows, &rb);
			break;
		case CHAR_FOR_EVT_UNSUBSCRIBE: /* unsubscribe event for a request */
			client_on_event_unsubscribe(protows, &rb);
			break;
		case CHAR_FOR_SUBCALL_CALL: /* subcall */
			client_on_subcall(protows, &rb);
			break;
		case CHAR_FOR_DESCRIPTION: /* description */
			client_on_description(protows, &rb);
			break;
		default: /* unexpected message */
			/* TODO: close the connection */
			break;
		}
		pthread_mutex_unlock(&protows->mutex);
	}
	free(rb.base);
}

int afb_proto_ws_client_call(
		struct afb_proto_ws *protows,
		const char *verb,
		struct json_object *args,
		const char *sessionid,
		void *request
)
{
	int rc = -1;
	struct client_call *call;
	struct writebuf wb = { .count = 0 };

	/* allocate call data */
	call = malloc(sizeof *call);
	if (call == NULL) {
		errno = ENOMEM;
		return -1;
	}
	call->request = request;

	/* init call data */
	pthread_mutex_lock(&protows->mutex);
	call->callid = ptr2id(call);
	while(client_call_search(protows, call->callid) != NULL)
		call->callid++;
	call->protows = protows;
	call->next = protows->calls;
	protows->calls = call;

	/* creates the call message */
	if (!writebuf_char(&wb, CHAR_FOR_CALL)
	 || !writebuf_uint32(&wb, call->callid)
	 || !writebuf_string(&wb, verb)
	 || !writebuf_string(&wb, sessionid)
	 || !writebuf_object(&wb, args)) {
		errno = EINVAL;
		goto clean;
	}

	/* send */
	rc = afb_ws_binary_v(protows->ws, wb.iovec, wb.count);
	if (rc >= 0) {
		rc = 0;
		goto end;
	}

clean:
	client_call_destroy(call);
end:
	pthread_mutex_unlock(&protows->mutex);
	return rc;
}

/* get the description */
int afb_proto_ws_client_describe(struct afb_proto_ws *protows, void (*callback)(void*, struct json_object*), void *closure)
{
	struct client_describe *desc, *d;
	struct writebuf wb = { .count = 0 };

	desc = malloc(sizeof *desc);
	if (!desc) {
		errno = ENOMEM;
		goto error;
	}

	/* fill in stack the description of the task */
	pthread_mutex_lock(&protows->mutex);
	desc->descid = ptr2id(desc);
	d = protows->describes;
	while (d) {
		if (d->descid != desc->descid)
			d = d->next;
		else {
			desc->descid++;
			d = protows->describes;
		}
	}
	desc->callback = callback;
	desc->closure = closure;
	desc->protows = protows;
	desc->next = protows->describes;
	protows->describes = desc;
	pthread_mutex_unlock(&protows->mutex);

	/* send */
	if (writebuf_char(&wb, CHAR_FOR_DESCRIBE)
	 && writebuf_uint32(&wb, desc->descid)
	 && afb_ws_binary_v(protows->ws, wb.iovec, wb.count) >= 0)
		return 0;

	pthread_mutex_lock(&protows->mutex);
	d = protows->describes;
	if (d == desc)
		protows->describes = desc->next;
	else {
		while(d && d->next != desc)
			d = d->next;
		if (d)
			d->next = desc->next;
	}
	free(desc);
	pthread_mutex_unlock(&protows->mutex);
error:
	/* TODO? callback(closure, NULL); */
	return -1;
}

/******************* client description part for server *****************************/

/* on call, propagate it to the ws service */
static void server_on_call(struct afb_proto_ws *protows, struct readbuf *rb)
{
	struct afb_proto_ws_call *call;
	const char *uuid, *verb;
	uint32_t callid;
	size_t lenverb;
	struct json_object *object;

	afb_proto_ws_addref(protows);

	/* reads the call message data */
	if (!readbuf_uint32(rb, &callid)
	 || !readbuf_string(rb, &verb, &lenverb)
	 || !readbuf_string(rb, &uuid, NULL)
	 || !readbuf_object(rb, &object))
		goto overflow;

	/* create the request */
	call = malloc(sizeof *call);
	if (call == NULL)
		goto out_of_memory;

	call->protows = protows;
	call->callid = callid;
	call->refcount = 1;
	call->buffer = rb->base;
	rb->base = NULL; /* don't free the buffer */

	protows->server_itf->on_call(protows->closure, call, verb, object, uuid);
	return;

out_of_memory:
	json_object_put(object);

overflow:
	afb_proto_ws_unref(protows);
}

/* on subcall reply */
static void server_on_subcall_reply(struct afb_proto_ws *protows, struct readbuf *rb)
{
	char ie;
	uint32_t subcallid;
	struct json_object *object;
	struct server_subcall *sc, **psc;

	/* reads the call message data */
	if (!readbuf_uint32(rb, &subcallid)
	 || !readbuf_char(rb, &ie)
	 || !readbuf_object(rb, &object)) {
		/* TODO bad protocol */
		return;
	}

	/* search the subcall and unlink it */
	pthread_mutex_lock(&protows->mutex);
	psc = &protows->subcalls;
	while ((sc = *psc) && sc->subcallid != subcallid)
		psc = &sc->next;
	if (!sc) {
		pthread_mutex_unlock(&protows->mutex);
		json_object_put(object);
		/* TODO subcall not found */
	} else {
		*psc = sc->next;
		pthread_mutex_unlock(&protows->mutex);
		sc->callback(sc->closure, -(int)ie, object);
		free(sc);
	}
}

static int server_send_description(struct afb_proto_ws *protows, uint32_t descid, struct json_object *descobj)
{
	struct writebuf wb = { .count = 0 };

	return -!(writebuf_char(&wb, CHAR_FOR_DESCRIPTION)
		 && writebuf_uint32(&wb, descid)
		 && writebuf_object(&wb, descobj)
	         && afb_ws_binary_v(protows->ws, wb.iovec, wb.count) >= 0);
}

int afb_proto_ws_describe_put(struct afb_proto_ws_describe *describe, struct json_object *description)
{
	int rc = server_send_description(describe->protows, describe->descid, description);
	afb_proto_ws_addref(describe->protows);
	free(describe);
	return rc;
}

/* on describe, propagate it to the ws service */
static void server_on_describe(struct afb_proto_ws *protows, struct readbuf *rb)
{
	uint32_t descid;
	struct afb_proto_ws_describe *desc;

	/* reads the descid */
	if (readbuf_uint32(rb, &descid)) {
		if (protows->server_itf->on_describe) {
			/* create asynchronous job */
			desc = malloc(sizeof *desc);
			if (desc) {
				desc->descid = descid;
				desc->protows = protows;
				afb_proto_ws_addref(protows);
				protows->server_itf->on_describe(protows->closure, desc);
				return;
			}
		}
		server_send_description(protows, descid, NULL);
	}
}

/* callback when receiving binary data */
static void server_on_binary(void *closure, char *data, size_t size)
{
	struct afb_proto_ws *protows;
	struct readbuf rb;

	rb.base = data;
	if (size > 0) {
		rb.head = data;
		rb.end = data + size;
		protows = closure;

		switch (*rb.head++) {
		case CHAR_FOR_CALL:
			server_on_call(protows, &rb);
			break;
		case CHAR_FOR_SUBCALL_REPLY:
			server_on_subcall_reply(protows, &rb);
			break;
		case CHAR_FOR_DESCRIBE:
			server_on_describe(protows, &rb);
			break;
		default: /* unexpected message */
			/* TODO: close the connection */
			break;
		}
	}
	free(rb.base);
}

/******************* server part: manage events **********************************/

static int server_event_send(struct afb_proto_ws *protows, char order, const char *event_name, int event_id, struct json_object *data)
{
	struct writebuf wb = { .count = 0 };

	return -!(writebuf_char(&wb, order)
		 && (order == CHAR_FOR_EVT_BROADCAST || writebuf_uint32(&wb, event_id))
		 && writebuf_string(&wb, event_name)
		 && (order == CHAR_FOR_EVT_ADD || order == CHAR_FOR_EVT_DEL || writebuf_object(&wb, data))
		 && afb_ws_binary_v(protows->ws, wb.iovec, wb.count) >= 0);
}

int afb_proto_ws_server_event_create(struct afb_proto_ws *protows, const char *event_name, int event_id)
{
	return server_event_send(protows, CHAR_FOR_EVT_ADD, event_name, event_id, NULL);
}

int afb_proto_ws_server_event_remove(struct afb_proto_ws *protows, const char *event_name, int event_id)
{
	return server_event_send(protows, CHAR_FOR_EVT_DEL, event_name, event_id, NULL);
}

int afb_proto_ws_server_event_push(struct afb_proto_ws *protows, const char *event_name, int event_id, struct json_object *data)
{
	return server_event_send(protows, CHAR_FOR_EVT_PUSH, event_name, event_id, data);
}

int afb_proto_ws_server_event_broadcast(struct afb_proto_ws *protows, const char *event_name, struct json_object *data)
{
	return server_event_send(protows, CHAR_FOR_EVT_BROADCAST, event_name, 0, data);
}

/*****************************************************/

/* callback when receiving a hangup */
static void on_hangup(void *closure)
{
	struct afb_proto_ws *protows = closure;
	struct server_subcall *sc, *nsc;
	struct client_describe *cd, *ncd;

	nsc = protows->subcalls;
	while (nsc) {
		sc= nsc;
		nsc = sc->next;
		sc->callback(sc->closure, 1, NULL);
		free(sc);
	}

	ncd = protows->describes;
	while (ncd) {
		cd= ncd;
		ncd = cd->next;
		cd->callback(cd->closure, NULL);
		free(cd);
	}

	if (protows->fd >= 0) {
		protows->fd = -1;
		if (protows->on_hangup)
			protows->on_hangup(protows->closure);
	}
}

/*****************************************************/

static const struct afb_ws_itf proto_ws_client_ws_itf =
{
	.on_close = NULL,
	.on_text = NULL,
	.on_binary = client_on_binary,
	.on_error = NULL,
	.on_hangup = on_hangup
};

static const struct afb_ws_itf server_ws_itf =
{
	.on_close = NULL,
	.on_text = NULL,
	.on_binary = server_on_binary,
	.on_error = NULL,
	.on_hangup = on_hangup
};

/*****************************************************/

static struct afb_proto_ws *afb_proto_ws_create(int fd, const struct afb_proto_ws_server_itf *itfs, const struct afb_proto_ws_client_itf *itfc, void *closure, const struct afb_ws_itf *itf)
{
	struct afb_proto_ws *protows;

	protows = calloc(1, sizeof *protows);
	if (protows == NULL)
		errno = ENOMEM;
	else {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
		fcntl(fd, F_SETFL, O_NONBLOCK);
		protows->ws = afb_ws_create(afb_common_get_event_loop(), fd, itf, protows);
		if (protows->ws != NULL) {
			protows->fd = fd;
			protows->refcount = 1;
			protows->subcalls = NULL;
			protows->closure = closure;
			protows->server_itf = itfs;
			protows->client_itf = itfc;
			pthread_mutex_init(&protows->mutex, NULL);
			return protows;
		}
		free(protows);
	}
	return NULL;
}

struct afb_proto_ws *afb_proto_ws_create_client(int fd, const struct afb_proto_ws_client_itf *itf, void *closure)
{
	return afb_proto_ws_create(fd, NULL, itf, closure, &proto_ws_client_ws_itf);
}

struct afb_proto_ws *afb_proto_ws_create_server(int fd, const struct afb_proto_ws_server_itf *itf, void *closure)
{
	return afb_proto_ws_create(fd, itf, NULL, closure, &server_ws_itf);
}

void afb_proto_ws_unref(struct afb_proto_ws *protows)
{
	if (!__atomic_sub_fetch(&protows->refcount, 1, __ATOMIC_RELAXED)) {
		afb_proto_ws_hangup(protows);
		afb_ws_destroy(protows->ws);
		pthread_mutex_destroy(&protows->mutex);
		free(protows);
	}
}

void afb_proto_ws_addref(struct afb_proto_ws *protows)
{
	__atomic_add_fetch(&protows->refcount, 1, __ATOMIC_RELAXED);
}

int afb_proto_ws_is_client(struct afb_proto_ws *protows)
{
	return !!protows->client_itf;
}

int afb_proto_ws_is_server(struct afb_proto_ws *protows)
{
	return !!protows->server_itf;
}

void afb_proto_ws_hangup(struct afb_proto_ws *protows)
{
	afb_ws_hangup(protows->ws);
}

void afb_proto_ws_on_hangup(struct afb_proto_ws *protows, void (*on_hangup)(void *closure))
{
	protows->on_hangup = on_hangup;
}

