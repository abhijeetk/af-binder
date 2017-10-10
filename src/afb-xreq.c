/*
 * Copyright (C) 2017 "IoT.bzh"
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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <json-c/json.h>
#include <afb/afb-binding-v1.h>
#include <afb/afb-binding-v2.h>
#include <afb/afb-request.h>

#include "afb-context.h"
#include "afb-xreq.h"
#include "afb-evt.h"
#include "afb-msg-json.h"
#include "afb-cred.h"
#include "afb-hook.h"
#include "afb-api.h"
#include "afb-api-dyn.h"
#include "afb-apiset.h"
#include "afb-auth.h"
#include "jobs.h"
#include "verbose.h"

/******************************************************************************/

static void xreq_finalize(struct afb_xreq *xreq)
{
	if (!xreq->replied)
		afb_xreq_fail(xreq, "error", "no reply");
	if (xreq->hookflags)
		afb_hook_xreq_end(xreq);
	if (xreq->caller)
		afb_xreq_unhooked_unref(xreq->caller);
	xreq->queryitf->unref(xreq);
}

inline void afb_xreq_unhooked_addref(struct afb_xreq *xreq)
{
	__atomic_add_fetch(&xreq->refcount, 1, __ATOMIC_RELAXED);
}

inline void afb_xreq_unhooked_unref(struct afb_xreq *xreq)
{
	if (!__atomic_sub_fetch(&xreq->refcount, 1, __ATOMIC_RELAXED))
		xreq_finalize(xreq);
}

/******************************************************************************/

static inline struct afb_request *to_request(struct afb_xreq *xreq)
{
	return &xreq->request;
}

static inline struct afb_req to_req(struct afb_xreq *xreq)
{
	return (struct afb_req){ .itf = xreq->request.itf, .closure = &xreq->request };
}

static inline struct afb_xreq *from_request(struct afb_request *request)
{
	return CONTAINER_OF(struct afb_xreq, request, request);
}

/******************************************************************************/

struct subcall
{
	struct afb_xreq xreq;

	void (*completion)(struct subcall*, int, struct json_object*);

	union {
		struct {
			struct jobloop *jobloop;
			struct json_object *result;
			int status;
		};
		struct {
			union {
				void (*callback)(void*, int, struct json_object*);
				void (*callback_req)(void*, int, struct json_object*, struct afb_req);
				void (*callback_request)(void*, int, struct json_object*, struct afb_request*);
			};
			void *closure;
		};
	};
};

static int subcall_subscribe_cb(struct afb_xreq *xreq, struct afb_eventid *eventid)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	return afb_xreq_subscribe(subcall->xreq.caller, eventid);
}

static int subcall_unsubscribe_cb(struct afb_xreq *xreq, struct afb_eventid *eventid)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	return afb_xreq_unsubscribe(subcall->xreq.caller, eventid);
}

static void subcall_reply_cb(struct afb_xreq *xreq, int status, struct json_object *result)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	subcall->completion(subcall, status, result);
	json_object_put(result);
	afb_xreq_unhooked_unref(&subcall->xreq);
}

static void subcall_destroy_cb(struct afb_xreq *xreq)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	json_object_put(subcall->xreq.json);
	afb_cred_unref(subcall->xreq.cred);
	free(subcall);
}

const struct afb_xreq_query_itf afb_xreq_subcall_itf = {
	.reply = subcall_reply_cb,
	.unref = subcall_destroy_cb,
	.subscribe = subcall_subscribe_cb,
	.unsubscribe = subcall_unsubscribe_cb
};

static struct subcall *subcall_alloc(
		struct afb_xreq *caller,
		const char *api,
		const char *verb,
		struct json_object *args
)
{
	struct subcall *subcall;
	size_t lenapi, lenverb;
	char *copy;

	lenapi = 1 + strlen(api);
	lenverb = 1 + strlen(verb);
	subcall = malloc(lenapi + lenverb + sizeof *subcall);
	if (!subcall)
		ERROR("out of memory");
	else {
		copy = (char*)&subcall[1];
		memcpy(copy, api, lenapi);
		api = copy;
		copy = &copy[lenapi];
		memcpy(copy, verb, lenverb);
		verb = copy;

		afb_xreq_init(&subcall->xreq, &afb_xreq_subcall_itf);
		afb_context_subinit(&subcall->xreq.context, &caller->context);
		subcall->xreq.cred = afb_cred_addref(caller->cred);
		subcall->xreq.json = args;
		subcall->xreq.request.api = api;
		subcall->xreq.request.verb = verb;
		subcall->xreq.caller = caller;
		afb_xreq_unhooked_addref(caller);
	}
	return subcall;
}


static void subcall_on_reply(struct subcall *subcall, int status, struct json_object *result)
{
	subcall->callback(subcall->closure, status, result);
}

static void subcall_req_on_reply(struct subcall *subcall, int status, struct json_object *result)
{
	subcall->callback_req(subcall->closure, status, result, to_req(subcall->xreq.caller));
}

static void subcall_request_on_reply(struct subcall *subcall, int status, struct json_object *result)
{
	subcall->callback_request(subcall->closure, status, result, to_request(subcall->xreq.caller));
}

static void subcall_hooked_on_reply(struct subcall *subcall, int status, struct json_object *result)
{
	afb_hook_xreq_subcall_result(subcall->xreq.caller, status, result);
	subcall_on_reply(subcall, status, result);
}

static void subcall_req_hooked_on_reply(struct subcall *subcall, int status, struct json_object *result)
{
	afb_hook_xreq_subcall_req_result(subcall->xreq.caller, status, result);
	subcall_req_on_reply(subcall, status, result);
}

static void subcall_request_hooked_on_reply(struct subcall *subcall, int status, struct json_object *result)
{
	afb_hook_xreq_subcall_result(subcall->xreq.caller, status, result);
	subcall_request_on_reply(subcall, status, result);
}

static void subcall_reply_direct_cb(void *closure, int status, struct json_object *result)
{
	struct afb_xreq *xreq = closure;

	if (xreq->replied) {
		ERROR("subcall replied more than one time!!");
		json_object_put(result);
	} else {
		xreq->replied = 1;
		subcall_reply_cb(xreq, status, result);
	}
}

static void subcall_process(struct subcall *subcall, void (*completion)(struct subcall*, int, struct json_object*))
{
	subcall->completion = completion;
	if (subcall->xreq.caller->queryitf->subcall) {
		subcall->xreq.caller->queryitf->subcall(
			subcall->xreq.caller, subcall->xreq.request.api, subcall->xreq.request.verb,
			subcall->xreq.json, subcall_reply_direct_cb, &subcall->xreq);
	} else {
		afb_xreq_unhooked_addref(&subcall->xreq);
		afb_xreq_process(&subcall->xreq, subcall->xreq.caller->apiset);
	}
}

static void subcall(struct subcall *subcall, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	subcall->callback = callback;
	subcall->closure = cb_closure;
	subcall_process(subcall, subcall_on_reply);
}

static void subcall_req(struct subcall *subcall, void (*callback)(void*, int, struct json_object*, struct afb_req), void *cb_closure)
{
	subcall->callback_req = callback;
	subcall->closure = cb_closure;
	subcall_process(subcall, subcall_req_on_reply);
}

static void subcall_request(struct subcall *subcall, void (*callback)(void*, int, struct json_object*, struct afb_request*), void *cb_closure)
{
	subcall->callback_request = callback;
	subcall->closure = cb_closure;
	subcall_process(subcall, subcall_request_on_reply);
}

static void subcall_hooked(struct subcall *subcall, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	subcall->callback = callback;
	subcall->closure = cb_closure;
	subcall_process(subcall, subcall_hooked_on_reply);
}

static void subcall_req_hooked(struct subcall *subcall, void (*callback)(void*, int, struct json_object*, struct afb_req), void *cb_closure)
{
	subcall->callback_req = callback;
	subcall->closure = cb_closure;
	subcall_process(subcall, subcall_req_hooked_on_reply);
}

static void subcall_request_hooked(struct subcall *subcall, void (*callback)(void*, int, struct json_object*, struct afb_request*), void *cb_closure)
{
	subcall->callback_request = callback;
	subcall->closure = cb_closure;
	subcall_process(subcall, subcall_request_hooked_on_reply);
}

static void subcall_sync_leave(struct subcall *subcall)
{
	struct jobloop *jobloop = __atomic_exchange_n(&subcall->jobloop, NULL, __ATOMIC_RELAXED);
	if (jobloop)
		jobs_leave(jobloop);
}

static void subcall_sync_reply(struct subcall *subcall, int status, struct json_object *result)
{
	subcall->status = status;
	subcall->result = json_object_get(result);
	subcall_sync_leave(subcall);
}

static void subcall_sync_enter(int signum, void *closure, struct jobloop *jobloop)
{
	struct subcall *subcall = closure;

	if (!signum) {
		subcall->jobloop = jobloop;
		subcall->result = NULL;
		subcall->status = 0;
		subcall_process(subcall, subcall_sync_reply);
	} else {
		subcall->status = -1;
		subcall_sync_leave(subcall);
	}
}

static int subcallsync(struct subcall *subcall, struct json_object **result)
{
	int rc;

	afb_xreq_unhooked_addref(&subcall->xreq);
	rc = jobs_enter(NULL, 0, subcall_sync_enter, subcall);
	*result = subcall->result;
	if (rc < 0 || subcall->status < 0) {
		*result = *result ?: afb_msg_json_internal_error();
		rc = -1;
	}
	afb_xreq_unhooked_unref(&subcall->xreq);
	return rc;
}

/******************************************************************************/

static void vinfo(void *first, void *second, const char *fmt, va_list args, void (*fun)(void*,void*,const char*))
{
	char *info;
	if (fmt == NULL || vasprintf(&info, fmt, args) < 0)
		info = NULL;
	fun(first, second, info);
	free(info);
}

/******************************************************************************/

static struct json_object *xreq_json_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	if (!xreq->json && xreq->queryitf->json)
		xreq->json = xreq->queryitf->json(xreq);
	return xreq->json;
}

static struct afb_arg xreq_get_cb(struct afb_request *closure, const char *name)
{
	struct afb_xreq *xreq = from_request(closure);
	struct afb_arg arg;
	struct json_object *object, *value;

	if (xreq->queryitf->get)
		arg = xreq->queryitf->get(xreq, name);
	else {
		object = xreq_json_cb(closure);
		if (json_object_object_get_ex(object, name, &value)) {
			arg.name = name;
			arg.value = json_object_get_string(value);
		} else {
			arg.name = NULL;
			arg.value = NULL;
		}
		arg.path = NULL;
	}
	return arg;
}

static void xreq_success_cb(struct afb_request *closure, struct json_object *obj, const char *info)
{
	struct afb_xreq *xreq = from_request(closure);

	if (xreq->replied) {
		ERROR("reply called more than one time!!");
		json_object_put(obj);
	} else {
		xreq->replied = 1;
		if (xreq->queryitf->success)
			xreq->queryitf->success(xreq, obj, info);
		else
			xreq->queryitf->reply(xreq, 0, afb_msg_json_reply_ok(info, obj, &xreq->context, NULL));
	}
}

static void xreq_fail_cb(struct afb_request *closure, const char *status, const char *info)
{
	struct afb_xreq *xreq = from_request(closure);

	if (xreq->replied) {
		ERROR("reply called more than one time!!");
	} else {
		xreq->replied = 1;
		if (xreq->queryitf->fail)
			xreq->queryitf->fail(xreq, status, info);
		else
			xreq->queryitf->reply(xreq, -1, afb_msg_json_reply_error(status, info, &xreq->context, NULL));
	}
}

static void xreq_vsuccess_cb(struct afb_request *closure, struct json_object *obj, const char *fmt, va_list args)
{
	vinfo(closure, obj, fmt, args, (void*)xreq_success_cb);
}

static void xreq_vfail_cb(struct afb_request *closure, const char *status, const char *fmt, va_list args)
{
	vinfo(closure, (void*)status, fmt, args, (void*)xreq_fail_cb);
}

static void *xreq_context_get_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	return afb_context_get(&xreq->context);
}

static void xreq_context_set_cb(struct afb_request *closure, void *value, void (*free_value)(void*))
{
	struct afb_xreq *xreq = from_request(closure);
	afb_context_set(&xreq->context, value, free_value);
}

static struct afb_request *xreq_addref_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	afb_xreq_unhooked_addref(xreq);
	return closure;
}

static void xreq_unref_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	afb_xreq_unhooked_unref(xreq);
}

static void xreq_session_close_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	afb_context_close(&xreq->context);
}

static int xreq_session_set_LOA_cb(struct afb_request *closure, unsigned level)
{
	struct afb_xreq *xreq = from_request(closure);
	return afb_context_change_loa(&xreq->context, level);
}

static int xreq_subscribe_eventid_cb(struct afb_request *closure, struct afb_eventid *eventid);
static int xreq_subscribe_cb(struct afb_request *closure, struct afb_event event)
{
	return xreq_subscribe_eventid_cb(closure, afb_event_to_eventid(event));
}

static int xreq_subscribe_eventid_cb(struct afb_request *closure, struct afb_eventid *eventid)
{
	struct afb_xreq *xreq = from_request(closure);
	return afb_xreq_subscribe(xreq, eventid);
}

int afb_xreq_subscribe(struct afb_xreq *xreq, struct afb_eventid *eventid)
{
	if (xreq->listener)
		return afb_evt_eventid_add_watch(xreq->listener, eventid);
	if (xreq->queryitf->subscribe)
		return xreq->queryitf->subscribe(xreq, eventid);
	ERROR("no event listener, subscription impossible");
	errno = EINVAL;
	return -1;
}

static int xreq_unsubscribe_eventid_cb(struct afb_request *closure, struct afb_eventid *eventid);
static int xreq_unsubscribe_cb(struct afb_request *closure, struct afb_event event)
{
	return xreq_unsubscribe_eventid_cb(closure, afb_event_to_eventid(event));
}

static int xreq_unsubscribe_eventid_cb(struct afb_request *closure, struct afb_eventid *eventid)
{
	struct afb_xreq *xreq = from_request(closure);
	return afb_xreq_unsubscribe(xreq, eventid);
}

int afb_xreq_unsubscribe(struct afb_xreq *xreq, struct afb_eventid *eventid)
{
	if (xreq->listener)
		return afb_evt_eventid_remove_watch(xreq->listener, eventid);
	if (xreq->queryitf->unsubscribe)
		return xreq->queryitf->unsubscribe(xreq, eventid);
	ERROR("no event listener, unsubscription impossible");
	errno = EINVAL;
	return -1;
}

static void xreq_subcall_cb(struct afb_request *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	struct afb_xreq *xreq = from_request(closure);
	struct subcall *sc;

	sc = subcall_alloc(xreq, api, verb, args);
	if (sc == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error());
		json_object_put(args);
	} else {
		subcall(sc, callback, cb_closure);
	}
}

static void xreq_subcall_req_cb(struct afb_request *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*, struct afb_req), void *cb_closure)
{
	struct afb_xreq *xreq = from_request(closure);
	struct subcall *sc;

	sc = subcall_alloc(xreq, api, verb, args);
	if (sc == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error(), to_req(xreq));
		json_object_put(args);
	} else {
		subcall_req(sc, callback, cb_closure);
	}
}

static void xreq_subcall_request_cb(struct afb_request *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*, struct afb_request*), void *cb_closure)
{
	struct afb_xreq *xreq = from_request(closure);
	struct subcall *sc;

	sc = subcall_alloc(xreq, api, verb, args);
	if (sc == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error(), to_request(xreq));
		json_object_put(args);
	} else {
		subcall_request(sc, callback, cb_closure);
	}
}


static int xreq_subcallsync_cb(struct afb_request *closure, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	int rc;
	struct subcall *sc;
	struct afb_xreq *xreq = from_request(closure);
	struct json_object *resu;

	sc = subcall_alloc(xreq, api, verb, args);
	if (!sc) {
		rc = -1;
		resu = afb_msg_json_internal_error();
		json_object_put(args);
	} else {
		rc = subcallsync(sc, &resu);
	}
	if (result)
		*result = resu;
	else
		json_object_put(resu);
	return rc;
}

static void xreq_vverbose_cb(struct afb_request *closure, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	char *p;
	struct afb_xreq *xreq = from_request(closure);

	if (!fmt || vasprintf(&p, fmt, args) < 0)
		vverbose(level, file, line, func, fmt, args);
	else {
		verbose(level, file, line, func, "[REQ/API %s] %s", xreq->request.api, p);
		free(p);
	}
}

static struct afb_stored_req *xreq_store_cb(struct afb_request *closure)
{
	xreq_addref_cb(closure);
	return (struct afb_stored_req*)closure;
}

static int xreq_has_permission_cb(struct afb_request *closure, const char *permission)
{
	struct afb_xreq *xreq = from_request(closure);
	return afb_auth_has_permission(xreq, permission);
}

static char *xreq_get_application_id_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	return xreq->cred && xreq->cred->id ? strdup(xreq->cred->id) : NULL;
}

static void *xreq_context_make_cb(struct afb_request *closure, int replace, void *(*create_value)(void*), void (*free_value)(void*), void *create_closure)
{
	struct afb_xreq *xreq = from_request(closure);
	return afb_context_make(&xreq->context, replace, create_value, free_value, create_closure);
}

static int xreq_get_uid_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	return xreq->cred && xreq->cred->id ? (int)xreq->cred->uid : -1;
}

/******************************************************************************/

static struct json_object *xreq_hooked_json_cb(struct afb_request *closure)
{
	struct json_object *r = xreq_json_cb(closure);
	struct afb_xreq *xreq = from_request(closure);
	return afb_hook_xreq_json(xreq, r);
}

static struct afb_arg xreq_hooked_get_cb(struct afb_request *closure, const char *name)
{
	struct afb_arg r = xreq_get_cb(closure, name);
	struct afb_xreq *xreq = from_request(closure);
	return afb_hook_xreq_get(xreq, name, r);
}

static void xreq_hooked_success_cb(struct afb_request *closure, struct json_object *obj, const char *info)
{
	struct afb_xreq *xreq = from_request(closure);
	afb_hook_xreq_success(xreq, obj, info);
	xreq_success_cb(closure, obj, info);
}

static void xreq_hooked_fail_cb(struct afb_request *closure, const char *status, const char *info)
{
	struct afb_xreq *xreq = from_request(closure);
	afb_hook_xreq_fail(xreq, status, info);
	xreq_fail_cb(closure, status, info);
}

static void xreq_hooked_vsuccess_cb(struct afb_request *closure, struct json_object *obj, const char *fmt, va_list args)
{
	vinfo(closure, obj, fmt, args, (void*)xreq_hooked_success_cb);
}

static void xreq_hooked_vfail_cb(struct afb_request *closure, const char *status, const char *fmt, va_list args)
{
	vinfo(closure, (void*)status, fmt, args, (void*)xreq_hooked_fail_cb);
}

static void *xreq_hooked_context_get_cb(struct afb_request *closure)
{
	void *r = xreq_context_get_cb(closure);
	struct afb_xreq *xreq = from_request(closure);
	return afb_hook_xreq_context_get(xreq, r);
}

static void xreq_hooked_context_set_cb(struct afb_request *closure, void *value, void (*free_value)(void*))
{
	struct afb_xreq *xreq = from_request(closure);
	afb_hook_xreq_context_set(xreq, value, free_value);
	xreq_context_set_cb(closure, value, free_value);
}

static struct afb_request *xreq_hooked_addref_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	afb_hook_xreq_addref(xreq);
	return xreq_addref_cb(closure);
}

static void xreq_hooked_unref_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	afb_hook_xreq_unref(xreq);
	xreq_unref_cb(closure);
}

static void xreq_hooked_session_close_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	afb_hook_xreq_session_close(xreq);
	xreq_session_close_cb(closure);
}

static int xreq_hooked_session_set_LOA_cb(struct afb_request *closure, unsigned level)
{
	int r = xreq_session_set_LOA_cb(closure, level);
	struct afb_xreq *xreq = from_request(closure);
	return afb_hook_xreq_session_set_LOA(xreq, level, r);
}

static int xreq_hooked_subscribe_eventid_cb(struct afb_request *closure, struct afb_eventid *eventid);
static int xreq_hooked_subscribe_cb(struct afb_request *closure, struct afb_event event)
{
	return xreq_hooked_subscribe_eventid_cb(closure, afb_event_to_eventid(event));
}

static int xreq_hooked_subscribe_eventid_cb(struct afb_request *closure, struct afb_eventid *eventid)
{
	int r = xreq_subscribe_eventid_cb(closure, eventid);
	struct afb_xreq *xreq = from_request(closure);
	return afb_hook_xreq_subscribe(xreq, eventid, r);
}

static int xreq_hooked_unsubscribe_eventid_cb(struct afb_request *closure, struct afb_eventid *eventid);
static int xreq_hooked_unsubscribe_cb(struct afb_request *closure, struct afb_event event)
{
	return xreq_hooked_unsubscribe_eventid_cb(closure, afb_event_to_eventid(event));
}

static int xreq_hooked_unsubscribe_eventid_cb(struct afb_request *closure, struct afb_eventid *eventid)
{
	int r = xreq_unsubscribe_eventid_cb(closure, eventid);
	struct afb_xreq *xreq = from_request(closure);
	return afb_hook_xreq_unsubscribe(xreq, eventid, r);
}

static void xreq_hooked_subcall_cb(struct afb_request *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	struct afb_xreq *xreq = from_request(closure);
	struct subcall *sc;

	afb_hook_xreq_subcall(xreq, api, verb, args);
	sc = subcall_alloc(xreq, api, verb, args);
	if (sc == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error());
		json_object_put(args);
	} else {
		subcall_hooked(sc, callback, cb_closure);
	}
}

static void xreq_hooked_subcall_req_cb(struct afb_request *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*, struct afb_req), void *cb_closure)
{
	struct afb_xreq *xreq = from_request(closure);
	struct subcall *sc;

	afb_hook_xreq_subcall_req(xreq, api, verb, args);
	sc = subcall_alloc(xreq, api, verb, args);
	if (sc == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error(), to_req(xreq));
		json_object_put(args);
	} else {
		subcall_req_hooked(sc, callback, cb_closure);
	}
}

static void xreq_hooked_subcall_request_cb(struct afb_request *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*, struct afb_request *), void *cb_closure)
{
	struct afb_xreq *xreq = from_request(closure);
	struct subcall *sc;

	afb_hook_xreq_subcall(xreq, api, verb, args);
	sc = subcall_alloc(xreq, api, verb, args);
	if (sc == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error(), to_request(xreq));
		json_object_put(args);
	} else {
		subcall_request_hooked(sc, callback, cb_closure);
	}
}

static int xreq_hooked_subcallsync_cb(struct afb_request *closure, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	int r;
	struct afb_xreq *xreq = from_request(closure);
	afb_hook_xreq_subcallsync(xreq, api, verb, args);
	r = xreq_subcallsync_cb(closure, api, verb, args, result);
	return afb_hook_xreq_subcallsync_result(xreq, r, *result);
}

static void xreq_hooked_vverbose_cb(struct afb_request *closure, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	struct afb_xreq *xreq = from_request(closure);
	va_list ap;
	va_copy(ap, args);
	xreq_vverbose_cb(closure, level, file, line, func, fmt, args);
	afb_hook_xreq_vverbose(xreq, level, file, line, func, fmt, ap);
	va_end(ap);
}

static struct afb_stored_req *xreq_hooked_store_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	struct afb_stored_req *r = xreq_store_cb(closure);
	afb_hook_xreq_store(xreq, r);
	return r;
}

static int xreq_hooked_has_permission_cb(struct afb_request *closure, const char *permission)
{
	struct afb_xreq *xreq = from_request(closure);
	int r = xreq_has_permission_cb(closure, permission);
	return afb_hook_xreq_has_permission(xreq, permission, r);
}

static char *xreq_hooked_get_application_id_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	char *r = xreq_get_application_id_cb(closure);
	return afb_hook_xreq_get_application_id(xreq, r);
}

static void *xreq_hooked_context_make_cb(struct afb_request *closure, int replace, void *(*create_value)(void*), void (*free_value)(void*), void *create_closure)
{
	struct afb_xreq *xreq = from_request(closure);
	void *result = xreq_context_make_cb(closure, replace, create_value, free_value, create_closure);
	return afb_hook_xreq_context_make(xreq, replace, create_value, free_value, create_closure, result);
}

static int xreq_hooked_get_uid_cb(struct afb_request *closure)
{
	struct afb_xreq *xreq = from_request(closure);
	int r = xreq_get_uid_cb(closure);
	return afb_hook_xreq_get_uid(xreq, r);
}

/******************************************************************************/

const struct afb_request_itf xreq_itf = {
	.json = xreq_json_cb,
	.get = xreq_get_cb,
	.success = xreq_success_cb,
	.fail = xreq_fail_cb,
	.vsuccess = xreq_vsuccess_cb,
	.vfail = xreq_vfail_cb,
	.context_get = xreq_context_get_cb,
	.context_set = xreq_context_set_cb,
	.addref = xreq_addref_cb,
	.unref = xreq_unref_cb,
	.session_close = xreq_session_close_cb,
	.session_set_LOA = xreq_session_set_LOA_cb,
	.subscribe = xreq_subscribe_cb,
	.unsubscribe = xreq_unsubscribe_cb,
	.subcall = xreq_subcall_cb,
	.subcallsync = xreq_subcallsync_cb,
	.vverbose = xreq_vverbose_cb,
	.store = xreq_store_cb,
	.subcall_req = xreq_subcall_req_cb,
	.has_permission = xreq_has_permission_cb,
	.get_application_id = xreq_get_application_id_cb,
	.context_make = xreq_context_make_cb,
	.subscribe_eventid = xreq_subscribe_eventid_cb,
	.unsubscribe_eventid = xreq_unsubscribe_eventid_cb,
	.subcall_request = xreq_subcall_request_cb,
	.get_uid = xreq_get_uid_cb,
};

const struct afb_request_itf xreq_hooked_itf = {
	.json = xreq_hooked_json_cb,
	.get = xreq_hooked_get_cb,
	.success = xreq_hooked_success_cb,
	.fail = xreq_hooked_fail_cb,
	.vsuccess = xreq_hooked_vsuccess_cb,
	.vfail = xreq_hooked_vfail_cb,
	.context_get = xreq_hooked_context_get_cb,
	.context_set = xreq_hooked_context_set_cb,
	.addref = xreq_hooked_addref_cb,
	.unref = xreq_hooked_unref_cb,
	.session_close = xreq_hooked_session_close_cb,
	.session_set_LOA = xreq_hooked_session_set_LOA_cb,
	.subscribe = xreq_hooked_subscribe_cb,
	.unsubscribe = xreq_hooked_unsubscribe_cb,
	.subcall = xreq_hooked_subcall_cb,
	.subcallsync = xreq_hooked_subcallsync_cb,
	.vverbose = xreq_hooked_vverbose_cb,
	.store = xreq_hooked_store_cb,
	.subcall_req = xreq_hooked_subcall_req_cb,
	.has_permission = xreq_hooked_has_permission_cb,
	.get_application_id = xreq_hooked_get_application_id_cb,
	.context_make = xreq_hooked_context_make_cb,
	.subscribe_eventid = xreq_hooked_subscribe_eventid_cb,
	.unsubscribe_eventid = xreq_hooked_unsubscribe_eventid_cb,
	.subcall_request = xreq_hooked_subcall_request_cb,
	.get_uid = xreq_hooked_get_uid_cb,
};

/******************************************************************************/

struct afb_req afb_xreq_unstore(struct afb_stored_req *sreq)
{
	struct afb_xreq *xreq = (struct afb_xreq *)sreq;
	if (xreq->hookflags)
		afb_hook_xreq_unstore(xreq);
	return to_req(xreq);
}

struct json_object *afb_xreq_json(struct afb_xreq *xreq)
{
	return afb_request_json(to_request(xreq));
}

void afb_xreq_success(struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	afb_request_success(to_request(xreq), obj, info);
}

void afb_xreq_success_f(struct afb_xreq *xreq, struct json_object *obj, const char *info, ...)
{
	va_list args;

	va_start(args, info);
	afb_request_success_v(to_request(xreq), obj, info, args);
	va_end(args);
}

void afb_xreq_fail(struct afb_xreq *xreq, const char *status, const char *info)
{
	afb_request_fail(to_request(xreq), status, info);
}

void afb_xreq_fail_f(struct afb_xreq *xreq, const char *status, const char *info, ...)
{
	va_list args;

	va_start(args, info);
	afb_request_fail_v(to_request(xreq), status, info, args);
	va_end(args);

}

const char *afb_xreq_raw(struct afb_xreq *xreq, size_t *size)
{
	struct json_object *obj = xreq_json_cb(to_request(xreq));
	const char *result = json_object_to_json_string(obj);
	if (size != NULL)
		*size = strlen(result);
	return result;
}

void afb_xreq_addref(struct afb_xreq *xreq)
{
	afb_request_addref(to_request(xreq));
}

void afb_xreq_unref(struct afb_xreq *xreq)
{
	afb_request_unref(to_request(xreq));
}

void afb_xreq_unhooked_subcall(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*, struct afb_request *), void *cb_closure)
{
	xreq_subcall_request_cb(to_request(xreq), api, verb, args, callback, cb_closure);
}

void afb_xreq_subcall(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*, struct afb_request *), void *cb_closure)
{
	afb_request_subcall(to_request(xreq), api, verb, args, callback, cb_closure);
}

int afb_xreq_unhooked_subcall_sync(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	return xreq_subcallsync_cb(to_request(xreq), api, verb, args, result);
}

int afb_xreq_subcall_sync(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	return afb_request_subcall_sync(to_request(xreq), api, verb, args, result);
}

static int xreq_session_check_apply_v1(struct afb_xreq *xreq, int sessionflags)
{
	int loa;

	if ((sessionflags & (AFB_SESSION_CLOSE_V1|AFB_SESSION_RENEW_V1|AFB_SESSION_CHECK_V1|AFB_SESSION_LOA_EQ_V1)) != 0) {
		if (!afb_context_check(&xreq->context)) {
			afb_context_close(&xreq->context);
			afb_xreq_fail_f(xreq, "denied", "invalid token's identity");
			errno = EINVAL;
			return -1;
		}
	}

	if ((sessionflags & AFB_SESSION_LOA_GE_V1) != 0) {
		loa = (sessionflags >> AFB_SESSION_LOA_SHIFT_V1) & AFB_SESSION_LOA_MASK_V1;
		if (!afb_context_check_loa(&xreq->context, loa)) {
			afb_xreq_fail_f(xreq, "denied", "invalid LOA");
			errno = EPERM;
			return -1;
		}
	}

	if ((sessionflags & AFB_SESSION_LOA_LE_V1) != 0) {
		loa = (sessionflags >> AFB_SESSION_LOA_SHIFT_V1) & AFB_SESSION_LOA_MASK_V1;
		if (afb_context_check_loa(&xreq->context, loa + 1)) {
			afb_xreq_fail_f(xreq, "denied", "invalid LOA");
			errno = EPERM;
			return -1;
		}
	}

	if ((sessionflags & AFB_SESSION_RENEW_V1) != 0) {
		afb_context_refresh(&xreq->context);
	}
	if ((sessionflags & AFB_SESSION_CLOSE_V1) != 0) {
		afb_context_change_loa(&xreq->context, 0);
		afb_context_close(&xreq->context);
	}

	return 0;
}

static int xreq_session_check_apply_v2(struct afb_xreq *xreq, uint32_t sessionflags, const struct afb_auth *auth)
{
	int loa;

	if (sessionflags != 0) {
		if (!afb_context_check(&xreq->context)) {
			afb_context_close(&xreq->context);
			afb_xreq_fail_f(xreq, "denied", "invalid token's identity");
			errno = EINVAL;
			return -1;
		}
	}

	loa = (int)(sessionflags & AFB_SESSION_LOA_MASK_V2);
	if (loa && !afb_context_check_loa(&xreq->context, loa)) {
		afb_xreq_fail_f(xreq, "denied", "invalid LOA");
		errno = EPERM;
		return -1;
	}

	if (auth && !afb_auth_check(xreq, auth)) {
		afb_xreq_fail_f(xreq, "denied", "authorisation refused");
		errno = EPERM;
		return -1;
	}

	if ((sessionflags & AFB_SESSION_REFRESH_V2) != 0) {
		afb_context_refresh(&xreq->context);
	}
	if ((sessionflags & AFB_SESSION_CLOSE_V2) != 0) {
		afb_context_close(&xreq->context);
	}

	return 0;
}

void afb_xreq_call_verb_v1(struct afb_xreq *xreq, const struct afb_verb_desc_v1 *verb)
{
	if (!verb)
		afb_xreq_fail_unknown_verb(xreq);
	else
		if (!xreq_session_check_apply_v1(xreq, verb->session))
			verb->callback(to_req(xreq));
}

void afb_xreq_call_verb_v2(struct afb_xreq *xreq, const struct afb_verb_v2 *verb)
{
	if (!verb)
		afb_xreq_fail_unknown_verb(xreq);
	else
		if (!xreq_session_check_apply_v2(xreq, verb->session, verb->auth))
			verb->callback(to_req(xreq));
}

void afb_xreq_call_verb_vdyn(struct afb_xreq *xreq, const struct afb_api_dyn_verb *verb)
{
	if (!verb)
		afb_xreq_fail_unknown_verb(xreq);
	else
		if (xreq_session_check_apply_v2(xreq, verb->session, verb->auth) >= 0)
			verb->callback(to_request(xreq));
}

void afb_xreq_init(struct afb_xreq *xreq, const struct afb_xreq_query_itf *queryitf)
{
	memset(xreq, 0, sizeof *xreq);
	xreq->request.itf = &xreq_hooked_itf; /* hook by default */
	xreq->refcount = 1;
	xreq->queryitf = queryitf;
}

void afb_xreq_fail_unknown_api(struct afb_xreq *xreq)
{
	afb_xreq_fail_f(xreq, "unknown-api", "api %s not found (for verb %s)", xreq->request.api, xreq->request.verb);
}

void afb_xreq_fail_unknown_verb(struct afb_xreq *xreq)
{
	afb_xreq_fail_f(xreq, "unknown-verb", "verb %s unknown within api %s", xreq->request.verb, xreq->request.api);
}

static void init_hooking(struct afb_xreq *xreq)
{
	afb_hook_init_xreq(xreq);
	if (xreq->hookflags)
		afb_hook_xreq_begin(xreq);
	else
		xreq->request.itf = &xreq_itf; /* unhook the interface */
}

/**
 * job callback for asynchronous and secured processing of the request.
 */
static void process_async(int signum, void *arg)
{
	struct afb_xreq *xreq = arg;
	const struct afb_api *api;

	if (signum != 0) {
		/* emit the error (assumes that hooking is initialised) */
		afb_xreq_fail_f(xreq, "aborted", "signal %s(%d) caught", strsignal(signum), signum);
	} else {
		/* init hooking */
		init_hooking(xreq);
		/* invoke api call method to process the reqiest */
		api = (const struct afb_api*)xreq->context.api_key;
		api->itf->call(api->closure, xreq);
	}
	/* release the request */
	afb_xreq_unhooked_unref(xreq);
}

/**
 * Early request failure of the request 'xreq' with, as usual, 'status' and 'info'
 * The early failure occurs only in function 'afb_xreq_process' where normally,
 * the hooking is not initialised. So this "early" failure takes care to initialise
 * the hooking in first.
 */
static void early_failure(struct afb_xreq *xreq, const char *status, const char *info, ...)
{
	va_list args;

	/* init hooking */
	init_hooking(xreq);

	/* send error */
	va_start(args, info);
	afb_request_fail_v(to_request(xreq), status, info, args);
	va_end(args);
}

/**
 * Enqueue a job for processing the request 'xreq' using the given 'apiset'.
 * Errors are reported as request failures.
 */
void afb_xreq_process(struct afb_xreq *xreq, struct afb_apiset *apiset)
{
	const struct afb_api *api;
	struct afb_xreq *caller;

	/* lookup at the api */
	xreq->apiset = apiset;
	api = afb_apiset_lookup_started(apiset, xreq->request.api, 1);
	if (!api) {
		if (errno == ENOENT)
			early_failure(xreq, "unknown-api", "api %s not found (for verb %s)", xreq->request.api, xreq->request.verb);
		else
			early_failure(xreq, "bad-api-state", "api %s not started correctly: %m", xreq->request.api);
		goto end;
	}
	xreq->context.api_key = api;

	/* check self locking */
	if (api->group) {
		caller = xreq->caller;
		while (caller) {
			if (((const struct afb_api *)caller->context.api_key)->group == api->group) {
				/* noconcurrency lock detected */
				ERROR("self-lock detected in call stack for API %s", xreq->request.api);
				early_failure(xreq, "self-locked", "recursive self lock, API %s", xreq->request.api);
				goto end;
			}
			caller = caller->caller;
		}
	}

	/* queue the request job */
	afb_xreq_unhooked_addref(xreq);
	if (jobs_queue(api->group, afb_apiset_timeout_get(apiset), process_async, xreq) < 0) {
		/* TODO: allows or not to proccess it directly as when no threading? (see above) */
		ERROR("can't process job with threads: %m");
		early_failure(xreq, "cancelled", "not able to create a job for the task");
		afb_xreq_unhooked_unref(xreq);
	}
end:
	afb_xreq_unhooked_unref(xreq);
}

