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

#include <afb/afb-req-itf.h>

#include "afb-common.h"

#include "afb-session.h"
#include "afb-cred.h"
#include "afb-ws.h"
#include "afb-msg-json.h"
#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-api-so.h"
#include "afb-context.h"
#include "afb-evt.h"
#include "afb-xreq.h"
#include "verbose.h"
#include "sd-fds.h"

struct api_ws_memo;
struct api_ws_event;
struct api_ws_client;

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

/*
 */
struct api_ws
{
	char *path;		/* path of the object for the API */
	char *api;		/* api name of the interface */
	int fd;			/* file descriptor */
	pthread_mutex_t mutex;	/**< resource control */
	union {
		struct {
			struct afb_ws *ws;
			struct api_ws_event *events;
			struct api_ws_memo *memos;
		} client;
		struct {
			sd_event_source *listensrc; /**< systemd source for server socket */
			struct afb_apiset *apiset;
		} server;
	};
};

#define RETOK   1
#define RETERR  2
#define RETRAW  3

/******************* common usefull tools **********************************/

/**
 * translate a pointer to some integer
 * @param ptr the pointer to translate
 * @return an integer
 */
static inline uint32_t ptr2id(void *ptr)
{
	return (uint32_t)(((intptr_t)ptr) >> 6);
}

/******************* websocket interface for client part **********************************/

static void api_ws_client_on_binary(void *closure, char *data, size_t size);

static const struct afb_ws_itf api_ws_client_ws_itf =
{
	.on_close = NULL,
	.on_text = NULL,
	.on_binary = api_ws_client_on_binary,
	.on_error = NULL,
	.on_hangup = NULL
};

/******************* event structures for server part **********************************/

static void api_ws_server_event_add(void *closure, const char *event, int eventid);
static void api_ws_server_event_remove(void *closure, const char *event, int eventid);
static void api_ws_server_event_push(void *closure, const char *event, int eventid, struct json_object *object);
static void api_ws_server_event_broadcast(void *closure, const char *event, int eventid, struct json_object *object);

/* the interface for events pushing */
static const struct afb_evt_itf api_ws_server_evt_itf = {
	.broadcast = api_ws_server_event_broadcast,
	.push = api_ws_server_event_push,
	.add = api_ws_server_event_add,
	.remove = api_ws_server_event_remove
};

/******************* handling subcalls *****************************/

/**
 * Structure on server side for recording pending
 * subcalls.
 */
struct api_ws_subcall
{
	struct api_ws_subcall *next;	/**< next subcall for the client */
	uint32_t subcallid;		/**< the subcallid */
	void (*callback)(void*, int, struct json_object*); /**< callback on completion */
	void *closure;			/**< closure of the callback */
};

/**
 * Structure for sending back replies on client side
 */
struct api_ws_reply
{
	struct api_ws *apiws;	/**< api descriptor */
	uint32_t subcallid;	/**< subcallid for the reply */
};

/******************* client description part for server *****************************/

struct api_ws_client
{
	/* the server ws-api */
	const char *api;

	/* count of references */
	int refcount;

	/* file descriptor */
	int fd;

	/* resource control */
	pthread_mutex_t mutex;

	/* listener for events */
	struct afb_evt_listener *listener;

	/* websocket */
	struct afb_ws *ws;

	/* credentials */
	struct afb_cred *cred;

	/* pending subcalls */
	struct api_ws_subcall *subcalls;

	/* apiset */
	struct afb_apiset *apiset;
};

/******************* websocket interface for client part **********************************/

static void api_ws_server_on_binary(void *closure, char *data, size_t size);
static void api_ws_server_on_hangup(void *closure);

static const struct afb_ws_itf api_ws_server_ws_itf =
{
	.on_close = NULL,
	.on_text = NULL,
	.on_binary = api_ws_server_on_binary,
	.on_error = NULL,
	.on_hangup = api_ws_server_on_hangup
};

/******************* ws request part for server *****************/

/*
 * structure for a ws request
 */
struct api_ws_server_req {
	struct afb_xreq xreq;		/* the xreq */
	struct api_ws_client *client;	/* the client of the request */
	uint32_t msgid;			/* the incoming request msgid */
};

static void api_ws_server_req_success_cb(struct afb_xreq *xreq, struct json_object *obj, const char *info);
static void api_ws_server_req_fail_cb(struct afb_xreq *xreq, const char *status, const char *info);
static void api_ws_server_req_destroy_cb(struct afb_xreq *xreq);
static int api_ws_server_req_subscribe_cb(struct afb_xreq *xreq, struct afb_event event);
static int api_ws_server_req_unsubscribe_cb(struct afb_xreq *xreq, struct afb_event event);
static void api_ws_server_req_subcall_cb(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure);

const struct afb_xreq_query_itf afb_api_ws_xreq_itf = {
	.success = api_ws_server_req_success_cb,
	.fail = api_ws_server_req_fail_cb,
	.unref = api_ws_server_req_destroy_cb,
	.subcall = api_ws_server_req_subcall_cb,
	.subscribe = api_ws_server_req_subscribe_cb,
	.unsubscribe = api_ws_server_req_unsubscribe_cb
};

/******************* common part **********************************/

/*
 * create a structure api_ws not connected to the 'path'.
 */
static struct api_ws *api_ws_make(const char *path)
{
	struct api_ws *api;
	size_t length;

	/* allocates the structure */
	length = strlen(path);
	api = calloc(1, sizeof *api + 1 + length);
	if (api == NULL) {
		errno = ENOMEM;
		goto error;
	}

	/* path is copied after the struct */
	api->path = (char*)(api+1);
	memcpy(api->path, path, length + 1);

	/* api name is at the end of the path */
	while (length && path[length - 1] != '/' && path[length - 1] != ':')
		length = length - 1;
	api->api = &api->path[length];
	if (api->api == NULL || !afb_api_is_valid_name(api->api)) {
		errno = EINVAL;
		goto error2;
	}
	pthread_mutex_init(&api->mutex, NULL);

	api->fd = -1;
	return api;

error2:
	free(api);
error:
	return NULL;
}

static int api_ws_socket_unix(const char *path, int server)
{
	int fd, rc;
	struct sockaddr_un addr;
	size_t length;

	length = strlen(path);
	if (length >= 108) {
		errno = ENAMETOOLONG;
		return -1;
	}

	if (server && path[0] != '@')
		unlink(path);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return fd;

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path);
	if (addr.sun_path[0] == '@')
		addr.sun_path[0] = 0; /* implement abstract sockets */
	if (server) {
		rc = bind(fd, (struct sockaddr *) &addr, (socklen_t)(sizeof addr));
	} else {
		rc = connect(fd, (struct sockaddr *) &addr, (socklen_t)(sizeof addr));
	}
	if (rc < 0) {
		close(fd);
		return rc;
	}
	return fd;
}

static int api_ws_socket_inet(const char *path, int server)
{
	int rc, fd;
	const char *service, *host, *api;
	struct addrinfo hint, *rai, *iai;

	/* scan the uri */
	api = strrchr(path, '/');
	service = strrchr(path, ':');
	if (api == NULL || service == NULL || api < service) {
		errno = EINVAL;
		return -1;
	}
	host = strndupa(path, service++ - path);
	service = strndupa(service, api - service);

	/* get addr */
	memset(&hint, 0, sizeof hint);
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	rc = getaddrinfo(host, service, &hint, &rai);
	if (rc != 0) {
		errno = EINVAL;
		return -1;
	}

	/* get the socket */
	iai = rai;
	while (iai != NULL) {
		fd = socket(iai->ai_family, iai->ai_socktype, iai->ai_protocol);
		if (fd >= 0) {
			if (server) {
				rc = bind(fd, iai->ai_addr, iai->ai_addrlen);
			} else {
				rc = connect(fd, iai->ai_addr, iai->ai_addrlen);
			}
			if (rc == 0) {
				freeaddrinfo(rai);
				return fd;
			}
			close(fd);
		}
		iai = iai->ai_next;
	}
	freeaddrinfo(rai);
	return -1;
	
}

static int api_ws_socket(const char *path, int server)
{
	int fd, rc;

	/* check for systemd socket */
	if (0 == strncmp(path, "sd:", 3))
		fd = sd_fds_for(path + 3);
	else {
		/* check for unix socket */
		if (0 == strncmp(path, "unix:", 5))
			/* unix socket */
			fd = api_ws_socket_unix(path + 5, server);
		else
			/* inet socket */
			fd = api_ws_socket_inet(path, server);

		if (fd >= 0 && server) {
			rc = 1;
			setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof rc);
			rc = listen(fd, 5);
		}
	}
	/* configure the socket */
	if (fd >= 0) {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
		fcntl(fd, F_SETFL, O_NONBLOCK);
	}
	return fd;
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

static char *api_ws_read_get(struct readbuf *rb, uint32_t length)
{
	char *before = rb->head;
	char *after = before + length;
	if (after > rb->end)
		return 0;
	rb->head = after;
	return before;
}

static int api_ws_read_char(struct readbuf *rb, char *value)
{
	if (rb->head >= rb->end)
		return 0;
	*value = *rb->head++;
	return 1;
}

static int api_ws_read_uint32(struct readbuf *rb, uint32_t *value)
{
	char *after = rb->head + sizeof *value;
	if (after > rb->end)
		return 0;
	memcpy(value, rb->head, sizeof *value);
	rb->head = after;
	*value = le32toh(*value);
	return 1;
}

static int api_ws_read_string(struct readbuf *rb, const char **value, size_t *length)
{
	uint32_t len;
	if (!api_ws_read_uint32(rb, &len) || !len)
		return 0;
	if (length)
		*length = (size_t)(len - 1);
	return (*value = api_ws_read_get(rb, len)) != NULL &&  rb->head[-1] == 0;
}

static int api_ws_read_object(struct readbuf *rb, struct json_object **object)
{
	const char *string;
	struct json_object *o;
	int rc = api_ws_read_string(rb, &string, NULL);
	if (rc) {
		o = json_tokener_parse(string);
		if (o == NULL && strcmp(string, "null"))
			o = json_object_new_string(string);
		*object = o;
	}
	return rc;
}

static int api_ws_write_put(struct writebuf *wb, const void *value, size_t length)
{
	int i = wb->count;
	if (i == WRITEBUF_COUNT_MAX)
		return 0;
	wb->iovec[i].iov_base = (void*)value;
	wb->iovec[i].iov_len = length;
	wb->count = i + 1;
	return 1;
}

static int api_ws_write_char(struct writebuf *wb, char value)
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

static int api_ws_write_uint32(struct writebuf *wb, uint32_t value)
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

static int api_ws_write_string_length(struct writebuf *wb, const char *value, size_t length)
{
	uint32_t len = (uint32_t)++length;
	return (size_t)len == length && len && api_ws_write_uint32(wb, len) && api_ws_write_put(wb, value, length);
}

static int api_ws_write_string(struct writebuf *wb, const char *value)
{
	return api_ws_write_string_length(wb, value, strlen(value));
}

static int api_ws_write_object(struct writebuf *wb, struct json_object *object)
{
	const char *string = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
	return string != NULL && api_ws_write_string(wb, string);
}

/******************* client part **********************************/

/*
 * structure for recording query data
 */
struct api_ws_memo {
	struct api_ws_memo *next;	/* the next memo */
	struct api_ws *api;		/* the ws api */
	struct afb_xreq *xreq;		/* the request handle */
	uint32_t msgid;			/* the message identifier */
};

struct api_ws_event
{
	struct api_ws_event *next;
	struct afb_event event;
	int eventid;
	int refcount;
};

/* search a memorized request */
static struct api_ws_memo *api_ws_client_memo_search(struct api_ws *api, uint32_t msgid)
{
	struct api_ws_memo *memo;

	memo = api->client.memos;
	while (memo != NULL && memo->msgid != msgid)
		memo = memo->next;

	return memo;
}

/* search the event */
static struct api_ws_event *api_ws_client_event_search(struct api_ws *api, uint32_t eventid, const char *name)
{
	struct api_ws_event *ev;

	ev = api->client.events;
	while (ev != NULL && (ev->eventid != eventid || 0 != strcmp(afb_evt_event_name(ev->event), name)))
		ev = ev->next;

	return ev;
}


/* allocates and init the memorizing data */
static struct api_ws_memo *api_ws_client_memo_make(struct api_ws *api, struct afb_xreq *xreq)
{
	struct api_ws_memo *memo;

	memo = malloc(sizeof *memo);
	if (memo != NULL) {
		afb_xreq_addref(xreq);
		memo->xreq = xreq;
		memo->msgid = ptr2id(memo);
		while(api_ws_client_memo_search(api, memo->msgid) != NULL)
			memo->msgid++;
		memo->api = api;
		memo->next = api->client.memos;
		api->client.memos = memo;
	}
	return memo;
}

/* free and release the memorizing data */
static void api_ws_client_memo_destroy(struct api_ws_memo *memo)
{
	struct api_ws_memo **prv;

	prv = &memo->api->client.memos;
	while (*prv != NULL) {
		if (*prv == memo) {
			*prv = memo->next;
			break;
		}
		prv = &(*prv)->next;
	}

	afb_xreq_unref(memo->xreq);
	free(memo);
}

/* get event data from the message */
static int api_ws_client_msg_event_read(struct readbuf *rb, uint32_t *eventid, const char **name)
{
	return api_ws_read_uint32(rb, eventid) && api_ws_read_string(rb, name, NULL);
}

/* get event from the message */
static int api_ws_client_msg_event_get(struct api_ws *api, struct readbuf *rb, struct api_ws_event **ev)
{
	const char *name;
	uint32_t eventid;

	/* get event data from the message */
	if (!api_ws_client_msg_event_read(rb, &eventid, &name)) {
		ERROR("Invalid message");
		return 0;
	}

	/* check conflicts */
	*ev = api_ws_client_event_search(api, eventid, name);
	if (*ev == NULL) {
		ERROR("event %s not found", name);
		return 0;
	}

	return 1;
}

/* get event from the message */
static int api_ws_client_msg_memo_get(struct api_ws *api, struct readbuf *rb, struct api_ws_memo **memo)
{
	uint32_t msgid;

	/* get event data from the message */
	if (!api_ws_read_uint32(rb, &msgid)) {
		ERROR("Invalid message");
		return 0;
	}

	/* get the memo */
	*memo = api_ws_client_memo_search(api, msgid);
	if (*memo == NULL) {
		ERROR("message not found");
		return 0;
	}

	return 1;
}

/* read a subscrition message */
static int api_ws_client_msg_subscription_get(struct api_ws *api, struct readbuf *rb, struct api_ws_memo **memo, struct api_ws_event **ev)
{
	return api_ws_client_msg_memo_get(api, rb, memo) && api_ws_client_msg_event_get(api, rb, ev);
}

/* adds an event */
static void api_ws_client_event_create(struct api_ws *api, struct readbuf *rb)
{
	size_t offset;
	const char *name;
	uint32_t eventid;
	struct api_ws_event *ev;

	/* get event data from the message */
	offset = api_ws_client_msg_event_read(rb, &eventid, &name);
	if (offset == 0) {
		ERROR("Invalid message");
		return;
	}

	/* check conflicts */
	ev = api_ws_client_event_search(api, eventid, name);
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
			ev->next = api->client.events;
			api->client.events = ev;
			return;
		}
	}
	ERROR("can't create event %s, out of memory", name);
}

/* removes an event */
static void api_ws_client_event_drop(struct api_ws *api, struct readbuf *rb)
{
	struct api_ws_event *ev, **prv;

	/* retrieves the event */
	if (!api_ws_client_msg_event_get(api, rb, &ev))
		return;

	/* decrease the reference count */
	if (--ev->refcount)
		return;

	/* unlinks the event */
	prv = &api->client.events;
	while (*prv != ev)
		prv = &(*prv)->next;
	*prv = ev->next;

	/* destroys the event */
	afb_event_drop(ev->event);
	free(ev);
}

/* subscribes an event */
static void api_ws_client_event_subscribe(struct api_ws *api, struct readbuf *rb)
{
	struct api_ws_event *ev;
	struct api_ws_memo *memo;

	if (api_ws_client_msg_subscription_get(api, rb, &memo, &ev)) {
		/* subscribe the request from the event */
		if (afb_xreq_subscribe(memo->xreq, ev->event) < 0)
			ERROR("can't subscribe: %m");
	}
}

/* unsubscribes an event */
static void api_ws_client_event_unsubscribe(struct api_ws *api, struct readbuf *rb)
{
	struct api_ws_event *ev;
	struct api_ws_memo *memo;

	if (api_ws_client_msg_subscription_get(api, rb, &memo, &ev)) {
		/* unsubscribe the request from the event */
		if (afb_xreq_unsubscribe(memo->xreq, ev->event) < 0)
			ERROR("can't unsubscribe: %m");
	}
}

/* receives broadcasted events */
static void api_ws_client_event_broadcast(struct api_ws *api, struct readbuf *rb)
{
	struct json_object *object;
	const char *event;

	if (api_ws_read_string(rb, &event, NULL) && api_ws_read_object(rb, &object))
		afb_evt_broadcast(event, object);
	else
		ERROR("unreadable broadcasted event");
}

/* pushs an event */
static void api_ws_client_event_push(struct api_ws *api, struct readbuf *rb)
{
	struct api_ws_event *ev;
	struct json_object *object;

	if (api_ws_client_msg_event_get(api, rb, &ev) && api_ws_read_object(rb, &object))
		afb_event_push(ev->event, object);
	else
		ERROR("unreadable push event");
}

static void api_ws_client_reply_success(struct api_ws *api, struct readbuf *rb)
{
	struct api_ws_memo *memo;
	struct json_object *object;
	const char *info;
	uint32_t flags;

	/* retrieve the message data */
	if (!api_ws_client_msg_memo_get(api, rb, &memo))
		return;

	if (api_ws_read_uint32(rb, &flags)
	 && api_ws_read_string(rb, &info, NULL)
	 && api_ws_read_object(rb, &object)) {
		memo->xreq->context.flags = (unsigned)flags;
		afb_xreq_success(memo->xreq, object, *info ? info : NULL);
	} else {
		/* failing to have the answer */
		afb_xreq_fail(memo->xreq, "error", "ws error");
	}
	api_ws_client_memo_destroy(memo);
}

static void api_ws_client_reply_fail(struct api_ws *api, struct readbuf *rb)
{
	struct api_ws_memo *memo;
	const char *info, *status;
	uint32_t flags;

	/* retrieve the message data */
	if (!api_ws_client_msg_memo_get(api, rb, &memo))
		return;

	if (api_ws_read_uint32(rb, &flags)
	 && api_ws_read_string(rb, &status, NULL)
	 && api_ws_read_string(rb, &info, NULL)) {
		memo->xreq->context.flags = (unsigned)flags;
		afb_xreq_fail(memo->xreq, status, *info ? info : NULL);
	} else {
		/* failing to have the answer */
		afb_xreq_fail(memo->xreq, "error", "ws error");
	}
	api_ws_client_memo_destroy(memo);
}

/* send a subcall reply */
static void api_ws_client_send_subcall_reply(struct api_ws_reply *reply, int iserror, json_object *object)
{
	int rc;
	struct writebuf wb = { .count = 0 };
	char ie = (char)!!iserror;

	if (!api_ws_write_char(&wb, CHAR_FOR_SUBCALL_REPLY)
	 || !api_ws_write_uint32(&wb, reply->subcallid)
	 || !api_ws_write_char(&wb, ie)
	 || !api_ws_write_object(&wb, object)) {
		/* write error ? */
		return;
	}

	rc = afb_ws_binary_v(reply->apiws->client.ws, wb.iovec, wb.count);
	if (rc >= 0)
		return;
	ERROR("error while sending subcall reply");
}

/* callback for subcall reply */
static void api_ws_client_subcall_reply_cb(void *closure, int iserror, json_object *object)
{
	api_ws_client_send_subcall_reply(closure, iserror, object);
	free(closure);
}

/* received a subcall request */
static void api_ws_client_subcall(struct api_ws *apiws, struct readbuf *rb)
{
	struct api_ws_reply *reply;
	struct api_ws_memo *memo;
	const char *api, *verb;
	uint32_t subcallid;
	struct json_object *object;

	reply = malloc(sizeof *reply);
	if (!reply)
		return;

	/* retrieve the message data */
	if (!api_ws_client_msg_memo_get(apiws, rb, &memo))
		return;

	if (api_ws_read_uint32(rb, &subcallid)
	 && api_ws_read_string(rb, &api, NULL)
	 && api_ws_read_string(rb, &verb, NULL)
	 && api_ws_read_object(rb, &object)) {
		reply->apiws = apiws;
		reply->subcallid = subcallid;
		afb_xreq_subcall(memo->xreq, api, verb, object, api_ws_client_subcall_reply_cb, reply);
	}
}

/* callback when receiving binary data */
static void api_ws_client_on_binary(void *closure, char *data, size_t size)
{
	if (size > 0) {
		struct api_ws *apiws = closure;
		struct readbuf rb = { .head = data, .end = data + size };

		pthread_mutex_lock(&apiws->mutex);
		switch (*rb.head++) {
		case CHAR_FOR_ANSWER_SUCCESS: /* success */
			api_ws_client_reply_success(apiws, &rb);
			break;
		case CHAR_FOR_ANSWER_FAIL: /* fail */
			api_ws_client_reply_fail(apiws, &rb);
			break;
		case CHAR_FOR_EVT_BROADCAST: /* broadcast */
			api_ws_client_event_broadcast(apiws, &rb);
			break;
		case CHAR_FOR_EVT_ADD: /* creates the event */
			api_ws_client_event_create(apiws, &rb);
			break;
		case CHAR_FOR_EVT_DEL: /* drops the event */
			api_ws_client_event_drop(apiws, &rb);
			break;
		case CHAR_FOR_EVT_PUSH: /* pushs the event */
			api_ws_client_event_push(apiws, &rb);
			break;
		case CHAR_FOR_EVT_SUBSCRIBE: /* subscribe event for a request */
			api_ws_client_event_subscribe(apiws, &rb);
			break;
		case CHAR_FOR_EVT_UNSUBSCRIBE: /* unsubscribe event for a request */
			api_ws_client_event_unsubscribe(apiws, &rb);
			break;
		case CHAR_FOR_SUBCALL_CALL: /* subcall */
			api_ws_client_subcall(apiws, &rb);
			break;
		default: /* unexpected message */
			/* TODO: close the connection */
			break;
		}
		pthread_mutex_unlock(&apiws->mutex);
	}
	free(data);
}

/* on call, propagate it to the ws service */
static void api_ws_client_call_cb(void * closure, struct afb_xreq *xreq)
{
	int rc;
	struct api_ws_memo *memo;
	struct writebuf wb = { .count = 0 };
	const char *raw;
	size_t szraw;
	struct api_ws *apiws = closure;

	pthread_mutex_lock(&apiws->mutex);

	/* create the recording data */
	memo = api_ws_client_memo_make(apiws, xreq);
	if (memo == NULL) {
		afb_xreq_fail_f(xreq, "error", "out of memory");
		goto end;
	}

	/* creates the call message */
	raw = afb_xreq_raw(xreq, &szraw);
	if (raw == NULL)
		goto internal_error;
	if (!api_ws_write_char(&wb, CHAR_FOR_CALL)
	 || !api_ws_write_uint32(&wb, memo->msgid)
	 || !api_ws_write_uint32(&wb, (uint32_t)xreq->context.flags)
	 || !api_ws_write_string(&wb, xreq->verb)
	 || !api_ws_write_string(&wb, afb_session_uuid(xreq->context.session))
	 || !api_ws_write_string_length(&wb, raw, szraw))
		goto overflow;

	/* send */
	rc = afb_ws_binary_v(apiws->client.ws, wb.iovec, wb.count);
	if (rc >= 0)
		goto end;

	afb_xreq_fail(xreq, "error", "websocket sending error");
	goto clean_memo;

internal_error:
	afb_xreq_fail(xreq, "error", "internal: raw is NULL!");
	goto clean_memo;

overflow:
	afb_xreq_fail(xreq, "error", "overflow: size doesn't match 32 bits!");

clean_memo:
	api_ws_client_memo_destroy(memo);
end:
	pthread_mutex_unlock(&apiws->mutex);
}

/*  */
static void api_ws_client_disconnect(struct api_ws *api)
{
	if (api->fd >= 0) {
		afb_ws_destroy(api->client.ws);
		api->client.ws = NULL;
		close(api->fd);
		api->fd = -1;
	}
}

/*  */
static int api_ws_client_connect(struct api_ws *api)
{
	struct afb_ws *ws;
	int fd;

	fd = api_ws_socket(api->path, 0);
	if (fd >= 0) {
		ws = afb_ws_create(afb_common_get_event_loop(), fd, &api_ws_client_ws_itf, api);
		if (ws != NULL) {
			api->client.ws = ws;
			api->fd = fd;
			return 0;
		}
		close(fd);
	}
	return -1;
}

static struct afb_api_itf ws_api_itf = {
	.call = api_ws_client_call_cb
};

/* adds a afb-ws-service client api */
int afb_api_ws_add_client(const char *path, struct afb_apiset *apiset)
{
	int rc;
	struct api_ws *api;
	struct afb_api afb_api;

	/* create the ws client api */
	api = api_ws_make(path);
	if (api == NULL)
		goto error;

	/* connect to the service */
	rc = api_ws_client_connect(api);
	if (rc < 0) {
		ERROR("can't connect to ws service %s", api->path);
		goto error2;
	}

	/* record it as an API */
	afb_api.closure = api;
	afb_api.itf = &ws_api_itf;
	if (afb_apiset_add(apiset, api->api, afb_api) < 0)
		goto error3;

	return 0;

error3:
	api_ws_client_disconnect(api);
error2:
	free(api);
error:
	return -1;
}

/******************* client description part for server *****************************/

static void api_ws_server_client_unref(struct api_ws_client *client)
{
	struct api_ws_subcall *sc, *nsc;

	if (!__atomic_sub_fetch(&client->refcount, 1, __ATOMIC_RELAXED)) {
		afb_evt_listener_unref(client->listener);
		afb_ws_destroy(client->ws);
		nsc = client->subcalls;
		while (nsc) {
			sc= nsc;
			nsc = sc->next;
			sc->callback(sc->closure, 1, NULL);
			free(sc);
		}
		afb_cred_unref(client->cred);
		afb_apiset_unref(client->apiset);
		free(client);
	}
}

static void api_ws_server_client_addref(struct api_ws_client *client)
{
	__atomic_add_fetch(&client->refcount, 1, __ATOMIC_RELAXED);
}

/* on call, propagate it to the ws service */
static void api_ws_server_on_call(struct api_ws_client *client, struct readbuf *rb)
{
	struct api_ws_server_req *wreq;
	char *cverb;
	const char *uuid, *verb;
	uint32_t flags, msgid;
	size_t lenverb;
	struct json_object *object;

	api_ws_server_client_addref(client);

	/* reads the call message data */
	if (!api_ws_read_uint32(rb, &msgid)
	 || !api_ws_read_uint32(rb, &flags)
	 || !api_ws_read_string(rb, &verb, &lenverb)
	 || !api_ws_read_string(rb, &uuid, NULL)
	 || !api_ws_read_object(rb, &object))
		goto overflow;

	/* create the request */
	wreq = malloc(++lenverb + sizeof *wreq);
	if (wreq == NULL)
		goto out_of_memory;

	afb_xreq_init(&wreq->xreq, &afb_api_ws_xreq_itf);
	wreq->client = client;
	wreq->msgid = msgid;
	cverb = (char*)&wreq[1];
	memcpy(cverb, verb, lenverb);

	/* init the context */
	if (afb_context_connect(&wreq->xreq.context, uuid, NULL) < 0)
		goto unconnected;
	wreq->xreq.context.flags = flags;

	/* makes the call */
	wreq->xreq.cred = afb_cred_addref(client->cred);
	wreq->xreq.api = client->api;
	wreq->xreq.verb = cverb;
	wreq->xreq.json = object;
	afb_xreq_process(&wreq->xreq, client->apiset);
	return;

unconnected:
	free(wreq);
out_of_memory:
	json_object_put(object);
overflow:
	api_ws_server_client_unref(client);
}

/* on subcall reply */
static void api_ws_server_on_subcall_reply(struct api_ws_client *client, struct readbuf *rb)
{
	char iserror;
	uint32_t subcallid;
	struct json_object *object;
	struct api_ws_subcall *sc, **psc;

	/* reads the call message data */
	if (!api_ws_read_uint32(rb, &subcallid)
	 || !api_ws_read_char(rb, &iserror)
	 || !api_ws_read_object(rb, &object)) {
		/* TODO bad protocol */
		return;
	}

	/* search the subcall and unlink it */
	pthread_mutex_lock(&client->mutex);
	psc = &client->subcalls;
	while ((sc = *psc) && sc->subcallid != subcallid)
		psc = &sc->next;
	if (!sc) {
		pthread_mutex_unlock(&client->mutex);
		/* TODO subcall not found */
	} else {
		*psc = sc->next;
		pthread_mutex_unlock(&client->mutex);
		sc->callback(sc->closure, (int)iserror, object);
		free(sc);
	}
	json_object_put(object);
}

/* callback when receiving binary data */
static void api_ws_server_on_binary(void *closure, char *data, size_t size)
{
	if (size > 0) {
		struct readbuf rb = { .head = data, .end = data + size };
		switch (*rb.head++) {
		case CHAR_FOR_CALL:
			api_ws_server_on_call(closure, &rb);
			break;
		case CHAR_FOR_SUBCALL_REPLY:
			api_ws_server_on_subcall_reply(closure, &rb);
			break;
		default: /* unexpected message */
			/* TODO: close the connection */
			break;
		}
	}
	free(data);
}

/* callback when receiving a hangup */
static void api_ws_server_on_hangup(void *closure)
{
	struct api_ws_client *client = closure;

	/* close the socket */
	if (client->fd >= 0) {
		close(client->fd);
		client->fd = -1;
	}

	/* release the client */
	api_ws_server_client_unref(client);
}

static void api_ws_server_accept(struct api_ws *api)
{
	struct api_ws_client *client;
	struct sockaddr addr;
	socklen_t lenaddr;

	client = calloc(1, sizeof *client);
	if (client != NULL) {
		client->listener = afb_evt_listener_create(&api_ws_server_evt_itf, client);
		if (client->listener != NULL) {
			lenaddr = (socklen_t)sizeof addr;
			client->fd = accept(api->fd, &addr, &lenaddr);
			if (client->fd >= 0) {
				client->cred = afb_cred_create_for_socket(client->fd);
				fcntl(client->fd, F_SETFD, FD_CLOEXEC);
				fcntl(client->fd, F_SETFL, O_NONBLOCK);
				client->ws = afb_ws_create(afb_common_get_event_loop(), client->fd, &api_ws_server_ws_itf, client);
				if (client->ws != NULL) {
					client->api = api->api;
					client->apiset = afb_apiset_addref(api->server.apiset);
					client->refcount = 1;
					client->subcalls = NULL;
					return;
				}
				afb_cred_unref(client->cred);
				close(client->fd);
			}
			afb_evt_listener_unref(client->listener);
		}
		free(client);
	}
}

/******************* server part: manage events **********************************/

static void api_ws_server_event_send(struct api_ws_client *client, char order, const char *event, int eventid, const char *data)
{
	int rc;
	struct writebuf wb = { .count = 0 };

	if (api_ws_write_char(&wb, order)
	 && api_ws_write_uint32(&wb, eventid)
	 && api_ws_write_string(&wb, event)
	 && (data == NULL || api_ws_write_string(&wb, data))) {
		rc = afb_ws_binary_v(client->ws, wb.iovec, wb.count);
		if (rc >= 0)
			return;
	}
	ERROR("error while sending %c for event %s", order, event);
}

static void api_ws_server_event_add(void *closure, const char *event, int eventid)
{
	api_ws_server_event_send(closure, CHAR_FOR_EVT_ADD, event, eventid, NULL);
}

static void api_ws_server_event_remove(void *closure, const char *event, int eventid)
{
	api_ws_server_event_send(closure, CHAR_FOR_EVT_DEL, event, eventid, NULL);
}

static void api_ws_server_event_push(void *closure, const char *event, int eventid, struct json_object *object)
{
	const char *data = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
	api_ws_server_event_send(closure, CHAR_FOR_EVT_PUSH, event, eventid, data ? : "null");
	json_object_put(object);
}

static void api_ws_server_event_broadcast(void *closure, const char *event, int eventid, struct json_object *object)
{
	int rc;
	struct api_ws_client *client = closure;

	struct writebuf wb = { .count = 0 };

	if (api_ws_write_char(&wb, CHAR_FOR_EVT_BROADCAST) && api_ws_write_string(&wb, event) && api_ws_write_object(&wb, object)) {
		rc = afb_ws_binary_v(client->ws, wb.iovec, wb.count);
		if (rc < 0)
			ERROR("error while broadcasting event %s", event);
	} else
		ERROR("error while broadcasting event %s", event);
	json_object_put(object);
}

/******************* ws request part for server *****************/

/* decrement the reference count of the request and free/release it on falling to null */
static void api_ws_server_req_destroy_cb(struct afb_xreq *xreq)
{
	struct api_ws_server_req *wreq = CONTAINER_OF_XREQ(struct api_ws_server_req, xreq);

	afb_context_disconnect(&wreq->xreq.context);
	afb_cred_unref(wreq->xreq.cred);
	json_object_put(wreq->xreq.json);
	api_ws_server_client_unref(wreq->client);
	free(wreq);
}

static void api_ws_server_req_success_cb(struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	int rc;
	struct writebuf wb = { .count = 0 };
	struct api_ws_server_req *wreq = CONTAINER_OF_XREQ(struct api_ws_server_req, xreq);

	if (api_ws_write_char(&wb, CHAR_FOR_ANSWER_SUCCESS)
	 && api_ws_write_uint32(&wb, wreq->msgid)
	 && api_ws_write_uint32(&wb, (uint32_t)wreq->xreq.context.flags)
	 && api_ws_write_string(&wb, info ? : "")
	 && api_ws_write_object(&wb, obj)) {
		rc = afb_ws_binary_v(wreq->client->ws, wb.iovec, wb.count);
		if (rc >= 0)
			goto success;
	}
	ERROR("error while sending success");
success:
	json_object_put(obj);
}

static void api_ws_server_req_fail_cb(struct afb_xreq *xreq, const char *status, const char *info)
{
	int rc;
	struct writebuf wb = { .count = 0 };
	struct api_ws_server_req *wreq = CONTAINER_OF_XREQ(struct api_ws_server_req, xreq);

	if (api_ws_write_char(&wb, CHAR_FOR_ANSWER_FAIL)
	 && api_ws_write_uint32(&wb, wreq->msgid)
	 && api_ws_write_uint32(&wb, (uint32_t)wreq->xreq.context.flags)
	 && api_ws_write_string(&wb, status)
	 && api_ws_write_string(&wb, info ? : "")) {
		rc = afb_ws_binary_v(wreq->client->ws, wb.iovec, wb.count);
		if (rc >= 0)
			return;
	}
	ERROR("error while sending fail");
}

static void api_ws_server_req_subcall_cb(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	int rc;
	struct writebuf wb = { .count = 0 };
	struct api_ws_subcall *sc, *osc;
	struct api_ws_server_req *wreq = CONTAINER_OF_XREQ(struct api_ws_server_req, xreq);
	struct api_ws_client *client = wreq->client;

	sc = malloc(sizeof *sc);
	if (!sc) {

	} else {
		sc->callback = callback;
		sc->closure = cb_closure;

		pthread_mutex_unlock(&client->mutex);
		sc->subcallid = ptr2id(sc);
		do {
			sc->subcallid++;
			osc = client->subcalls;
			while(osc && osc->subcallid != sc->subcallid)
				osc = osc->next;
		} while (osc);
		sc->next = client->subcalls;
		client->subcalls = sc;
		pthread_mutex_unlock(&client->mutex);

		if (api_ws_write_char(&wb, CHAR_FOR_SUBCALL_CALL)
		 && api_ws_write_uint32(&wb, wreq->msgid)
		 && api_ws_write_uint32(&wb, sc->subcallid)
		 && api_ws_write_string(&wb, api)
		 && api_ws_write_string(&wb, verb)
		 && api_ws_write_object(&wb, args)) {
			rc = afb_ws_binary_v(wreq->client->ws, wb.iovec, wb.count);
			if (rc >= 0)
				return;
		}
		ERROR("error while sending fail");
	}
}

static int api_ws_server_req_subscribe_cb(struct afb_xreq *xreq, struct afb_event event)
{
	int rc, rc2;
	struct writebuf wb = { .count = 0 };
	struct api_ws_server_req *wreq = CONTAINER_OF_XREQ(struct api_ws_server_req, xreq);

	rc = afb_evt_add_watch(wreq->client->listener, event);
	if (rc < 0)
		return rc;

	if (api_ws_write_char(&wb, CHAR_FOR_EVT_SUBSCRIBE)
	 && api_ws_write_uint32(&wb, wreq->msgid)
	 && api_ws_write_uint32(&wb, (uint32_t)afb_evt_event_id(event))
	 && api_ws_write_string(&wb, afb_evt_event_name(event))) {
		rc2 = afb_ws_binary_v(wreq->client->ws, wb.iovec, wb.count);
		if (rc2 >= 0)
			goto success;
	}
	ERROR("error while subscribing event");
success:
	return rc;
}

static int api_ws_server_req_unsubscribe_cb(struct afb_xreq *xreq, struct afb_event event)
{
	int rc, rc2;
	struct writebuf wb = { .count = 0 };
	struct api_ws_server_req *wreq = CONTAINER_OF_XREQ(struct api_ws_server_req, xreq);

	if (api_ws_write_char(&wb, CHAR_FOR_EVT_UNSUBSCRIBE)
	 && api_ws_write_uint32(&wb, wreq->msgid)
	 && api_ws_write_uint32(&wb, (uint32_t)afb_evt_event_id(event))
	 && api_ws_write_string(&wb, afb_evt_event_name(event))) {
		rc2 = afb_ws_binary_v(wreq->client->ws, wb.iovec, wb.count);
		if (rc2 >= 0)
			goto success;
	}
	ERROR("error while subscribing event");
success:
	rc = afb_evt_remove_watch(wreq->client->listener, event);
	return rc;
}

/******************* server part **********************************/

static int api_ws_server_connect(struct api_ws *api);

static int api_ws_server_listen_callback(sd_event_source *src, int fd, uint32_t revents, void *closure)
{
	if ((revents & EPOLLIN) != 0)
		api_ws_server_accept(closure);
	if ((revents & EPOLLHUP) != 0)
		api_ws_server_connect(closure);
	return 0;
}

static void api_ws_server_disconnect(struct api_ws *api)
{
	if (api->server.listensrc != NULL) {
		sd_event_source_unref(api->server.listensrc);
		api->server.listensrc = NULL;
	}
	if (api->fd >= 0) {
		close(api->fd);
		api->fd = -1;
	}
}

static int api_ws_server_connect(struct api_ws *api)
{
	int rc;

	/* ensure disconnected */
	api_ws_server_disconnect(api);

	/* request the service object name */
	api->fd = api_ws_socket(api->path, 1);
	if (api->fd < 0)
		ERROR("can't create socket %s", api->path);
	else {
		/* listen for service */
		rc = sd_event_add_io(afb_common_get_event_loop(), &api->server.listensrc, api->fd, EPOLLIN, api_ws_server_listen_callback, api);
		if (rc >= 0)
			return 0;
		close(api->fd);
		errno = -rc;
		ERROR("can't add ws object %s", api->path);
	}
	return -1;
}

/* create the service */
int afb_api_ws_add_server(const char *path, struct afb_apiset *apiset)
{
	int rc;
	struct api_ws *api;

	/* creates the ws api object */
	api = api_ws_make(path);
	if (api == NULL)
		goto error;

	/* connect for serving */
	rc = api_ws_server_connect(api);
	if (rc < 0)
		goto error2;

	api->server.apiset = afb_apiset_addref(apiset);
	return 0;

error2:
	free(api);
error:
	return -1;
}


