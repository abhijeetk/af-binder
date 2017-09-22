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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 0
#include <afb/afb-binding.h>

#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-api-dyn.h"
#include "afb-common.h"
#include "afb-cred.h"
#include "afb-evt.h"
#include "afb-export.h"
#include "afb-hook.h"
#include "afb-msg-json.h"
#include "afb-session.h"
#include "afb-xreq.h"
#include "jobs.h"
#include "verbose.h"

/*************************************************************************
 * internal types and structures
 ************************************************************************/

enum afb_api_version
{
	Api_Version_Dyn = 0,
	Api_Version_1 = 1,
	Api_Version_2 = 2,
};

enum afb_api_state
{
	Api_State_Pre_Init,
	Api_State_Init,
	Api_State_Run
};

struct afb_export
{
	/* keep it first */
	struct afb_dynapi dynapi;

	/* name of the api */
	char *apiname;

	/* version of the api */
	unsigned version: 4;

	/* current state */
	unsigned state: 4;

	/* hooking flags */
	int hookditf;
	int hooksvc;

	/* dynamic api */
	struct afb_api_dyn *apidyn;

	/* session for service */
	struct afb_session *session;

	/* apiset for service */
	struct afb_apiset *apiset;

	/* event listener for service or NULL */
	struct afb_evt_listener *listener;

	/* start function */
	union {
		int (*v1)(struct afb_service);
		int (*v2)();
		int (*vdyn)(struct afb_dynapi *dynapi);
	} init;

	/* event handling */
	union {
		void (*v12)(const char *event, struct json_object *object);
		void (*vdyn)(struct afb_dynapi *dynapi, const char *event, struct json_object *object);
	} on_event;

	/* exported data */
	union {
		struct afb_binding_interface_v1 v1;
		struct afb_binding_data_v2 *v2;
	} export;
};

/************************************************************************************************************/

static inline struct afb_dynapi *to_dynapi(struct afb_export *export)
{
	return (struct afb_dynapi*)export;
}

static inline struct afb_export *from_dynapi(struct afb_dynapi *dynapi)
{
	return (struct afb_export*)dynapi;
}

/*************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
                                           F R O M     D I T F
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************/

/**********************************************
* normal flow
**********************************************/
static void vverbose_cb(void *closure, int level, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	char *p;
	struct afb_export *export = closure;

	if (!fmt || vasprintf(&p, fmt, args) < 0)
		vverbose(level, file, line, function, fmt, args);
	else {
		verbose(level, file, line, function, "[API %s] %s", export->apiname, p);
		free(p);
	}
}

static void old_vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args)
{
	vverbose_cb(closure, level, file, line, NULL, fmt, args);
}

static struct afb_eventid *eventid_make_cb(void *closure, const char *name)
{
	size_t plen, nlen;
	char *event;
	struct afb_export *export = closure;

	/* check daemon state */
	if (export->state == Api_State_Pre_Init) {
		ERROR("[API %s] Bad call to 'afb_daemon_event_make(%s)', must not be in PreInit", export->apiname, name);
		errno = EINVAL;
		return NULL;
	}

	/* makes the event name */
	plen = strlen(export->apiname);
	nlen = strlen(name);
	event = alloca(nlen + plen + 2);
	memcpy(event, export->apiname, plen);
	event[plen] = '/';
	memcpy(event + plen + 1, name, nlen + 1);

	/* create the event */
	return afb_evt_create_event(event);
}

static struct afb_event event_make_cb(void *closure, const char *name)
{
	struct afb_eventid *eventid = eventid_make_cb(closure, name);
	return (struct afb_event){ .itf = eventid ? eventid->itf : NULL, .closure = eventid };
}

static int event_broadcast_cb(void *closure, const char *name, struct json_object *object)
{
	size_t plen, nlen;
	char *event;
	struct afb_export *export = closure;

	/* check daemon state */
	if (export->state == Api_State_Pre_Init) {
		ERROR("[API %s] Bad call to 'afb_daemon_event_broadcast(%s, %s)', must not be in PreInit", export->apiname, name, json_object_to_json_string(object));
		errno = EINVAL;
		return 0;
	}

	/* makes the event name */
	plen = strlen(export->apiname);
	nlen = strlen(name);
	event = alloca(nlen + plen + 2);
	memcpy(event, export->apiname, plen);
	event[plen] = '/';
	memcpy(event + plen + 1, name, nlen + 1);

	/* broadcast the event */
	return afb_evt_broadcast(event, object);
}

static int rootdir_open_locale_cb(void *closure, const char *filename, int flags, const char *locale)
{
	return afb_common_rootdir_open_locale(filename, flags, locale);
}

static int queue_job_cb(void *closure, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout)
{
	return jobs_queue(group, timeout, callback, argument);
}

static struct afb_req unstore_req_cb(void *closure, struct afb_stored_req *sreq)
{
	return afb_xreq_unstore(sreq);
}

static int require_api_cb(void *closure, const char *name, int initialized)
{
	struct afb_export *export = closure;
	if (export->state != Api_State_Init) {
		ERROR("[API %s] Bad call to 'afb_daemon_require(%s, %d)', must be in Init", export->apiname, name, initialized);
		errno = EINVAL;
		return -1;
	}
	return -!(initialized ? afb_apiset_lookup_started : afb_apiset_lookup)(export->apiset, name, 1);
}

static int rename_api_cb(void *closure, const char *name)
{
	struct afb_export *export = closure;
	if (export->state != Api_State_Pre_Init) {
		ERROR("[API %s] Bad call to 'afb_daemon_rename(%s)', must be in PreInit", export->apiname, name);
		errno = EINVAL;
		return -1;
	}
	if (!afb_api_is_valid_name(name)) {
		ERROR("[API %s] Can't rename to %s: bad API name", export->apiname, name);
		errno = EINVAL;
		return -1;
	}
	NOTICE("[API %s] renamed to [API %s]", export->apiname, name);
	afb_export_rename(export, name);
	return 0;
}

static int api_new_api_cb(
		void *closure,
		const char *api,
		const char *info,
		int (*preinit)(void*, struct afb_dynapi *),
		void *preinit_closure)
{
	struct afb_export *export = closure;
	return afb_api_dyn_add(export->apiset, api, info, preinit, preinit_closure);
}

/**********************************************
* hooked flow
**********************************************/
static void hooked_vverbose_cb(void *closure, int level, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	struct afb_export *export = closure;
	va_list ap;
	va_copy(ap, args);
	vverbose_cb(closure, level, file, line, function, fmt, args);
	afb_hook_ditf_vverbose(export, level, file, line, function, fmt, ap);
	va_end(ap);
}

static void hooked_old_vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args)
{
	hooked_vverbose_cb(closure, level, file, line, NULL, fmt, args);
}

static struct afb_eventid *hooked_eventid_make_cb(void *closure, const char *name)
{
	struct afb_export *export = closure;
	struct afb_eventid *r = eventid_make_cb(closure, name);
	afb_hook_ditf_event_make(export, name, r);
	return r;
}

static struct afb_event hooked_event_make_cb(void *closure, const char *name)
{
	struct afb_eventid *eventid = hooked_eventid_make_cb(closure, name);
	return (struct afb_event){ .itf = eventid ? eventid->itf : NULL, .closure = eventid };
}

static int hooked_event_broadcast_cb(void *closure, const char *name, struct json_object *object)
{
	int r;
	struct afb_export *export = closure;
	json_object_get(object);
	afb_hook_ditf_event_broadcast_before(export, name, json_object_get(object));
	r = event_broadcast_cb(closure, name, object);
	afb_hook_ditf_event_broadcast_after(export, name, object, r);
	json_object_put(object);
	return r;
}

static struct sd_event *hooked_get_event_loop(void *closure)
{
	struct afb_export *export = closure;
	struct sd_event *r = afb_common_get_event_loop();
	return afb_hook_ditf_get_event_loop(export, r);
}

static struct sd_bus *hooked_get_user_bus(void *closure)
{
	struct afb_export *export = closure;
	struct sd_bus *r = afb_common_get_user_bus();
	return afb_hook_ditf_get_user_bus(export, r);
}

static struct sd_bus *hooked_get_system_bus(void *closure)
{
	struct afb_export *export = closure;
	struct sd_bus *r = afb_common_get_system_bus();
	return afb_hook_ditf_get_system_bus(export, r);
}

static int hooked_rootdir_get_fd(void *closure)
{
	struct afb_export *export = closure;
	int r = afb_common_rootdir_get_fd();
	return afb_hook_ditf_rootdir_get_fd(export, r);
}

static int hooked_rootdir_open_locale_cb(void *closure, const char *filename, int flags, const char *locale)
{
	struct afb_export *export = closure;
	int r = rootdir_open_locale_cb(closure, filename, flags, locale);
	return afb_hook_ditf_rootdir_open_locale(export, filename, flags, locale, r);
}

static int hooked_queue_job_cb(void *closure, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout)
{
	struct afb_export *export = closure;
	int r = queue_job_cb(closure, callback, argument, group, timeout);
	return afb_hook_ditf_queue_job(export, callback, argument, group, timeout, r);
}

static struct afb_req hooked_unstore_req_cb(void *closure, struct afb_stored_req *sreq)
{
	struct afb_export *export = closure;
	afb_hook_ditf_unstore_req(export, sreq);
	return unstore_req_cb(closure, sreq);
}

static int hooked_require_api_cb(void *closure, const char *name, int initialized)
{
	int result;
	struct afb_export *export = closure;
	afb_hook_ditf_require_api(export, name, initialized);
	result = require_api_cb(closure, name, initialized);
	return afb_hook_ditf_require_api_result(export, name, initialized, result);
}

static int hooked_rename_api_cb(void *closure, const char *name)
{
	struct afb_export *export = closure;
	const char *oldname = export->apiname;
	int result = rename_api_cb(closure, name);
	return afb_hook_ditf_rename_api(export, oldname, name, result);
}

static int hooked_api_new_api_cb(
		void *closure,
		const char *api,
		const char *info,
		int (*preinit)(void*, struct afb_dynapi *),
		void *preinit_closure)
{
	/* TODO */
	return api_new_api_cb(closure, api, info, preinit, preinit_closure);
}
/**********************************************
* vectors
**********************************************/
static const struct afb_daemon_itf daemon_itf = {
	.vverbose_v1 = old_vverbose_cb,
	.vverbose_v2 = vverbose_cb,
	.event_make = event_make_cb,
	.event_broadcast = event_broadcast_cb,
	.get_event_loop = afb_common_get_event_loop,
	.get_user_bus = afb_common_get_user_bus,
	.get_system_bus = afb_common_get_system_bus,
	.rootdir_get_fd = afb_common_rootdir_get_fd,
	.rootdir_open_locale = rootdir_open_locale_cb,
	.queue_job = queue_job_cb,
	.unstore_req = unstore_req_cb,
	.require_api = require_api_cb,
	.rename_api = rename_api_cb,
	.new_api = api_new_api_cb,
};

static const struct afb_daemon_itf hooked_daemon_itf = {
	.vverbose_v1 = hooked_old_vverbose_cb,
	.vverbose_v2 = hooked_vverbose_cb,
	.event_make = hooked_event_make_cb,
	.event_broadcast = hooked_event_broadcast_cb,
	.get_event_loop = hooked_get_event_loop,
	.get_user_bus = hooked_get_user_bus,
	.get_system_bus = hooked_get_system_bus,
	.rootdir_get_fd = hooked_rootdir_get_fd,
	.rootdir_open_locale = hooked_rootdir_open_locale_cb,
	.queue_job = hooked_queue_job_cb,
	.unstore_req = hooked_unstore_req_cb,
	.require_api = hooked_require_api_cb,
	.rename_api = hooked_rename_api_cb,
	.new_api = hooked_api_new_api_cb,
};

/*************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
                                           F R O M     S V C
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************/

/* the common session for services sharing their session */
static struct afb_session *common_session;

/*************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
                                           F R O M     S V C
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************/

/*
 * Structure for requests initiated by the service
 */
struct call_req
{
	struct afb_xreq xreq;

	struct afb_export *export;

	/* the args */
	union {
		void (*callback)(void*, int, struct json_object*);
		void (*callback_dynapi)(void*, int, struct json_object*, struct afb_dynapi*);
	};
	void *closure;

	/* sync */
	struct jobloop *jobloop;
	struct json_object *result;
	int status;
};

/*
 * destroys the call_req
 */
static void callreq_destroy(struct afb_xreq *xreq)
{
	struct call_req *callreq = CONTAINER_OF_XREQ(struct call_req, xreq);

	afb_context_disconnect(&callreq->xreq.context);
	json_object_put(callreq->xreq.json);
	afb_cred_unref(callreq->xreq.cred);
	free(callreq);
}

static void callreq_reply_async(struct afb_xreq *xreq, int status, json_object *obj)
{
	struct call_req *callreq = CONTAINER_OF_XREQ(struct call_req, xreq);
	if (callreq->callback)
		callreq->callback(callreq->closure, status, obj);
	json_object_put(obj);
}

static void callreq_reply_async_dynapi(struct afb_xreq *xreq, int status, json_object *obj)
{
	struct call_req *callreq = CONTAINER_OF_XREQ(struct call_req, xreq);
	if (callreq->callback_dynapi)
		callreq->callback_dynapi(callreq->closure, status, obj, to_dynapi(callreq->export));
	json_object_put(obj);
}

static void callreq_sync_leave(struct call_req *callreq)
{
	struct jobloop *jobloop = callreq->jobloop;

	if (jobloop) {
		callreq->jobloop = NULL;
		jobs_leave(jobloop);
	}
}

static void callreq_reply_sync(struct afb_xreq *xreq, int status, json_object *obj)
{
	struct call_req *callreq = CONTAINER_OF_XREQ(struct call_req, xreq);
	callreq->status = status;
	callreq->result = obj;
	callreq_sync_leave(callreq);
}

static void callreq_sync_enter(int signum, void *closure, struct jobloop *jobloop)
{
	struct call_req *callreq = closure;

	if (!signum) {
		callreq->jobloop = jobloop;
		afb_xreq_process(&callreq->xreq, callreq->export->apiset);
	} else {
		callreq->result = afb_msg_json_internal_error();
		callreq->status = -1;
		callreq_sync_leave(callreq);
	}
}

/* interface for requests of services */
const struct afb_xreq_query_itf afb_export_xreq_async_itf = {
	.unref = callreq_destroy,
	.reply = callreq_reply_async
};

/* interface for requests of services */
const struct afb_xreq_query_itf afb_export_xreq_async_dynapi_itf = {
	.unref = callreq_destroy,
	.reply = callreq_reply_async_dynapi
};

/* interface for requests of services */
const struct afb_xreq_query_itf afb_export_xreq_sync_itf = {
	.unref = callreq_destroy,
	.reply = callreq_reply_sync
};

/*
 * create an call_req
 */
static struct call_req *callreq_create(
		struct afb_export *export,
		const char *api,
		const char *verb,
		struct json_object *args,
		const struct afb_xreq_query_itf *itf)
{
	struct call_req *callreq;
	size_t lenapi, lenverb;
	char *copy;

	/* allocates the request */
	lenapi = 1 + strlen(api);
	lenverb = 1 + strlen(verb);
	callreq = malloc(lenapi + lenverb + sizeof *callreq);
	if (callreq != NULL) {
		/* initialises the request */
		afb_xreq_init(&callreq->xreq, itf);
		afb_context_init(&callreq->xreq.context, export->session, NULL);
		callreq->xreq.context.validated = 1;
		copy = (char*)&callreq[1];
		memcpy(copy, api, lenapi);
		callreq->xreq.api = copy;
		copy = &copy[lenapi];
		memcpy(copy, verb, lenverb);
		callreq->xreq.verb = copy;
		callreq->xreq.listener = export->listener;
		callreq->xreq.json = args;
		callreq->export = export;
	}
	return callreq;
}

/*
 * Initiates a call for the service
 */
static void svc_call(
		void *closure,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*),
		void *cbclosure)
{
	struct afb_export *export = closure;
	struct call_req *callreq;
	struct json_object *ierr;

	/* allocates the request */
	callreq = callreq_create(export, api, verb, args, &afb_export_xreq_async_itf);
	if (callreq == NULL) {
		ERROR("out of memory");
		json_object_put(args);
		ierr = afb_msg_json_internal_error();
		if (callback)
			callback(cbclosure, -1, ierr);
		json_object_put(ierr);
		return;
	}

	/* initialises the request */
	callreq->jobloop = NULL;
	callreq->callback = callback;
	callreq->closure = cbclosure;

	/* terminates and frees ressources if needed */
	afb_xreq_process(&callreq->xreq, export->apiset);
}

static void svc_call_dynapi(
		struct afb_dynapi *dynapi,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*, struct afb_dynapi*),
		void *cbclosure)
{
	struct afb_export *export = from_dynapi(dynapi);
	struct call_req *callreq;
	struct json_object *ierr;

	/* allocates the request */
	callreq = callreq_create(export, api, verb, args, &afb_export_xreq_async_dynapi_itf);
	if (callreq == NULL) {
		ERROR("out of memory");
		json_object_put(args);
		ierr = afb_msg_json_internal_error();
		if (callback)
			callback(cbclosure, -1, ierr, to_dynapi(export));
		json_object_put(ierr);
		return;
	}

	/* initialises the request */
	callreq->jobloop = NULL;
	callreq->callback_dynapi = callback;
	callreq->closure = cbclosure;

	/* terminates and frees ressources if needed */
	afb_xreq_process(&callreq->xreq, export->apiset);
}

static int svc_call_sync(
		void *closure,
		const char *api,
		const char *verb,
		struct json_object *args,
		struct json_object **result)
{
	struct afb_export *export = closure;
	struct call_req *callreq;
	struct json_object *resu;
	int rc;

	/* allocates the request */
	callreq = callreq_create(export, api, verb, args, &afb_export_xreq_sync_itf);
	if (callreq == NULL) {
		ERROR("out of memory");
		errno = ENOMEM;
		json_object_put(args);
		resu = afb_msg_json_internal_error();
		rc = -1;
	} else {
		/* initialises the request */
		callreq->jobloop = NULL;
		callreq->callback = NULL;
		callreq->result = NULL;
		callreq->status = 0;
		afb_xreq_unhooked_addref(&callreq->xreq); /* avoid early callreq destruction */
		rc = jobs_enter(NULL, 0, callreq_sync_enter, callreq);
		if (rc >= 0)
			rc = callreq->status;
		resu = (rc >= 0 || callreq->result) ? callreq->result : afb_msg_json_internal_error();
		afb_xreq_unhooked_unref(&callreq->xreq);
	}
	if (result)
		*result = resu;
	else
		json_object_put(resu);
	return rc;
}

struct hooked_call
{
	struct afb_export *export;
	union {
		void (*callback)(void*, int, struct json_object*);
		void (*callback_dynapi)(void*, int, struct json_object*, struct afb_dynapi*);
	};
	void *cbclosure;
};

static void svc_hooked_call_result(void *closure, int status, struct json_object *result)
{
	struct hooked_call *hc = closure;
	afb_hook_svc_call_result(hc->export, status, result);
	hc->callback(hc->cbclosure, status, result);
	free(hc);
}

static void svc_hooked_call_dynapi_result(void *closure, int status, struct json_object *result, struct afb_dynapi *dynapi)
{
	struct hooked_call *hc = closure;
	afb_hook_svc_call_result(hc->export, status, result);
	hc->callback_dynapi(hc->cbclosure, status, result, dynapi);
	free(hc);
}

static void svc_hooked_call(
		void *closure,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*),
		void *cbclosure)
{
	struct afb_export *export = closure;
	struct hooked_call *hc;

	if (export->hooksvc & afb_hook_flag_svc_call)
		afb_hook_svc_call(export, api, verb, args);

	if (export->hooksvc & afb_hook_flag_svc_call_result) {
		hc = malloc(sizeof *hc);
		if (!hc)
			WARNING("allocation failed");
		else {
			hc->export = export;
			hc->callback = callback;
			hc->cbclosure = cbclosure;
			callback = svc_hooked_call_result;
			cbclosure = hc;
		}
	}
	svc_call(closure, api, verb, args, callback, cbclosure);
}

static void svc_hooked_call_dynapi(
		struct afb_dynapi *dynapi,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*, struct afb_dynapi*),
		void *cbclosure)
{
	struct afb_export *export = from_dynapi(dynapi);
	struct hooked_call *hc;

	if (export->hooksvc & afb_hook_flag_svc_call)
		afb_hook_svc_call(export, api, verb, args);

	if (export->hooksvc & afb_hook_flag_svc_call_result) {
		hc = malloc(sizeof *hc);
		if (!hc)
			WARNING("allocation failed");
		else {
			hc->export = export;
			hc->callback_dynapi = callback;
			hc->cbclosure = cbclosure;
			callback = svc_hooked_call_dynapi_result;
			cbclosure = hc;
		}
	}
	svc_call_dynapi(dynapi, api, verb, args, callback, cbclosure);
}

static int svc_hooked_call_sync(
		void *closure,
		const char *api,
		const char *verb,
		struct json_object *args,
		struct json_object **result)
{
	struct afb_export *export = closure;
	struct json_object *resu;
	int rc;

	if (export->hooksvc & afb_hook_flag_svc_callsync)
		afb_hook_svc_callsync(export, api, verb, args);

	rc = svc_call_sync(closure, api, verb, args, &resu);

	if (export->hooksvc & afb_hook_flag_svc_callsync_result)
		afb_hook_svc_callsync_result(export, rc, resu);

	if (result)
		*result = resu;
	else
		json_object_put(resu);

	return rc;
}

/* the interface for services */
static const struct afb_service_itf service_itf = {
	.call = svc_call,
	.call_sync = svc_call_sync
};

/* the interface for services */
static const struct afb_service_itf hooked_service_itf = {
	.call = svc_hooked_call,
	.call_sync = svc_hooked_call_sync
};

/*************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
                                           F R O M     D Y N A P I
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************/

static int api_set_verbs_v2_cb(
		struct afb_dynapi *dynapi,
		const struct afb_verb_v2 *verbs)
{
	struct afb_export *export = from_dynapi(dynapi);

	if (export->apidyn) {
		afb_api_dyn_set_verbs_v2(export->apidyn, verbs);
		return 0;
	}

	errno = EPERM;
	return -1;
}

static int api_add_verb_cb(
		struct afb_dynapi *dynapi,
		const char *verb,
		const char *info,
		void (*callback)(struct afb_request *request),
		void *vcbdata,
		const struct afb_auth *auth,
		uint32_t session)
{
	struct afb_export *export = from_dynapi(dynapi);

	if (export->apidyn)
		return afb_api_dyn_add_verb(export->apidyn, verb, info, callback, vcbdata, auth, session);

	errno = EPERM;
	return -1;
}

static int api_sub_verb_cb(
		struct afb_dynapi *dynapi,
		const char *verb)
{
	struct afb_export *export = from_dynapi(dynapi);

	if (export->apidyn)
		return afb_api_dyn_sub_verb(export->apidyn, verb);

	errno = EPERM;
	return -1;
}

static int api_set_on_event_cb(
		struct afb_dynapi *dynapi,
		void (*onevent)(struct afb_dynapi *dynapi, const char *event, struct json_object *object))
{
	struct afb_export *export = from_dynapi(dynapi);
	return afb_export_handle_events_vdyn(export, onevent);
}

static int api_set_on_init_cb(
		struct afb_dynapi *dynapi,
		int (*oninit)(struct afb_dynapi *dynapi))
{
	struct afb_export *export = from_dynapi(dynapi);

	return afb_export_handle_init_vdyn(export, oninit);
}

static void api_seal_cb(
		struct afb_dynapi *dynapi)
{
	struct afb_export *export = from_dynapi(dynapi);

	export->apidyn = NULL;
}

static int hooked_api_set_verbs_v2_cb(
		struct afb_dynapi *dynapi,
		const struct afb_verb_v2 *verbs)
{
	/* TODO */
	return api_set_verbs_v2_cb(dynapi, verbs);
}

static int hooked_api_add_verb_cb(
		struct afb_dynapi *dynapi,
		const char *verb,
		const char *info,
		void (*callback)(struct afb_request *request),
		void *vcbdata,
		const struct afb_auth *auth,
		uint32_t session)
{
	/* TODO */
	return api_add_verb_cb(dynapi, verb, info, callback, vcbdata, auth, session);
}

static int hooked_api_sub_verb_cb(
		struct afb_dynapi *dynapi,
		const char *verb)
{
	/* TODO */
	return api_sub_verb_cb(dynapi, verb);
}

static int hooked_api_set_on_event_cb(
		struct afb_dynapi *dynapi,
		void (*onevent)(struct afb_dynapi *dynapi, const char *event, struct json_object *object))
{
	/* TODO */
	return api_set_on_event_cb(dynapi, onevent);
}

static int hooked_api_set_on_init_cb(
		struct afb_dynapi *dynapi,
		int (*oninit)(struct afb_dynapi *dynapi))
{
	/* TODO */
	return api_set_on_init_cb(dynapi, oninit);
}

static void hooked_api_seal_cb(
		struct afb_dynapi *dynapi)
{
	/* TODO */
	api_seal_cb(dynapi);
}

static const struct afb_dynapi_itf dynapi_itf = {

	.vverbose = (void*)vverbose_cb,

	.get_event_loop = afb_common_get_event_loop,
	.get_user_bus = afb_common_get_user_bus,
	.get_system_bus = afb_common_get_system_bus,
	.rootdir_get_fd = afb_common_rootdir_get_fd,
	.rootdir_open_locale = rootdir_open_locale_cb,
	.queue_job = queue_job_cb,

	.require_api = require_api_cb,
	.rename_api = rename_api_cb,

	.event_broadcast = event_broadcast_cb,
	.eventid_make = eventid_make_cb,

	.call = svc_call_dynapi,
	.call_sync = svc_call_sync,

	.api_new_api = api_new_api_cb,
	.api_set_verbs_v2 = api_set_verbs_v2_cb,
	.api_add_verb = api_add_verb_cb,
	.api_sub_verb = api_sub_verb_cb,
	.api_set_on_event = api_set_on_event_cb,
	.api_set_on_init = api_set_on_init_cb,
	.api_seal = api_seal_cb,
};

static const struct afb_dynapi_itf hooked_dynapi_itf = {

	.vverbose = hooked_vverbose_cb,

	.get_event_loop = hooked_get_event_loop,
	.get_user_bus = hooked_get_user_bus,
	.get_system_bus = hooked_get_system_bus,
	.rootdir_get_fd = hooked_rootdir_get_fd,
	.rootdir_open_locale = hooked_rootdir_open_locale_cb,
	.queue_job = hooked_queue_job_cb,

	.require_api = hooked_require_api_cb,
	.rename_api = hooked_rename_api_cb,

	.event_broadcast = hooked_event_broadcast_cb,
	.eventid_make = hooked_eventid_make_cb,

	.call = svc_hooked_call_dynapi,
	.call_sync = svc_hooked_call_sync,

	.api_new_api = hooked_api_new_api_cb,
	.api_set_verbs_v2 = hooked_api_set_verbs_v2_cb,
	.api_add_verb = hooked_api_add_verb_cb,
	.api_sub_verb = hooked_api_sub_verb_cb,
	.api_set_on_event = hooked_api_set_on_event_cb,
	.api_set_on_init = hooked_api_set_on_init_cb,
	.api_seal = hooked_api_seal_cb,
};

/*************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
                                           F R O M     S V C
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************/

/*
 * Propagates the event to the service
 */
static void export_on_event_v12(void *closure, const char *event, int eventid, struct json_object *object)
{
	struct afb_export *export = closure;

	if (export->hooksvc & afb_hook_flag_svc_on_event_before)
		afb_hook_svc_on_event_before(export, event, eventid, object);
	export->on_event.v12(event, object);
	if (export->hooksvc & afb_hook_flag_svc_on_event_after)
		afb_hook_svc_on_event_after(export, event, eventid, object);
	json_object_put(object);
}

/* the interface for events */
static const struct afb_evt_itf evt_v12_itf = {
	.broadcast = export_on_event_v12,
	.push = export_on_event_v12
};

/*
 * Propagates the event to the service
 */
static void export_on_event_vdyn(void *closure, const char *event, int eventid, struct json_object *object)
{
	struct afb_export *export = closure;

	if (export->hooksvc & afb_hook_flag_svc_on_event_before)
		afb_hook_svc_on_event_before(export, event, eventid, object);
	export->on_event.vdyn(to_dynapi(export), event, object);
	if (export->hooksvc & afb_hook_flag_svc_on_event_after)
		afb_hook_svc_on_event_after(export, event, eventid, object);
	json_object_put(object);
}

/* the interface for events */
static const struct afb_evt_itf evt_vdyn_itf = {
	.broadcast = export_on_event_vdyn,
	.push = export_on_event_vdyn
};

/*************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
                                           M E R G E D
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************/

static struct afb_export *create(struct afb_apiset *apiset, const char *apiname, enum afb_api_version version)
{
	struct afb_export *export;

	/* session shared with other exports */
	if (common_session == NULL) {
		common_session = afb_session_create (NULL, 0);
		if (common_session == NULL)
			return NULL;
	}
	export = calloc(1, sizeof *export);
	if (!export)
		errno = ENOMEM;
	else {
		memset(export, 0, sizeof *export);
		export->apiname = strdup(apiname);
		export->version = version;
		export->state = Api_State_Pre_Init;
		export->session = afb_session_addref(common_session);
		export->apiset = afb_apiset_addref(apiset);
	}
	return export;
}

void afb_export_destroy(struct afb_export *export)
{
	if (export) {
		if (export->listener != NULL)
			afb_evt_listener_unref(export->listener);
		afb_session_unref(export->session);
		afb_apiset_unref(export->apiset);
		free(export->apiname);
		free(export);
	}
}

struct afb_export *afb_export_create_v1(struct afb_apiset *apiset, const char *apiname, int (*init)(struct afb_service), void (*onevent)(const char*, struct json_object*))
{
	struct afb_export *export = create(apiset, apiname, Api_Version_1);
	if (export) {
		export->init.v1 = init;
		export->on_event.v12 = onevent;
		export->export.v1.mode = AFB_MODE_LOCAL;
		export->export.v1.daemon.closure = export;
		afb_export_verbosity_set(export, verbosity);
		afb_export_update_hook(export);
	}
	return export;
}

struct afb_export *afb_export_create_v2(struct afb_apiset *apiset, const char *apiname, struct afb_binding_data_v2 *data, int (*init)(), void (*onevent)(const char*, struct json_object*))
{
	struct afb_export *export = create(apiset, apiname, Api_Version_2);
	if (export) {
		export->init.v2 = init;
		export->on_event.v12 = onevent;
		export->export.v2 = data;
		data->daemon.closure = export;
		data->service.closure = export;
		afb_export_verbosity_set(export, verbosity);
		afb_export_update_hook(export);
	}
	return export;
}

struct afb_export *afb_export_create_vdyn(struct afb_apiset *apiset, const char *apiname, struct afb_api_dyn *apidyn)
{
	struct afb_export *export = create(apiset, apiname, Api_Version_Dyn);
	if (export) {
		export->apidyn = apidyn;
		afb_export_verbosity_set(export, verbosity);
		afb_export_update_hook(export);
	}
	return export;
}

void afb_export_rename(struct afb_export *export, const char *apiname)
{
	free(export->apiname);
	export->apiname = strdup(apiname);
	afb_export_update_hook(export);
}

const char *afb_export_apiname(const struct afb_export *export)
{
	return export->apiname;
}

void afb_export_update_hook(struct afb_export *export)
{
	export->hookditf = afb_hook_flags_ditf(export->apiname);
	export->hooksvc = afb_hook_flags_svc(export->apiname);
	export->dynapi.itf = export->hookditf|export->hooksvc ? &hooked_dynapi_itf : &dynapi_itf;

	switch (export->version) {
	case Api_Version_1:
		export->export.v1.daemon.itf = export->hookditf ? &hooked_daemon_itf : &daemon_itf;
		break;
	case Api_Version_2:
		export->export.v2->daemon.itf = export->hookditf ? &hooked_daemon_itf : &daemon_itf;
		export->export.v2->service.itf = export->hooksvc ? &hooked_service_itf : &service_itf;
		break;
	}
}

struct afb_binding_interface_v1 *afb_export_get_interface_v1(struct afb_export *export)
{
	return export->version == Api_Version_1 ? &export->export.v1 : NULL;
}

int afb_export_unshare_session(struct afb_export *export)
{
	if (export->session == common_session) {
		export->session = afb_session_create (NULL, 0);
		if (export->session)
			afb_session_unref(common_session);
		else {
			export->session = common_session;
			return -1;
		}
	}
	return 0;
}

void afb_export_set_apiset(struct afb_export *export, struct afb_apiset *apiset)
{
	struct afb_apiset *prvset = export->apiset;
	export->apiset = afb_apiset_addref(apiset);
	afb_apiset_unref(prvset);
}

struct afb_apiset *afb_export_get_apiset(struct afb_export *export)
{
	return export->apiset;
}

int afb_export_handle_events_v12(struct afb_export *export, void (*on_event)(const char *event, struct json_object *object))
{
	/* check version */
	switch (export->version) {
	case Api_Version_1: case Api_Version_2: break;
	default:
		ERROR("invalid version 12 for API %s", export->apiname);
		errno = EINVAL;
		return -1;
	}

	/* set the event handler */
	if (!on_event) {
		if (export->listener) {
			afb_evt_listener_unref(export->listener);
			export->listener = NULL;
		}
		export->on_event.v12 = on_event;
	} else {
		export->on_event.v12 = on_event;
		if (!export->listener) {
			export->listener = afb_evt_listener_create(&evt_v12_itf, export);
			if (export->listener == NULL)
				return -1;
		}
	}
	return 0;
}

int afb_export_handle_events_vdyn(struct afb_export *export, void (*on_event)(struct afb_dynapi *dynapi, const char *event, struct json_object *object))
{
	/* check version */
	switch (export->version) {
	case Api_Version_Dyn: break;
	default:
		ERROR("invalid version Dyn for API %s", export->apiname);
		errno = EINVAL;
		return -1;
	}

	/* set the event handler */
	if (!on_event) {
		if (export->listener) {
			afb_evt_listener_unref(export->listener);
			export->listener = NULL;
		}
		export->on_event.vdyn = on_event;
	} else {
		export->on_event.vdyn = on_event;
		if (!export->listener) {
			export->listener = afb_evt_listener_create(&evt_vdyn_itf, export);
			if (export->listener == NULL)
				return -1;
		}
	}
	return 0;
}

int afb_export_handle_init_vdyn(struct afb_export *export, int (*oninit)(struct afb_dynapi *dynapi))
{
	if (export->state != Api_State_Pre_Init) {
		ERROR("[API %s] Bad call to 'afb_dynapi_on_init', must be in PreInit", export->apiname);
		errno = EINVAL;
		return -1;
	}

	export->init.vdyn  = oninit;
	return 0;
}

/*
 * Starts a new service (v1)
 */
struct afb_binding_v1 *afb_export_register_v1(struct afb_export *export, struct afb_binding_v1 *(*regfun)(const struct afb_binding_interface_v1*))
{
	return regfun(&export->export.v1);
}

int afb_export_preinit_vdyn(struct afb_export *export, int (*preinit)(void*, struct afb_dynapi*), void *closure)
{
	return preinit(closure, to_dynapi(export));
}

int afb_export_verbosity_get(const struct afb_export *export)
{
	return export->dynapi.verbosity;
}

void afb_export_verbosity_set(struct afb_export *export, int level)
{
	export->dynapi.verbosity = level;
	switch (export->version) {
	case Api_Version_1: export->export.v1.verbosity = level; break;
	case Api_Version_2: export->export.v2->verbosity = level; break;
	}
}

/*************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
                                           N E W
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************/

int afb_export_start(struct afb_export *export, int share_session, int onneed, struct afb_apiset *apiset)
{
	int rc;

	/* check state */
	if (export->state != Api_State_Pre_Init) {
		/* not an error when onneed */
		if (onneed != 0)
			goto done;

		/* already started: it is an error */
		ERROR("Service of API %s already started", export->apiname);
		return -1;
	}

	/* unshare the session if asked */
	if (!share_session) {
		rc = afb_export_unshare_session(export);
		if (rc < 0) {
			ERROR("Can't unshare the session for %s", export->apiname);
			return -1;
		}
	}

	/* set event handling */
	switch (export->version) {
	case Api_Version_1:
	case Api_Version_2:
		rc = afb_export_handle_events_v12(export, export->on_event.v12);
		break;
	default:
		rc = 0;
		break;
	}
	if (rc < 0) {
		ERROR("Can't set event handler for %s", export->apiname);
		return -1;
	}

	/* Starts the service */
	if (export->hooksvc & afb_hook_flag_svc_start_before)
		afb_hook_svc_start_before(export);
	export->state = Api_State_Init;
	switch (export->version) {
	case Api_Version_1:
		rc = export->init.v1 ? export->init.v1((struct afb_service){ .itf = &hooked_service_itf, .closure = export }) : 0;
		break;
	case Api_Version_2:
		rc = export->init.v2 ? export->init.v2() : 0;
		break;
	case Api_Version_Dyn:
		rc = export->init.vdyn ? export->init.vdyn(to_dynapi(export)) : 0;
		break;
	default:
		break;
	}
	export->state = Api_State_Run;
	if (export->hooksvc & afb_hook_flag_svc_start_after)
		afb_hook_svc_start_after(export, rc);
	if (rc < 0) {
		/* initialisation error */
		ERROR("Initialisation of service API %s failed (%d): %m", export->apiname, rc);
		return rc;
	}

done:
	return 0;
}

