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

#include <json-c/json.h>

#include <afb/afb-req-itf.h>
#include <afb/afb-service-itf.h>

#include "afb-session.h"
#include "afb-context.h"
#include "afb-evt.h"
#include "afb-msg-json.h"
#include "afb-svc.h"
#include "afb-xreq.h"
#include "afb-cred.h"
#include "afb-apiset.h"
#include "verbose.h"

/*
 * Structure for recording service
 */
struct afb_svc
{
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

	/* the args */
	void (*callback)(void*, int, struct json_object*);
	void *closure;
};

/* functions for services */
static void svc_on_event(void *closure, const char *event, int eventid, struct json_object *object);
static void svc_call(void *closure, const char *api, const char *verb, struct json_object *args,
				void (*callback)(void*, int, struct json_object*), void *cbclosure);

/* the interface for services */
static const struct afb_service_itf service_itf = {
	.call = svc_call
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

/*
 * Allocates a new service
 */
static struct afb_svc *afb_svc_alloc(
			struct afb_apiset *apiset,
			int share_session,
			void (*on_event)(const char *event, struct json_object *object)
)
{
	struct afb_svc *svc;

	/* allocates the svc handler */
	svc = malloc(sizeof * svc);
	if (svc == NULL)
		goto error;

	/* instanciate the apiset */
	svc->apiset = afb_apiset_addref(apiset);

	/* instanciate the session */
	if (share_session) {
		/* session shared with other svcs */
		if (common_session == NULL) {
			common_session = afb_session_create (NULL, 0);
			if (common_session == NULL)
				goto error2;
		}
		svc->session = afb_session_addref(common_session);
	} else {
		/* session dedicated to the svc */
		svc->session = afb_session_create (NULL, 0);
		if (svc->session == NULL)
			goto error2;
	}

	/* initialises the listener if needed */
	svc->on_event = on_event;
	if (on_event == NULL)
		svc->listener = NULL;
	else {
		svc->listener = afb_evt_listener_create(&evt_itf, svc);
		if (svc->listener == NULL)
			goto error3;
	}

	return svc;

error3:
	afb_session_unref(svc->session);
error2:
	afb_apiset_unref(svc->apiset);
	free(svc);
error:
	return NULL;
}

/*
 * Creates a new service
 */
struct afb_svc *afb_svc_create(
		struct afb_apiset *apiset,
		int share_session,
		int (*init)(struct afb_service service),
		void (*on_event)(const char *event, struct json_object *object)
)
{
	int rc;
	struct afb_svc *svc;

	/* allocates the svc handler */
	svc = afb_svc_alloc(apiset, share_session, on_event);
	if (svc == NULL)
		goto error;

	/* initialises the svc now */
	rc = init((struct afb_service){ .itf = &service_itf, .closure = svc });
	if (rc < 0)
		goto error2;

	return svc;

error2:
	if (svc->listener != NULL)
		afb_evt_listener_unref(svc->listener);
	afb_session_unref(svc->session);
	afb_apiset_unref(svc->apiset);
	free(svc);
error:
	return NULL;
}

/*
 * Creates a new service
 */
struct afb_svc *afb_svc_create_v2(
			struct afb_apiset *apiset,
			int share_session,
			void (*on_event)(const char *event, struct json_object *object),
			int (*start)(const struct afb_binding_interface *interface, struct afb_service service),
			const struct afb_binding_interface *interface
)
{
	int rc;
	struct afb_svc *svc;

	/* allocates the svc handler */
	svc = afb_svc_alloc(apiset, share_session, on_event);
	if (svc == NULL)
		goto error;

	/* initialises the svc now */
	rc = start(interface, (struct afb_service){ .itf = &service_itf, .closure = svc });
	if (rc < 0)
		goto error2;

	return svc;

error2:
	if (svc->listener != NULL)
		afb_evt_listener_unref(svc->listener);
	afb_session_unref(svc->session);
	afb_apiset_unref(svc->apiset);
	free(svc);
error:
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
 * Initiates a call for the service
 */
static void svc_call(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cbclosure)
{
	struct afb_svc *svc = closure;
	struct svc_req *svcreq;

	/* allocates the request */
	svcreq = calloc(1, sizeof *svcreq);
	if (svcreq == NULL) {
		ERROR("out of memory");
		json_object_put(args);
		callback(cbclosure, 1, afb_msg_json_internal_error());
		return;
	}

	/* initialises the request */
	afb_xreq_init(&svcreq->xreq, &afb_svc_xreq_itf);
	afb_context_init(&svcreq->xreq.context, svc->session, NULL);
	svcreq->xreq.context.validated = 1;
	svcreq->xreq.cred = afb_cred_current();
	svcreq->xreq.api = api;
	svcreq->xreq.verb = verb;
	svcreq->xreq.listener = svc->listener;
	svcreq->xreq.json = args;
	svcreq->callback = callback;
	svcreq->closure = cbclosure;

	/* terminates and frees ressources if needed */
	afb_xreq_process(&svcreq->xreq, svc->apiset);
}

static void svcreq_destroy(struct afb_xreq *xreq)
{
	struct svc_req *svcreq = CONTAINER_OF_XREQ(struct svc_req, xreq);
	afb_context_disconnect(&svcreq->xreq.context);
	json_object_put(svcreq->xreq.json);
	afb_cred_unref(svcreq->xreq.cred);
	free(svcreq);
}

static void svcreq_reply(struct afb_xreq *xreq, int iserror, json_object *obj)
{
	struct svc_req *svcreq = CONTAINER_OF_XREQ(struct svc_req, xreq);
	svcreq->callback(svcreq->closure, iserror, obj);
	json_object_put(obj);
}

