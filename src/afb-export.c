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

#include <afb/afb-binding-v1.h>
#include <afb/afb-binding-v2.h>

#include "afb-api.h"
#include "afb-apiset.h"
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

extern struct afb_apiset *main_apiset;

/*************************************************************************
 * internal types and structures
 ************************************************************************/

enum afb_api_version
{
	Api_Version_None = 0,
	Api_Version_1 = 1,
	Api_Version_2 = 2,
	Api_Version_3 = 3
};

enum afb_api_state
{
	Api_State_Pre_Init,
	Api_State_Init,
	Api_State_Run
};

struct afb_export
{
	/* name of the api */
	char *apiname;

	/* version of the api */
	unsigned version: 4;

	/* current state */
	unsigned state: 4;

	/* hooking flags */
	int hookditf;
	int hooksvc;
	
	/* session for service */
	struct afb_session *session;

	/* apiset for service */
	struct afb_apiset *apiset;

	/* event listener for service or NULL */
	struct afb_evt_listener *listener;

	/* event callback for service */
	void (*on_event)(const char *event, struct json_object *object);

	/* exported data */
	union {
		struct afb_binding_interface_v1 v1;
		struct afb_binding_data_v2 *v2;
	} export;
};

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

static struct afb_event event_make_cb(void *closure, const char *name)
{
	size_t plen, nlen;
	char *event;
	struct afb_export *export = closure;

	/* check daemon state */
	if (export->state == Api_State_Pre_Init) {
		ERROR("[API %s] Bad call to 'afb_daemon_event_make(%s)', must not be in PreInit", export->apiname, name);
		errno = EINVAL;
		return (struct afb_event){ .itf = NULL, .closure = NULL };
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
	return -!(initialized ? afb_apiset_lookup_started : afb_apiset_lookup)(main_apiset, name, 1);
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

static struct afb_event hooked_event_make_cb(void *closure, const char *name)
{
	struct afb_export *export = closure;
	struct afb_event r = event_make_cb(closure, name);
	return afb_hook_ditf_event_make(export, name, r);
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
	.rename_api = rename_api_cb
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
	.rename_api = hooked_rename_api_cb
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
	void (*callback)(void*, int, struct json_object*);
	void *closure;

	/* sync */
	struct jobloop *jobloop;
	struct json_object *result;
	int status;
	int async;
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

static void callreq_reply(struct afb_xreq *xreq, int status, json_object *obj)
{
	struct call_req *callreq = CONTAINER_OF_XREQ(struct call_req, xreq);
	if (callreq->callback)
		callreq->callback(callreq->closure, status, obj);
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
const struct afb_xreq_query_itf afb_export_xreq_itf = {
	.unref = callreq_destroy,
	.reply = callreq_reply
};

/* interface for requests of services */
const struct afb_xreq_query_itf afb_export_xreq_sync_itf = {
	.unref = callreq_destroy,
	.reply = callreq_reply_sync
};

/*
 * create an call_req
 */
static struct call_req *callreq_create(struct afb_export *export, const char *api, const char *verb, struct json_object *args, const struct afb_xreq_query_itf *itf)
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
static void svc_call(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cbclosure)
{
	struct afb_export *export = closure;
	struct call_req *callreq;
	struct json_object *ierr;

	/* allocates the request */
	callreq = callreq_create(export, api, verb, args, &afb_export_xreq_itf);
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
	callreq->async = 1;

	/* terminates and frees ressources if needed */
	afb_xreq_process(&callreq->xreq, export->apiset);
}

static int svc_call_sync(void *closure, const char *api, const char *verb, struct json_object *args,
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
		callreq->async = 0;
		afb_xreq_addref(&callreq->xreq);
		rc = jobs_enter(NULL, 0, callreq_sync_enter, callreq);
		if (rc >= 0)
			rc = callreq->status;
		resu = (rc >= 0 || callreq->result) ? callreq->result : afb_msg_json_internal_error();
		afb_xreq_unref(&callreq->xreq);
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
	void (*callback)(void*, int, struct json_object*);
	void *cbclosure;
};

static void svc_hooked_call_result(void *closure, int status, struct json_object *result)
{
	struct hooked_call *hc = closure;
	afb_hook_svc_call_result(hc->export, status, result);
	hc->callback(hc->cbclosure, status, result);
	free(hc);
}

static void svc_hooked_call(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cbclosure)
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

static int svc_hooked_call_sync(void *closure, const char *api, const char *verb, struct json_object *args,
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
                                           F R O M     S V C
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************
 *************************************************************************************************************/

/*
 * Propagates the event to the service
 */
static void export_on_event(void *closure, const char *event, int eventid, struct json_object *object)
{
	struct afb_export *export = closure;

	if (export->hooksvc & afb_hook_flag_svc_on_event_before)
		afb_hook_svc_on_event_before(export, event, eventid, object);
	export->on_event(event, object);
	if (export->hooksvc & afb_hook_flag_svc_on_event_after)
		afb_hook_svc_on_event_after(export, event, eventid, object);
	json_object_put(object);
}

/* the interface for events */
static const struct afb_evt_itf evt_itf = {
	.broadcast = export_on_event,
	.push = export_on_event
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

static struct afb_export *create(const char *apiname, enum afb_api_version version)
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
		export->apiset = afb_apiset_addref(main_apiset);
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

struct afb_export *afb_export_create_v1(const char *apiname)
{
	struct afb_export *export = create(apiname, Api_Version_1);
	if (export) {
		export->export.v1.verbosity = verbosity;
		export->export.v1.mode = AFB_MODE_LOCAL;
		export->export.v1.daemon.closure = export;
		afb_export_update_hook(export);
	}
	return export;
}

struct afb_export *afb_export_create_v2(const char *apiname, struct afb_binding_data_v2 *data)
{
	struct afb_export *export = create(apiname, Api_Version_2);
	if (export) {
		export->export.v2 = data;
		data->daemon.closure = export;
		data->service.closure = export;
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
	switch (export->version) {
	case Api_Version_1:
		export->export.v1.daemon.itf = export->hookditf ? &hooked_daemon_itf : &daemon_itf;
		break;
	default:
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
	
/*
 * Creates a new service
 */
int afb_export_handle_events(struct afb_export *export, void (*on_event)(const char *event, struct json_object *object))
{
	if (on_event != export->on_event) {
		if (!on_event) {
			afb_evt_listener_unref(export->listener);
			export->listener = NULL;
		} else if (!export->listener) {
			export->listener = afb_evt_listener_create(&evt_itf, export);
			if (export->listener == NULL)
				return -1;
		}
		export->on_event = on_event;
	}
	return 0;
}



int afb_export_is_started(const struct afb_export *export)
{
	return export->state != Api_State_Pre_Init;
}


/*
 * Starts a new service (v1)
 */
struct afb_binding_v1 *afb_export_register_v1(struct afb_export *export, struct afb_binding_v1 *(*regfun)(const struct afb_binding_interface_v1*))
{
	return regfun(&export->export.v1);
	
}

int afb_export_start_v1(struct afb_export *export, int (*start)(struct afb_service))
{
	int rc;
	struct afb_service svc = { .itf = &hooked_service_itf, .closure = export };

	if (export->hooksvc & afb_hook_flag_svc_start_before)
		afb_hook_svc_start_before(export);
	export->state = Api_State_Init;
	rc = start ? start(svc) : 0;
	export->state = Api_State_Run;
	if (export->hooksvc & afb_hook_flag_svc_start_after)
		afb_hook_svc_start_after(export, rc);
	return rc;
}

/*
 * Starts a new service (v2)
 */
int afb_export_start_v2(struct afb_export *export, int (*start)())
{
	int rc;

	if (export->hooksvc & afb_hook_flag_svc_start_before)
		afb_hook_svc_start_before(export);
	export->state = Api_State_Init;
	rc = start ? start() : 0;
	export->state = Api_State_Run;
	if (export->hooksvc & afb_hook_flag_svc_start_after)
		afb_hook_svc_start_after(export, rc);
	if (rc >= 0)
		export->state = Api_State_Run;
	return rc;
}

int afb_export_verbosity_get(const struct afb_export *export)
{
	switch (export->version) {
	case Api_Version_1: return export->export.v1.verbosity;
	case Api_Version_2: return export->export.v2->verbosity;
	}
	return verbosity;
}

void afb_export_verbosity_set(struct afb_export *export, int level)
{
	switch (export->version) {
	case Api_Version_1: export->export.v1.verbosity = level; break;
	case Api_Version_2: export->export.v2->verbosity = level; break;
	}
}

