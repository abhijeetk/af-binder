/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
 * Author: José Bollo <jose.bollo@iot.bzh>
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
#include <errno.h>

#include <json-c/json.h>

#include <afb/afb-binding-v1.h>
#include <afb/afb-binding-v2.h>

#include "afb-session.h"
#include "afb-context.h"
#include "afb-evt.h"
#include "afb-msg-json.h"
#include "afb-svc.h"
#include "afb-xreq.h"
#include "afb-cred.h"
#include "afb-apiset.h"
#include "jobs.h"
#include "verbose.h"

/*
 * Structure for recording service
 */
struct afb_svc
{
	/* api/prefix */
	const char *api;

	/* session of the service */
	struct afb_session *session;

	/* the apiset for the service */
	struct afb_apiset *apiset;

	/* event listener of the service or NULL */
	struct afb_evt_listener *listener;

	/* on event callback for the service */
	void (*on_event)(const char *event, struct json_object *object);
};

/*
 * Structure for requests initiated by the service
 */
struct svc_req
{
	struct afb_xreq xreq;

	struct afb_svc *svc;

	/* the args */
	void (*callback)(void*, int, struct json_object*);
	void *closure;

	/* sync */
	struct jobloop *jobloop;
	struct json_object *result;
	int iserror;
};

/* functions for services */
static void svc_on_event(void *closure, const char *event, int eventid, struct json_object *object);
static void svc_call(void *closure, const char *api, const char *verb, struct json_object *args,
				void (*callback)(void*, int, struct json_object*), void *cbclosure);
static int svc_call_sync(void *closure, const char *api, const char *verb, struct json_object *args,
				struct json_object **result);

/* the interface for services */
static const struct afb_service_itf service_itf = {
	.call = svc_call,
	.call_sync = svc_call_sync
};

/* the interface for events */
static const struct afb_evt_itf evt_itf = {
	.broadcast = svc_on_event,
	.push = svc_on_event
};

/* functions for requests of services */
static void svcreq_destroy(struct afb_xreq *xreq);
static void svcreq_reply(struct afb_xreq *xreq, int iserror, json_object *obj);

/* interface for requests of services */
const struct afb_xreq_query_itf afb_svc_xreq_itf = {
	.unref = svcreq_destroy,
	.reply = svcreq_reply
};

/* the common session for services sharing their session */
static struct afb_session *common_session;

static inline struct afb_service to_afb_service(struct afb_svc *svc)
{
	return (struct afb_service){ .itf = &service_itf, .closure = svc };
}

/*
 * Frees a service
 */
static void svc_free(struct afb_svc *svc)
{
	if (svc->listener != NULL)
		afb_evt_listener_unref(svc->listener);
	if (svc->session)
		afb_session_unref(svc->session);
	afb_apiset_unref(svc->apiset);
	free(svc);
}

/*
 * Allocates a new service
 */
static struct afb_svc *afb_svc_alloc(
			const char *api,
			struct afb_apiset *apiset,
			int share_session
)
{
	struct afb_svc *svc;

	/* allocates the svc handler */
	svc = calloc(1, sizeof * svc);
	if (svc == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	/* instanciate the apiset */
	svc->api = api;
	svc->apiset = afb_apiset_addref(apiset);

	/* instanciate the session */
	if (share_session) {
		/* session shared with other svcs */
		if (common_session == NULL) {
			common_session = afb_session_create (NULL, 0);
			if (common_session == NULL)
				goto error;
		}
		svc->session = afb_session_addref(common_session);
	} else {
		/* session dedicated to the svc */
		svc->session = afb_session_create (NULL, 0);
		if (svc->session == NULL)
			goto error;
	}

	return svc;

error:
	svc_free(svc);
	return NULL;
}

/*
 * Creates a new service
 */
struct afb_svc *afb_svc_create_v1(
			const char *api,
			struct afb_apiset *apiset,
			int share_session,
			int (*start)(struct afb_service service),
			void (*on_event)(const char *event, struct json_object *object)
)
{
	int rc;
	struct afb_svc *svc;

	/* allocates the svc handler */
	svc = afb_svc_alloc(api, apiset, share_session);
	if (svc == NULL)
		goto error;

	/* initialises the listener if needed */
	if (on_event) {
		svc->on_event = on_event;
		svc->listener = afb_evt_listener_create(&evt_itf, svc);
		if (svc->listener == NULL)
			goto error;
	}

	/* initialises the svc now */
	if (start) {
		rc = start(to_afb_service(svc));
		if (rc < 0)
			goto error;
	}

	return svc;

error:
	svc_free(svc);
	return NULL;
}

/*
 * Creates a new service
 */
struct afb_svc *afb_svc_create_v2(
			const char *api,
			struct afb_apiset *apiset,
			int share_session,
			int (*start)(),
			void (*on_event)(const char *event, struct json_object *object),
			struct afb_binding_data_v2 *data
)
{
	int rc;
	struct afb_svc *svc;

	/* allocates the svc handler */
	svc = afb_svc_alloc(api, apiset, share_session);
	if (svc == NULL)
		goto error;
	data->service = to_afb_service(svc);

	/* initialises the listener if needed */
	if (on_event) {
		svc->on_event = on_event;
		svc->listener = afb_evt_listener_create(&evt_itf, svc);
		if (svc->listener == NULL)
			goto error;
	}

	/* starts the svc if needed */
	if (start) {
		rc = start();
		if (rc < 0)
			goto error;
	}

	return svc;

error:
	svc_free(svc);
	return NULL;
}

/*
 * Propagates the event to the service
 */
static void svc_on_event(void *closure, const char *event, int eventid, struct json_object *object)
{
	struct afb_svc *svc = closure;
	svc->on_event(event, object);
	json_object_put(object);
}

/*
 * create an svc_req
 */
static struct svc_req *svcreq_create(struct afb_svc *svc, const char *api, const char *verb, struct json_object *args)
{
	struct svc_req *svcreq;
	size_t lenapi, lenverb;
	char *copy;

	/* allocates the request */
	lenapi = 1 + strlen(api);
	lenverb = 1 + strlen(verb);
	svcreq = malloc(lenapi + lenverb + sizeof *svcreq);
	if (svcreq != NULL) {
		/* initialises the request */
		afb_xreq_init(&svcreq->xreq, &afb_svc_xreq_itf);
		afb_context_init(&svcreq->xreq.context, svc->session, NULL);
		svcreq->xreq.context.validated = 1;
		copy = (char*)&svcreq[1];
		memcpy(copy, api, lenapi);
		svcreq->xreq.api = copy;
		copy = &copy[lenapi];
		memcpy(copy, verb, lenverb);
		svcreq->xreq.verb = copy;
		svcreq->xreq.listener = svc->listener;
		svcreq->xreq.json = args;
		svcreq->svc = svc;
	}
	return svcreq;
}

/*
 * destroys the svc_req
 */
static void svcreq_destroy(struct afb_xreq *xreq)
{
	struct svc_req *svcreq = CONTAINER_OF_XREQ(struct svc_req, xreq);

	afb_context_disconnect(&svcreq->xreq.context);
	json_object_put(svcreq->xreq.json);
	afb_cred_unref(svcreq->xreq.cred);
	free(svcreq);
}

static void svcreq_sync_leave(struct svc_req *svcreq)
{
	struct jobloop *jobloop = svcreq->jobloop;

	if (jobloop) {
		svcreq->jobloop = NULL;
		jobs_leave(jobloop);
	}
}

static void svcreq_reply(struct afb_xreq *xreq, int iserror, json_object *obj)
{
	struct svc_req *svcreq = CONTAINER_OF_XREQ(struct svc_req, xreq);
	if (svcreq->callback) {
		svcreq->callback(svcreq->closure, iserror, obj);
		json_object_put(obj);
	} else {
		svcreq->iserror = iserror;
		svcreq->result = obj;
		svcreq_sync_leave(svcreq);
	}
}

static void svcreq_sync_enter(int signum, void *closure, struct jobloop *jobloop)
{
	struct svc_req *svcreq = closure;

	if (!signum) {
		svcreq->jobloop = jobloop;
		afb_xreq_process(&svcreq->xreq, svcreq->svc->apiset);
	} else {
		svcreq->result = afb_msg_json_internal_error();
		svcreq->iserror = 1;
		svcreq_sync_leave(svcreq);
	}
}

/*
 * Initiates a call for the service
 */
static void svc_call(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cbclosure)
{
	struct afb_svc *svc = closure;
	struct svc_req *svcreq;
	struct json_object *ierr;

	/* allocates the request */
	svcreq = svcreq_create(svc, api, verb, args);
	if (svcreq == NULL) {
		ERROR("out of memory");
		json_object_put(args);
		ierr = afb_msg_json_internal_error();
		callback(cbclosure, 1, ierr);
		json_object_put(ierr);
		return;
	}

	/* initialises the request */
	svcreq->jobloop = NULL;
	svcreq->callback = callback;
	svcreq->closure = cbclosure;

	/* terminates and frees ressources if needed */
	afb_xreq_process(&svcreq->xreq, svc->apiset);
}

static int svc_call_sync(void *closure, const char *api, const char *verb, struct json_object *args,
				struct json_object **result)
{
	struct afb_svc *svc = closure;
	struct svc_req *svcreq;
	int rc;

	/* allocates the request */
	svcreq = svcreq_create(svc, api, verb, args);
	if (svcreq == NULL) {
		ERROR("out of memory");
		errno = ENOMEM;
		json_object_put(args);
		*result = afb_msg_json_internal_error();
		return -1;
	}

	/* initialises the request */
	svcreq->jobloop = NULL;
	svcreq->callback = NULL;
	svcreq->result = NULL;
	svcreq->iserror = 1;
	afb_xreq_addref(&svcreq->xreq);
	rc = jobs_enter(NULL, 0, svcreq_sync_enter, svcreq);
	rc = rc >= 0 && !svcreq->iserror;
	*result = (rc || svcreq->result) ? svcreq->result : afb_msg_json_internal_error();
	afb_xreq_unref(&svcreq->xreq);
	return rc;
}

