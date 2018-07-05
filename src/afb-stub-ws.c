/*
 * Copyright (C) 2015-2018 "IoT.bzh"
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

#include <afb/afb-event-x2.h>

#include "afb-session.h"
#include "afb-cred.h"
#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-proto-ws.h"
#include "afb-stub-ws.h"
#include "afb-context.h"
#include "afb-evt.h"
#include "afb-xreq.h"
#include "verbose.h"
#include "fdev.h"
#include "jobs.h"

struct afb_stub_ws;


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
	struct afb_proto_ws_call *call;	/* the incoming call */
};

/*
 * structure for recording events on client side
 */
struct client_event
{
	struct client_event *next;
	struct afb_event_x2 *event;
	int id;
	int refcount;
};

/*
 * structure for recording describe requests
 */
struct client_describe
{
	struct afb_stub_ws *stubws;
	struct jobloop *jobloop;
	struct json_object *result;
};

/*
 * structure for jobs of describing
 */
struct server_describe
{
	struct afb_stub_ws *stubws;
	struct afb_proto_ws_describe *describe;
};

/*
 * structure for recording sessions
 */
struct server_session
{
	struct server_session *next;
	struct afb_session *session;
};

/******************* stub description for client or servers ******************/

struct afb_stub_ws
{
	/* count of references */
	int refcount;

	/* resource control */
	pthread_mutex_t mutex;

	/* protocol */
	struct afb_proto_ws *proto;

	/* listener for events (server side) */
	struct afb_evt_listener *listener;

	/* event replica (client side) */
	struct client_event *events;

	/* credentials of the client (server side) */
	struct afb_cred *cred;

	/* sessions (server side) */
	struct server_session *sessions;

	/* apiset */
	struct afb_apiset *apiset;

	/* on hangup callback */
	void (*on_hangup)(struct afb_stub_ws *);

	/* the api name */
	char apiname[1];
};

/******************* ws request part for server *****************/

/* decrement the reference count of the request and free/release it on falling to null */
static void server_req_destroy_cb(struct afb_xreq *xreq)
{
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	afb_context_disconnect(&wreq->xreq.context);
	afb_cred_unref(wreq->xreq.cred);
	json_object_put(wreq->xreq.json);
	afb_proto_ws_call_unref(wreq->call);
	afb_stub_ws_unref(wreq->stubws);
	free(wreq);
}

static void server_req_reply_cb(struct afb_xreq *xreq, struct json_object *obj, const char *error, const char *info)
{
	int rc;
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	rc = afb_proto_ws_call_reply(wreq->call, obj, error, info);
	if (rc < 0)
		ERROR("error while sending reply");
	json_object_put(obj);
}

static int server_req_subscribe_cb(struct afb_xreq *xreq, struct afb_event_x2 *event)
{
	int rc;
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	rc = afb_evt_event_x2_add_watch(wreq->stubws->listener, event);
	if (rc >= 0)
		rc = afb_proto_ws_call_subscribe(wreq->call,  afb_evt_event_x2_fullname(event), afb_evt_event_x2_id(event));
	if (rc < 0)
		ERROR("error while subscribing event");
	return rc;
}

static int server_req_unsubscribe_cb(struct afb_xreq *xreq, struct afb_event_x2 *event)
{
	int rc, rc2;
	struct server_req *wreq = CONTAINER_OF_XREQ(struct server_req, xreq);

	rc = afb_proto_ws_call_unsubscribe(wreq->call,  afb_evt_event_x2_fullname(event), afb_evt_event_x2_id(event));
	rc2 = afb_evt_event_x2_remove_watch(wreq->stubws->listener, event);
	if (rc >= 0 && rc2 < 0)
		rc = rc2;
	if (rc < 0)
		ERROR("error while unsubscribing event");
	return rc;
}

static const struct afb_xreq_query_itf server_req_xreq_itf = {
	.reply = server_req_reply_cb,
	.unref = server_req_destroy_cb,
	.subscribe = server_req_subscribe_cb,
	.unsubscribe = server_req_unsubscribe_cb
};

/******************* client part **********************************/

/* search the event */
static struct client_event *client_event_search(struct afb_stub_ws *stubws, uint32_t eventid, const char *name)
{
	struct client_event *ev;

	ev = stubws->events;
	while (ev != NULL && (ev->id != eventid || 0 != strcmp(afb_evt_event_x2_fullname(ev->event), name)))
		ev = ev->next;

	return ev;
}

/* on call, propagate it to the ws service */
static void client_call_cb(void * closure, struct afb_xreq *xreq)
{
	struct afb_stub_ws *stubws = closure;

	afb_proto_ws_client_call(
			stubws->proto,
			xreq->request.called_verb,
			afb_xreq_json(xreq),
			afb_session_uuid(xreq->context.session),
			xreq,
			xreq_on_behalf_cred_export(xreq));
	afb_xreq_unhooked_addref(xreq);
}

static void client_on_description_cb(void *closure, struct json_object *data)
{
	struct client_describe *desc = closure;

	desc->result = data;
	jobs_leave(desc->jobloop);
}

static void client_send_describe_cb(int signum, void *closure, struct jobloop *jobloop)
{
	struct client_describe *desc = closure;

	if (signum)
		jobs_leave(jobloop);
	else {
		desc->jobloop = jobloop;
		afb_proto_ws_client_describe(desc->stubws->proto, client_on_description_cb, desc);
	}
}

/* get the description */
static struct json_object *client_describe_cb(void * closure)
{
	struct client_describe desc;

	/* synchronous job: send the request and wait its result */
	desc.stubws = closure;
	desc.result = NULL;
	jobs_enter(NULL, 0, client_send_describe_cb, &desc);
	return desc.result;
}

/******************* server part: manage events **********************************/

static void server_event_add(void *closure, const char *event, int eventid)
{
	struct afb_stub_ws *stubws = closure;

	afb_proto_ws_server_event_create(stubws->proto, event, eventid);
}

static void server_event_remove(void *closure, const char *event, int eventid)
{
	struct afb_stub_ws *stubws = closure;

	afb_proto_ws_server_event_remove(stubws->proto, event, eventid);
}

static void server_event_push(void *closure, const char *event, int eventid, struct json_object *object)
{
	struct afb_stub_ws *stubws = closure;

	afb_proto_ws_server_event_push(stubws->proto, event, eventid, object);
	json_object_put(object);
}

static void server_event_broadcast(void *closure, const char *event, int eventid, struct json_object *object)
{
	struct afb_stub_ws *stubws = closure;

	afb_proto_ws_server_event_broadcast(stubws->proto, event, object);
	json_object_put(object);
}

/*****************************************************/

static void on_reply(void *closure, void *request, struct json_object *object, const char *error, const char *info)
{
	struct afb_xreq *xreq = request;

	afb_xreq_reply(xreq, object, error, info);
	afb_xreq_unhooked_unref(xreq);
}

static void on_event_create(void *closure, const char *event_name, int event_id)
{
	struct afb_stub_ws *stubws = closure;
	struct client_event *ev;

	/* check conflicts */
	ev = client_event_search(stubws, event_id, event_name);
	if (ev != NULL) {
		ev->refcount++;
		return;
	}

	/* no conflict, try to add it */
	ev = malloc(sizeof *ev);
	if (ev != NULL) {
		ev->event = afb_evt_event_x2_create(event_name);
		if (ev->event != NULL) {
			ev->refcount = 1;
			ev->id = event_id;
			ev->next = stubws->events;
			stubws->events = ev;
			return;
		}
		free(ev);
	}
	ERROR("can't create event %s, out of memory", event_name);
}

static void on_event_remove(void *closure, const char *event_name, int event_id)
{
	struct afb_stub_ws *stubws = closure;
	struct client_event *ev, **prv;

	/* check conflicts */
	ev = client_event_search(stubws, event_id, event_name);
	if (ev == NULL)
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
	afb_evt_event_x2_unref(ev->event);
	free(ev);
}

static void on_event_subscribe(void *closure, void *request, const char *event_name, int event_id)
{
	struct afb_stub_ws *stubws = closure;
	struct afb_xreq *xreq = request;
	struct client_event *ev;

	/* check conflicts */
	ev = client_event_search(stubws, event_id, event_name);
	if (ev == NULL)
		return;

	if (afb_xreq_subscribe(xreq, ev->event) < 0)
		ERROR("can't subscribe: %m");
}

static void on_event_unsubscribe(void *closure, void *request, const char *event_name, int event_id)
{
	struct afb_stub_ws *stubws = closure;
	struct afb_xreq *xreq = request;
	struct client_event *ev;

	/* check conflicts */
	ev = client_event_search(stubws, event_id, event_name);
	if (ev == NULL)
		return;

	if (afb_xreq_unsubscribe(xreq, ev->event) < 0)
		ERROR("can't unsubscribe: %m");
}

static void on_event_push(void *closure, const char *event_name, int event_id, struct json_object *data)
{
	struct afb_stub_ws *stubws = closure;
	struct client_event *ev;

	/* check conflicts */
	ev = client_event_search(stubws, event_id, event_name);
	if (ev)
		afb_evt_event_x2_push(ev->event, data);
	else
		ERROR("unreadable push event");
}

static void on_event_broadcast(void *closure, const char *event_name, struct json_object *data)
{
	afb_evt_broadcast(event_name, data);
}

/*****************************************************/

static void record_session(struct afb_stub_ws *stubws, struct afb_session *session)
{
	struct server_session *s, **prv;

	/* search */
	prv = &stubws->sessions;
	while ((s = *prv)) {
		if (s->session == session)
			return;
		if (afb_session_is_closed(s->session)) {
			*prv = s->next;
			afb_session_unref(s->session);
			free(s);
		}
		else
			prv = &s->next;
	}

	/* create */
	s = malloc(sizeof *s);
	if (s) {
		s->session = afb_session_addref(session);
		s->next = stubws->sessions;
		stubws->sessions = s;
	}
}

static void release_all_sessions(struct afb_stub_ws *stubws)
{
	struct server_session *s, *n;

	s = stubws->sessions;
	stubws->sessions = NULL;
	while(s) {
		n = s->next;
		afb_session_unref(s->session);
		free(s);
		s = n;
	}
}

/*****************************************************/

static void on_call(void *closure, struct afb_proto_ws_call *call, const char *verb, struct json_object *args, const char *sessionid, const char *user_creds)
{
	struct afb_stub_ws *stubws = closure;
	struct server_req *wreq;

	afb_stub_ws_addref(stubws);

	/* create the request */
	wreq = malloc(sizeof *wreq);
	if (wreq == NULL)
		goto out_of_memory;

	afb_xreq_init(&wreq->xreq, &server_req_xreq_itf);
	wreq->stubws = stubws;
	wreq->call = call;

	/* init the context */
	if (afb_context_connect(&wreq->xreq.context, sessionid, NULL) < 0)
		goto unconnected;
	wreq->xreq.context.validated = 1;
	record_session(stubws, wreq->xreq.context.session);
	if (wreq->xreq.context.created)
		afb_session_set_autoclose(wreq->xreq.context.session, 1);

	/* makes the call */
	wreq->xreq.cred = afb_cred_mixed_on_behalf_import(stubws->cred, sessionid, user_creds);
	wreq->xreq.request.called_api = stubws->apiname;
	wreq->xreq.request.called_verb = verb;
	wreq->xreq.json = args;
	afb_xreq_process(&wreq->xreq, stubws->apiset);
	return;

unconnected:
	free(wreq);
out_of_memory:
	json_object_put(args);
	afb_stub_ws_unref(stubws);
	afb_proto_ws_call_reply(call, NULL, "internal-error", NULL);
	afb_proto_ws_call_unref(call);
}

static void server_describe_sjob(int signum, void *closure)
{
	struct json_object *obj;
	struct server_describe *desc = closure;

	/* get the description if possible */
	obj = !signum ? afb_apiset_describe(desc->stubws->apiset, desc->stubws->apiname) : NULL;

	/* send it */
	afb_proto_ws_describe_put(desc->describe, obj);
	json_object_put(obj);
	afb_stub_ws_unref(desc->stubws);
}

static void server_describe_job(int signum, void *closure)
{
	server_describe_sjob(signum, closure);
	free(closure);
}

static void on_describe(void *closure, struct afb_proto_ws_describe *describe)
{
	struct server_describe *desc, sdesc;
	struct afb_stub_ws *stubws = closure;

	/* allocate (if possible) and init */
	desc = malloc(sizeof *desc);
	if (desc == NULL)
		desc = &sdesc;
	desc->stubws = stubws;
	desc->describe = describe;
	afb_stub_ws_addref(stubws);

	/* process */
	if (desc == &sdesc)
		jobs_call(NULL, 0, server_describe_sjob, desc);
	else {
		if (jobs_queue(NULL, 0, server_describe_job, desc) < 0)
			jobs_call(NULL, 0, server_describe_job, desc);
	}
}

/*****************************************************/

static const struct afb_proto_ws_client_itf client_itf =
{
	.on_reply = on_reply,
	.on_event_create = on_event_create,
	.on_event_remove = on_event_remove,
	.on_event_subscribe = on_event_subscribe,
	.on_event_unsubscribe = on_event_unsubscribe,
	.on_event_push = on_event_push,
	.on_event_broadcast = on_event_broadcast,
};

static const struct afb_proto_ws_server_itf server_itf =
{
	.on_call = on_call,
	.on_describe = on_describe
};

static struct afb_api_itf ws_api_itf = {
	.call = client_call_cb,
	.describe = client_describe_cb
};

/* the interface for events pushing */
static const struct afb_evt_itf server_evt_itf = {
	.broadcast = server_event_broadcast,
	.push = server_event_push,
	.add = server_event_add,
	.remove = server_event_remove
};

/*****************************************************/

static void drop_all_events(struct afb_stub_ws *stubws)
{
	struct client_event *ev, *nxt;

	ev = stubws->events;
	stubws->events = NULL;

	while (ev) {
		nxt = ev->next;
		afb_evt_event_x2_unref(ev->event);
		free(ev);
		ev = nxt;
	}
}

/* callback when receiving a hangup */
static void on_hangup(void *closure)
{
	struct afb_stub_ws *stubws = closure;

	afb_stub_ws_addref(stubws);
	if (stubws->on_hangup)
		stubws->on_hangup(stubws);

	release_all_sessions(stubws);
	afb_stub_ws_unref(stubws);
}

static int enqueue_processing(void (*callback)(int signum, void* arg), void *arg)
{
	return jobs_queue(NULL, 0, callback, arg);
}

/*****************************************************/

static struct afb_stub_ws *afb_stub_ws_create(struct fdev *fdev, const char *apiname, struct afb_apiset *apiset, int client)
{
	struct afb_stub_ws *stubws;

	stubws = calloc(1, sizeof *stubws + strlen(apiname));
	if (stubws == NULL)
		errno = ENOMEM;
	else {
		if (client)
			stubws->proto = afb_proto_ws_create_client(fdev, &client_itf, stubws);
		else
			stubws->proto = afb_proto_ws_create_server(fdev, &server_itf, stubws);

		if (stubws->proto) {
			strcpy(stubws->apiname, apiname);
			stubws->apiset = afb_apiset_addref(apiset);
			stubws->refcount = 1;
			afb_proto_ws_on_hangup(stubws->proto, on_hangup);
			afb_proto_ws_set_queuing(stubws->proto, enqueue_processing);
			return stubws;
		}
		free(stubws);
	}
	fdev_unref(fdev);
	return NULL;
}

struct afb_stub_ws *afb_stub_ws_create_client(struct fdev *fdev, const char *apiname, struct afb_apiset *apiset)
{
	return afb_stub_ws_create(fdev, apiname, apiset, 1);
}

struct afb_stub_ws *afb_stub_ws_create_server(struct fdev *fdev, const char *apiname, struct afb_apiset *apiset)
{
	struct afb_stub_ws *stubws;

	stubws = afb_stub_ws_create(fdev, apiname, apiset, 0);
	if (stubws) {
		stubws->cred = afb_cred_create_for_socket(fdev_fd(fdev));
		stubws->listener = afb_evt_listener_create(&server_evt_itf, stubws);
		if (stubws->listener != NULL)
			return stubws;
		afb_stub_ws_unref(stubws);
	}
	return NULL;
}

void afb_stub_ws_unref(struct afb_stub_ws *stubws)
{
	if (!__atomic_sub_fetch(&stubws->refcount, 1, __ATOMIC_RELAXED)) {
		drop_all_events(stubws);
		if (stubws->listener)
			afb_evt_listener_unref(stubws->listener);
		release_all_sessions(stubws);
		afb_proto_ws_unref(stubws->proto);
		afb_cred_unref(stubws->cred);
		afb_apiset_unref(stubws->apiset);
		free(stubws);
	}
}

void afb_stub_ws_addref(struct afb_stub_ws *stubws)
{
	__atomic_add_fetch(&stubws->refcount, 1, __ATOMIC_RELAXED);
}

void afb_stub_ws_on_hangup(struct afb_stub_ws *stubws, void (*on_hangup)(struct afb_stub_ws*))
{
	stubws->on_hangup = on_hangup;
}

const char *afb_stub_ws_name(struct afb_stub_ws *stubws)
{
	return stubws->apiname;
}

struct afb_api_item afb_stub_ws_client_api(struct afb_stub_ws *stubws)
{
	struct afb_api_item api;

	assert(!stubws->listener); /* check client */
	api.closure = stubws;
	api.itf = &ws_api_itf;
	api.group = NULL;
	return api;
}

int afb_stub_ws_client_add(struct afb_stub_ws *stubws, struct afb_apiset *apiset)
{
	return afb_apiset_add(apiset, stubws->apiname, afb_stub_ws_client_api(stubws));
}

