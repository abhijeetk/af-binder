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

#include <afb/afb-event-itf.h>

#include "afb-common.h"

#include "afb-session.h"
#include "afb-cred.h"
#include "afb-ws.h"
#include "afb-msg-json.h"
#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-stub-ws.h"
#include "afb-context.h"
#include "afb-evt.h"
#include "afb-xreq.h"
#include "verbose.h"
#include "jobs.h"

struct afb_stub_ws;

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
struct client_subcall
{
	struct afb_stub_ws *stubws;	/**< stub descriptor */
	uint32_t subcallid;		/**< subcallid for the reply */
};

/*
 * structure for recording calls on client side
 */
struct client_call {
	struct client_call *next;	/* the next call */
	struct afb_stub_ws *stubws;	/* the stub_ws */
	struct afb_xreq *xreq;		/* the request handle */
	uint32_t msgid;			/* the message identifier */
};

/*
 * structure for a ws request
 */
struct server_req {
	struct afb_xreq xreq;		/* the xreq */
	struct afb_stub_ws *stubws;	/* the client of the request */
	uint32_t msgid;			/* the incoming request msgid */
};

/*
 * structure for recording events on client side
 */
struct client_event
{
	struct client_event *next;
	struct afb_event event;
	int eventid;
	int refcount;
};

/*
 * structure for recording describe requests
 */
struct client_describe
{
	struct client_describe *next;
	struct afb_stub_ws *stubws;
	struct jobloop *jobloop;
	struct json_object *result;
	uint32_t descid;
};

/*
 * structure for jobs of describing
 */
struct server_describe
{
	struct afb_stub_ws *stubws;
	uint32_t descid;
};

/******************* client description part for server *****************************/

struct afb_stub_ws
{
	/* count of references */
	int refcount;

	/* file descriptor */
	int fd;

	/* resource control */
	pthread_mutex_t mutex;

	/* websocket */
	struct afb_ws *ws;

	/* listener for events (server side) */
	struct afb_evt_listener *listener;

	/* event replica (client side) */
	struct client_event *events;

	/* emitted calls (client side) */
	struct client_call *calls;

	/* credentials (server side) */
	struct afb_cred *cred;

	/* pending subcalls (server side) */
	struct server_subcall *subcalls;

	/* pending description (client side) */
	struct client_describe *describes;

	/* apiset */
	struct afb_apiset *apiset;

	/* the api name */
	char apiname[1];
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
	char *head, *end;
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

/* decrement the reference count of the request and free/release it on falling to null */
static void server_req_destroy_cb(struct afb_xreq *xreq)
{
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	afb_context_disconnect(&wreq->xreq.context);
	afb_cred_unref(wreq->xreq.cred);
	json_object_put(wreq->xreq.json);
	afb_stub_ws_unref(wreq->stubws);
	free(wreq);
}

static void server_req_success_cb(struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	int rc;
	struct writebuf wb = { .count = 0 };
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	if (writebuf_char(&wb, CHAR_FOR_ANSWER_SUCCESS)
	 && writebuf_uint32(&wb, wreq->msgid)
	 && writebuf_uint32(&wb, (uint32_t)wreq->xreq.context.flags)
	 && writebuf_string(&wb, info ? : "")
	 && writebuf_object(&wb, obj)) {
		rc = afb_ws_binary_v(wreq->stubws->ws, wb.iovec, wb.count);
		if (rc >= 0)
			goto success;
	}
	ERROR("error while sending success");
success:
	json_object_put(obj);
}

static void server_req_fail_cb(struct afb_xreq *xreq, const char *status, const char *info)
{
	int rc;
	struct writebuf wb = { .count = 0 };
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	if (writebuf_char(&wb, CHAR_FOR_ANSWER_FAIL)
	 && writebuf_uint32(&wb, wreq->msgid)
	 && writebuf_uint32(&wb, (uint32_t)wreq->xreq.context.flags)
	 && writebuf_string(&wb, status)
	 && writebuf_string(&wb, info ? : "")) {
		rc = afb_ws_binary_v(wreq->stubws->ws, wb.iovec, wb.count);
		if (rc >= 0)
			return;
	}
	ERROR("error while sending fail");
}

static void server_req_subcall_cb(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	int rc;
	struct writebuf wb = { .count = 0 };
	struct server_subcall *sc, *osc;
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);
	struct afb_stub_ws *stubws = wreq->stubws;

	sc = malloc(sizeof *sc);
	if (!sc) {
		callback(cb_closure, 1, afb_msg_json_internal_error());
	} else {
		sc->callback = callback;
		sc->closure = cb_closure;

		pthread_mutex_unlock(&stubws->mutex);
		sc->subcallid = ptr2id(sc);
		do {
			sc->subcallid++;
			osc = stubws->subcalls;
			while(osc && osc->subcallid != sc->subcallid)
				osc = osc->next;
		} while (osc);
		sc->next = stubws->subcalls;
		stubws->subcalls = sc;
		pthread_mutex_unlock(&stubws->mutex);

		if (writebuf_char(&wb, CHAR_FOR_SUBCALL_CALL)
		 && writebuf_uint32(&wb, wreq->msgid)
		 && writebuf_uint32(&wb, sc->subcallid)
		 && writebuf_string(&wb, api)
		 && writebuf_string(&wb, verb)
		 && writebuf_object(&wb, args)) {
			rc = afb_ws_binary_v(wreq->stubws->ws, wb.iovec, wb.count);
			if (rc >= 0)
				return;
		}
		ERROR("error while sending fail");
	}
}

static int server_req_subscribe_cb(struct afb_xreq *xreq, struct afb_event event)
{
	int rc, rc2;
	struct writebuf wb = { .count = 0 };
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	rc = afb_evt_add_watch(wreq->stubws->listener, event);
	if (rc < 0)
		return rc;

	if (writebuf_char(&wb, CHAR_FOR_EVT_SUBSCRIBE)
	 && writebuf_uint32(&wb, wreq->msgid)
	 && writebuf_uint32(&wb, (uint32_t)afb_evt_event_id(event))
	 && writebuf_string(&wb, afb_evt_event_name(event))) {
		rc2 = afb_ws_binary_v(wreq->stubws->ws, wb.iovec, wb.count);
		if (rc2 >= 0)
			goto success;
	}
	ERROR("error while subscribing event");
success:
	return rc;
}

static int server_req_unsubscribe_cb(struct afb_xreq *xreq, struct afb_event event)
{
	int rc, rc2;
	struct writebuf wb = { .count = 0 };
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	if (writebuf_char(&wb, CHAR_FOR_EVT_UNSUBSCRIBE)
	 && writebuf_uint32(&wb, wreq->msgid)
	 && writebuf_uint32(&wb, (uint32_t)afb_evt_event_id(event))
	 && writebuf_string(&wb, afb_evt_event_name(event))) {
		rc2 = afb_ws_binary_v(wreq->stubws->ws, wb.iovec, wb.count);
		if (rc2 >= 0)
			goto success;
	}
	ERROR("error while subscribing event");
success:
	rc = afb_evt_remove_watch(wreq->stubws->listener, event);
	return rc;
}

static const struct afb_xreq_query_itf server_req_xreq_itf = {
	.success = server_req_success_cb,
	.fail = server_req_fail_cb,
	.unref = server_req_destroy_cb,
	.subcall = server_req_subcall_cb,
	.subscribe = server_req_subscribe_cb,
	.unsubscribe = server_req_unsubscribe_cb
};

/******************* client part **********************************/

/* search a memorized call */
static struct client_call *client_call_search(struct afb_stub_ws *stubws, uint32_t msgid)
{
	struct client_call *call;

	call = stubws->calls;
	while (call != NULL && call->msgid != msgid)
		call = call->next;

	return call;
}

/* search the event */
static struct client_event *client_event_search(struct afb_stub_ws *stubws, uint32_t eventid, const char *name)
{
	struct client_event *ev;

	ev = stubws->events;
	while (ev != NULL && (ev->eventid != eventid || 0 != strcmp(afb_evt_event_name(ev->event), name)))
		ev = ev->next;

	return ev;
}


/* allocates and init the memorizing call */
static struct client_call *client_call_make(struct afb_stub_ws *stubws, struct afb_xreq *xreq)
{
	struct client_call *call;

	call = malloc(sizeof *call);
	if (call != NULL) {
		afb_xreq_addref(xreq);
		call->xreq = xreq;
		call->msgid = ptr2id(call);
		while(client_call_search(stubws, call->msgid) != NULL)
			call->msgid++;
		call->stubws = stubws;
		call->next = stubws->calls;
		stubws->calls = call;
	}
	return call;
}

/* free and release the memorizing call */
static void client_call_destroy(struct client_call *call)
{
	struct client_call **prv;

	prv = &call->stubws->calls;
	while (*prv != NULL) {
		if (*prv == call) {
			*prv = call->next;
			break;
		}
		prv = &(*prv)->next;
	}

	afb_xreq_unref(call->xreq);
	free(call);
}

/* get event data from the message */
static int client_msg_event_read(struct readbuf *rb, uint32_t *eventid, const char **name)
{
	return readbuf_uint32(rb, eventid) && readbuf_string(rb, name, NULL);
}

/* get event from the message */
static int client_msg_event_get(struct afb_stub_ws *stubws, struct readbuf *rb, struct client_event **ev)
{
	const char *name;
	uint32_t eventid;

	/* get event data from the message */
	if (!client_msg_event_read(rb, &eventid, &name)) {
		ERROR("Invalid message");
		return 0;
	}

	/* check conflicts */
	*ev = client_event_search(stubws, eventid, name);
	if (*ev == NULL) {
		ERROR("event %s not found", name);
		return 0;
	}

	return 1;
}

/* get event from the message */
static int client_msg_call_get(struct afb_stub_ws *stubws, struct readbuf *rb, struct client_call **call)
{
	uint32_t msgid;

	/* get event data from the message */
	if (!readbuf_uint32(rb, &msgid)) {
		ERROR("Invalid message");
		return 0;
	}

	/* get the call */
	*call = client_call_search(stubws, msgid);
	if (*call == NULL) {
		ERROR("message not found");
		return 0;
	}

	return 1;
}

/* read a subscrition message */
static int client_msg_subscription_get(struct afb_stub_ws *stubws, struct readbuf *rb, struct client_call **call, struct client_event **ev)
{
	return client_msg_call_get(stubws, rb, call) && client_msg_event_get(stubws, rb, ev);
}

/* adds an event */
static void client_event_create(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	size_t offset;
	const char *name;
	uint32_t eventid;
	struct client_event *ev;

	/* get event data from the message */
	offset = client_msg_event_read(rb, &eventid, &name);
	if (offset == 0) {
		ERROR("Invalid message");
		return;
	}

	/* check conflicts */
	ev = client_event_search(stubws, eventid, name);
	if (ev != NULL) {
		ev->refcount++;
		return;
	}

	/* no conflict, try to add it */
	ev = malloc(sizeof *ev);
	if (ev != NULL) {
		ev->event = afb_evt_create_event(name);
		if (ev->event.closure == NULL)
			free(ev);
		else {
			ev->refcount = 1;
			ev->eventid = eventid;
			ev->next = stubws->events;
			stubws->events = ev;
			return;
		}
	}
	ERROR("can't create event %s, out of memory", name);
}

/* removes an event */
static void client_event_drop(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct client_event *ev, **prv;

	/* retrieves the event */
	if (!client_msg_event_get(stubws, rb, &ev))
		return;

	/* decrease the reference count */
	if (--ev->refcount)
		return;

	/* unlinks the event */
	prv = &stubws->events;
	while (*prv != ev)
		prv = &(*prv)->next;
	*prv = ev->next;

	/* destroys the event */
	afb_event_drop(ev->event);
	free(ev);
}

/* subscribes an event */
static void client_event_subscribe(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct client_event *ev;
	struct client_call *call;

	if (client_msg_subscription_get(stubws, rb, &call, &ev)) {
		/* subscribe the request from the event */
		if (afb_xreq_subscribe(call->xreq, ev->event) < 0)
			ERROR("can't subscribe: %m");
	}
}

/* unsubscribes an event */
static void client_event_unsubscribe(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct client_event *ev;
	struct client_call *call;

	if (client_msg_subscription_get(stubws, rb, &call, &ev)) {
		/* unsubscribe the request from the event */
		if (afb_xreq_unsubscribe(call->xreq, ev->event) < 0)
			ERROR("can't unsubscribe: %m");
	}
}

/* receives broadcasted events */
static void client_event_broadcast(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct json_object *object;
	const char *event;

	if (readbuf_string(rb, &event, NULL) && readbuf_object(rb, &object))
		afb_evt_broadcast(event, object);
	else
		ERROR("unreadable broadcasted event");
}

/* pushs an event */
static void client_event_push(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct client_event *ev;
	struct json_object *object;

	if (client_msg_event_get(stubws, rb, &ev) && readbuf_object(rb, &object))
		afb_event_push(ev->event, object);
	else
		ERROR("unreadable push event");
}

static void client_reply_success(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct client_call *call;
	struct json_object *object;
	const char *info;
	uint32_t flags;

	/* retrieve the message data */
	if (!client_msg_call_get(stubws, rb, &call))
		return;

	if (readbuf_uint32(rb, &flags)
	 && readbuf_string(rb, &info, NULL)
	 && readbuf_object(rb, &object)) {
		call->xreq->context.flags = (unsigned)flags;
		afb_xreq_success(call->xreq, object, *info ? info : NULL);
	} else {
		/* failing to have the answer */
		afb_xreq_fail(call->xreq, "error", "ws error");
	}
	client_call_destroy(call);
}

static void client_reply_fail(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct client_call *call;
	const char *info, *status;
	uint32_t flags;

	/* retrieve the message data */
	if (!client_msg_call_get(stubws, rb, &call))
		return;

	if (readbuf_uint32(rb, &flags)
	 && readbuf_string(rb, &status, NULL)
	 && readbuf_string(rb, &info, NULL)) {
		call->xreq->context.flags = (unsigned)flags;
		afb_xreq_fail(call->xreq, status, *info ? info : NULL);
	} else {
		/* failing to have the answer */
		afb_xreq_fail(call->xreq, "error", "ws error");
	}
	client_call_destroy(call);
}

/* send a subcall reply */
static void client_send_subcall_reply(struct client_subcall *subcall, int status, json_object *object)
{
	int rc;
	struct writebuf wb = { .count = 0 };
	char ie = status < 0;

	if (!writebuf_char(&wb, CHAR_FOR_SUBCALL_REPLY)
	 || !writebuf_uint32(&wb, subcall->subcallid)
	 || !writebuf_char(&wb, ie)
	 || !writebuf_object(&wb, object)) {
		/* write error ? */
		return;
	}

	rc = afb_ws_binary_v(subcall->stubws->ws, wb.iovec, wb.count);
	if (rc >= 0)
		return;
	ERROR("error while sending subcall reply");
}

/* callback for subcall reply */
static void client_subcall_reply_cb(void *closure, int status, json_object *object)
{
	client_send_subcall_reply(closure, status, object);
	free(closure);
}

/* received a subcall request */
static void client_subcall(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct client_subcall *subcall;
	struct client_call *call;
	const char *api, *verb;
	uint32_t subcallid;
	struct json_object *object;

	subcall = malloc(sizeof *subcall);
	if (!subcall)
		return;

	/* retrieve the message data */
	if (!client_msg_call_get(stubws, rb, &call))
		return;

	if (readbuf_uint32(rb, &subcallid)
	 && readbuf_string(rb, &api, NULL)
	 && readbuf_string(rb, &verb, NULL)
	 && readbuf_object(rb, &object)) {
		subcall->stubws = stubws;
		subcall->subcallid = subcallid;
		afb_xreq_subcall(call->xreq, api, verb, object, client_subcall_reply_cb, subcall);
	}
}

/* pushs an event */
static void client_on_description(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	uint32_t descid;
	struct client_describe *desc;
	struct json_object *object;

	if (!readbuf_uint32(rb, &descid))
		ERROR("unreadable description");
	else {
		desc = stubws->describes;
		while (desc && desc->descid != descid)
			desc = desc->next;
		if (desc == NULL)
			ERROR("unexpected description");
		else {
			if (readbuf_object(rb, &object))
				desc->result = object;
			else
				ERROR("bad description");
			jobs_leave(desc->jobloop);
		}
	}
}

/* callback when receiving binary data */
static void client_on_binary(void *closure, char *data, size_t size)
{
	if (size > 0) {
		struct afb_stub_ws *stubws = closure;
		struct readbuf rb = { .head = data, .end = data + size };

		pthread_mutex_lock(&stubws->mutex);
		switch (*rb.head++) {
		case CHAR_FOR_ANSWER_SUCCESS: /* success */
			client_reply_success(stubws, &rb);
			break;
		case CHAR_FOR_ANSWER_FAIL: /* fail */
			client_reply_fail(stubws, &rb);
			break;
		case CHAR_FOR_EVT_BROADCAST: /* broadcast */
			client_event_broadcast(stubws, &rb);
			break;
		case CHAR_FOR_EVT_ADD: /* creates the event */
			client_event_create(stubws, &rb);
			break;
		case CHAR_FOR_EVT_DEL: /* drops the event */
			client_event_drop(stubws, &rb);
			break;
		case CHAR_FOR_EVT_PUSH: /* pushs the event */
			client_event_push(stubws, &rb);
			break;
		case CHAR_FOR_EVT_SUBSCRIBE: /* subscribe event for a request */
			client_event_subscribe(stubws, &rb);
			break;
		case CHAR_FOR_EVT_UNSUBSCRIBE: /* unsubscribe event for a request */
			client_event_unsubscribe(stubws, &rb);
			break;
		case CHAR_FOR_SUBCALL_CALL: /* subcall */
			client_subcall(stubws, &rb);
			break;
		case CHAR_FOR_DESCRIPTION: /* description */
			client_on_description(stubws, &rb);
			break;
		default: /* unexpected message */
			/* TODO: close the connection */
			break;
		}
		pthread_mutex_unlock(&stubws->mutex);
	}
	free(data);
}

/* on call, propagate it to the ws service */
static void client_call_cb(void * closure, struct afb_xreq *xreq)
{
	int rc;
	struct client_call *call;
	struct writebuf wb = { .count = 0 };
	const char *raw;
	size_t szraw;
	struct afb_stub_ws *stubws = closure;

	pthread_mutex_lock(&stubws->mutex);

	/* create the recording data */
	call = client_call_make(stubws, xreq);
	if (call == NULL) {
		afb_xreq_fail_f(xreq, "error", "out of memory");
		goto end;
	}

	/* creates the call message */
	raw = afb_xreq_raw(xreq, &szraw);
	if (raw == NULL)
		goto internal_error;
	if (!writebuf_char(&wb, CHAR_FOR_CALL)
	 || !writebuf_uint32(&wb, call->msgid)
	 || !writebuf_uint32(&wb, (uint32_t)xreq->context.flags)
	 || !writebuf_string(&wb, xreq->verb)
	 || !writebuf_string(&wb, afb_session_uuid(xreq->context.session))
	 || !writebuf_string_length(&wb, raw, szraw))
		goto overflow;

	/* send */
	rc = afb_ws_binary_v(stubws->ws, wb.iovec, wb.count);
	if (rc >= 0)
		goto end;

	afb_xreq_fail(xreq, "error", "websocket sending error");
	goto clean_call;

internal_error:
	afb_xreq_fail(xreq, "error", "internal: raw is NULL!");
	goto clean_call;

overflow:
	afb_xreq_fail(xreq, "error", "overflow: size doesn't match 32 bits!");

clean_call:
	client_call_destroy(call);
end:
	pthread_mutex_unlock(&stubws->mutex);
}

static void client_send_describe_cb(int signum, void *closure, struct jobloop *jobloop)
{
	struct client_describe *desc = closure;
	struct writebuf wb = { .count = 0 };

	if (!signum) {
		/* record the jobloop */
		desc->jobloop = jobloop;

		/* send */
		if (writebuf_char(&wb, CHAR_FOR_DESCRIBE)
		 && writebuf_uint32(&wb, desc->descid)
		 && afb_ws_binary_v(desc->stubws->ws, wb.iovec, wb.count) >= 0)
			return;
	}
	jobs_leave(jobloop);
}

/* get the description */
static struct json_object *client_describe_cb(void * closure)
{
	struct client_describe desc, *d;
	struct afb_stub_ws *stubws = closure;

	/* fill in stack the description of the task */
	pthread_mutex_lock(&stubws->mutex);
	desc.result = NULL;
	desc.descid = ptr2id(&desc);
	d = stubws->describes;
	while (d) {
		if (d->descid != desc.descid)
			d = d->next;
		else {
			desc.descid++;
			d = stubws->describes;
		}
	}
	desc.stubws = stubws;
	desc.next = stubws->describes;
	stubws->describes = &desc;
	pthread_mutex_unlock(&stubws->mutex);

	/* synchronous job: send the request and wait its result */
	jobs_enter(NULL, 0, client_send_describe_cb, &desc);

	/* unlink and send the result */
	pthread_mutex_lock(&stubws->mutex);
	d = stubws->describes;
	if (d == &desc)
		stubws->describes = desc.next;
	else {
		while (d) {
			if (d->next != &desc)
				d = d->next;
			else {
				d->next = desc.next;
				d = NULL;
			}
		}
	}
	pthread_mutex_unlock(&stubws->mutex);
	return desc.result;
}

/******************* client description part for server *****************************/

/* on call, propagate it to the ws service */
static void server_on_call(struct afb_stub_ws *stubws, struct readbuf *rb)
{
	struct server_req *wreq;
	char *cverb;
	const char *uuid, *verb;
	uint32_t flags, msgid;
	size_t lenverb;
	struct json_object *object;

	afb_stub_ws_addref(stubws);

	/* reads the call message data */
	if (!readbuf_uint32(rb, &msgid)
	 || !readbuf_uint32(rb, &flags)
	 || !readbuf_string(rb, &verb, &lenverb)
	 || !readbuf_string(rb, &uuid, NULL)
	 || !readbuf_object(rb, &object))
		goto overflow;

	/* create the request */
	wreq = malloc(++lenverb + sizeof *wreq);
	if (wreq == NULL)
		goto out_of_memory;

	afb_xreq_init(&wreq->xreq, &server_req_xreq_itf);
	wreq->stubws = stubws;
	wreq->msgid = msgid;
	cverb = (char*)&wreq[1];
	memcpy(cverb, verb, lenverb);

	/* init the context */
	if (afb_context_connect(&wreq->xreq.context, uuid, NULL) < 0)
		goto unconnected;
	wreq->xreq.context.flags = flags;

	/* makes the call */
	wreq->xreq.cred = afb_cred_addref(stubws->cred);
	wreq->xreq.api = stubws->apiname;
	wreq->xreq.verb = cverb;
	wreq->xreq.json = object;
	afb_xreq_process(&wreq->xreq, stubws->apiset);
	return;

unconnected:
	free(wreq);
out_of_memory:
	json_object_put(object);
overflow:
	afb_stub_ws_unref(stubws);
}

/* on subcall reply */
static void server_on_subcall_reply(struct afb_stub_ws *stubws, struct readbuf *rb)
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
	pthread_mutex_lock(&stubws->mutex);
	psc = &stubws->subcalls;
	while ((sc = *psc) && sc->subcallid != subcallid)
		psc = &sc->next;
	if (!sc) {
		pthread_mutex_unlock(&stubws->mutex);
		/* TODO subcall not found */
	} else {
		*psc = sc->next;
		pthread_mutex_unlock(&stubws->mutex);
		sc->callback(sc->closure, -(int)ie, object);
		free(sc);
	}
	json_object_put(object);
}

static void server_send_description(struct afb_stub_ws *stubws, uint32_t descid, struct json_object *descobj)
{
	struct writebuf wb = { .count = 0 };

	if (!writebuf_char(&wb, CHAR_FOR_DESCRIPTION)
	 || !writebuf_uint32(&wb, descid)
	 || !writebuf_object(&wb, descobj)
         || afb_ws_binary_v(stubws->ws, wb.iovec, wb.count) < 0)
		ERROR("can't send description");
}

static void server_describe_job(int signum, void *closure)
{
	struct json_object *obj;
	struct server_describe *desc = closure;

	/* get the description if possible */
	obj = !signum ? afb_apiset_describe(desc->stubws->apiset, desc->stubws->apiname) : NULL;

	/* send it */
	server_send_description(desc->stubws, desc->descid, obj);
	json_object_put(obj);
	afb_stub_ws_unref(desc->stubws);
	free(desc);
}

/* on describe, propagate it to the ws service */
static void server_on_describe(struct afb_stub_ws *stubws, struct readbuf *rb)
{

	uint32_t descid;
	struct server_describe *desc;

	/* reads the descid */
	if (readbuf_uint32(rb, &descid)) {
		/* create asynchronous job */
		desc = malloc(sizeof *desc);
		if (desc) {
			desc->descid = descid;
			desc->stubws = stubws;
			afb_stub_ws_addref(stubws);
			if (jobs_queue(NULL, 0, server_describe_job, desc) < 0)
				server_describe_job(0, desc);
			return;
		}
		server_send_description(stubws, descid, NULL);
	}
	ERROR("can't provide description");
}

/* callback when receiving binary data */
static void server_on_binary(void *closure, char *data, size_t size)
{
	if (size > 0) {
		struct readbuf rb = { .head = data, .end = data + size };
		switch (*rb.head++) {
		case CHAR_FOR_CALL:
			server_on_call(closure, &rb);
			break;
		case CHAR_FOR_SUBCALL_REPLY:
			server_on_subcall_reply(closure, &rb);
			break;
		case CHAR_FOR_DESCRIBE:
			server_on_describe(closure, &rb);
			break;
		default: /* unexpected message */
			/* TODO: close the connection */
			break;
		}
	}
	free(data);
}

/******************* server part: manage events **********************************/

static void server_event_send(struct afb_stub_ws *stubws, char order, const char *event, int eventid, const char *data)
{
	int rc;
	struct writebuf wb = { .count = 0 };

	if (writebuf_char(&wb, order)
	 && writebuf_uint32(&wb, eventid)
	 && writebuf_string(&wb, event)
	 && (data == NULL || writebuf_string(&wb, data))) {
		rc = afb_ws_binary_v(stubws->ws, wb.iovec, wb.count);
		if (rc >= 0)
			return;
	}
	ERROR("error while sending %c for event %s", order, event);
}

static void server_event_add(void *closure, const char *event, int eventid)
{
	server_event_send(closure, CHAR_FOR_EVT_ADD, event, eventid, NULL);
}

static void server_event_remove(void *closure, const char *event, int eventid)
{
	server_event_send(closure, CHAR_FOR_EVT_DEL, event, eventid, NULL);
}

static void server_event_push(void *closure, const char *event, int eventid, struct json_object *object)
{
	const char *data = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
	server_event_send(closure, CHAR_FOR_EVT_PUSH, event, eventid, data ? : "null");
	json_object_put(object);
}

static void server_event_broadcast(void *closure, const char *event, int eventid, struct json_object *object)
{
	int rc;
	struct afb_stub_ws *stubws = closure;

	struct writebuf wb = { .count = 0 };

	if (writebuf_char(&wb, CHAR_FOR_EVT_BROADCAST) && writebuf_string(&wb, event) && writebuf_object(&wb, object)) {
		rc = afb_ws_binary_v(stubws->ws, wb.iovec, wb.count);
		if (rc < 0)
			ERROR("error while broadcasting event %s", event);
	} else
		ERROR("error while broadcasting event %s", event);
	json_object_put(object);
}

/*****************************************************/

/* callback when receiving a hangup */
static void server_on_hangup(void *closure)
{
	struct afb_stub_ws *stubws = closure;

	/* close the socket */
	if (stubws->fd >= 0) {
		close(stubws->fd);
		stubws->fd = -1;
	}

	/* release the client */
	afb_stub_ws_unref(stubws);
}

/*****************************************************/

/* the interface for events pushing */
static const struct afb_evt_itf server_evt_itf = {
	.broadcast = server_event_broadcast,
	.push = server_event_push,
	.add = server_event_add,
	.remove = server_event_remove
};

static const struct afb_ws_itf stub_ws_client_ws_itf =
{
	.on_close = NULL,
	.on_text = NULL,
	.on_binary = client_on_binary,
	.on_error = NULL,
	.on_hangup = NULL
};

static const struct afb_ws_itf server_ws_itf =
{
	.on_close = NULL,
	.on_text = NULL,
	.on_binary = server_on_binary,
	.on_error = NULL,
	.on_hangup = server_on_hangup
};

static struct afb_api_itf ws_api_itf = {
	.call = client_call_cb,
	.describe = client_describe_cb
};

/*****************************************************/

static struct afb_stub_ws *afb_stub_ws_create(int fd, const char *apiname, struct afb_apiset *apiset, const struct afb_ws_itf *itf)
{
	struct afb_stub_ws *stubws;

	stubws = calloc(1, sizeof *stubws + strlen(apiname));
	if (stubws == NULL)
		errno = ENOMEM;
	else {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
		fcntl(fd, F_SETFL, O_NONBLOCK);
		stubws->ws = afb_ws_create(afb_common_get_event_loop(), fd, itf, stubws);
		if (stubws->ws != NULL) {
			stubws->fd = fd;
			strcpy(stubws->apiname, apiname);
			stubws->apiset = afb_apiset_addref(apiset);
			stubws->refcount = 1;
			stubws->subcalls = NULL;
			return stubws;
		}
		free(stubws);
	}
	return NULL;
}

struct afb_stub_ws *afb_stub_ws_create_client(int fd, const char *apiname, struct afb_apiset *apiset)
{
	struct afb_api afb_api;
	struct afb_stub_ws *stubws;

	stubws = afb_stub_ws_create(fd, apiname, apiset, &stub_ws_client_ws_itf);
	if (stubws) {
		afb_api.closure = stubws;
		afb_api.itf = &ws_api_itf;
		if (afb_apiset_add(apiset, stubws->apiname, afb_api) >= 0)
			return stubws;
		afb_stub_ws_unref(stubws);
	}
	return NULL;

}

struct afb_stub_ws *afb_stub_ws_create_server(int fd, const char *apiname, struct afb_apiset *apiset)
{
	struct afb_stub_ws *stubws;

	stubws = afb_stub_ws_create(fd, apiname, apiset, &server_ws_itf);
	if (stubws) {
		stubws->cred = afb_cred_create_for_socket(fd);
		stubws->listener = afb_evt_listener_create(&server_evt_itf, stubws);
		if (stubws->listener != NULL)
			return stubws;
		afb_stub_ws_unref(stubws);
	}
	return NULL;
}

void afb_stub_ws_unref(struct afb_stub_ws *stubws)
{
	struct server_subcall *sc, *nsc;

	if (!__atomic_sub_fetch(&stubws->refcount, 1, __ATOMIC_RELAXED)) {
		afb_evt_listener_unref(stubws->listener);
		afb_ws_destroy(stubws->ws);
		nsc = stubws->subcalls;
		while (nsc) {
			sc= nsc;
			nsc = sc->next;
			sc->callback(sc->closure, 1, NULL);
			free(sc);
		}
		afb_cred_unref(stubws->cred);
		afb_apiset_unref(stubws->apiset);
		free(stubws);
	}
}

void afb_stub_ws_addref(struct afb_stub_ws *stubws)
{
	__atomic_add_fetch(&stubws->refcount, 1, __ATOMIC_RELAXED);
}

