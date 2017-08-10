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
#define AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <json-c/json.h>
#include <afb/afb-binding-v1.h>
#include <afb/afb-binding-v2.h>

#include "afb-context.h"
#include "afb-xreq.h"
#include "afb-evt.h"
#include "afb-msg-json.h"
#include "afb-cred.h"
#include "afb-hook.h"
#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-auth.h"
#include "jobs.h"
#include "verbose.h"

/******************************************************************************/

static inline void xreq_addref(struct afb_xreq *xreq)
{
	__atomic_add_fetch(&xreq->refcount, 1, __ATOMIC_RELAXED);
}

static inline void xreq_unref(struct afb_xreq *xreq)
{
	if (!__atomic_sub_fetch(&xreq->refcount, 1, __ATOMIC_RELAXED)) {
		if (!xreq->replied)
			afb_xreq_fail(xreq, "error", "no reply");
		if (xreq->hookflags)
			afb_hook_xreq_end(xreq);
		xreq->queryitf->unref(xreq);
	}
}

/******************************************************************************/

extern const struct afb_req_itf xreq_itf;
extern const struct afb_req_itf xreq_hooked_itf;

static inline struct afb_req to_req(struct afb_xreq *xreq)
{
	return (struct afb_req){ .itf = xreq->hookflags ? &xreq_hooked_itf : &xreq_itf, .closure = xreq };
}

/******************************************************************************/

struct subcall
{
	struct afb_xreq xreq;
	struct afb_xreq *caller;
	void (*callback)(void*, int, struct json_object*);
	void *closure;
	union {
		struct {
			struct jobloop *jobloop;
			struct json_object *result;
			int status;
		};
		struct {
			union {
				void (*callback)(void*, int, struct json_object*);
				void (*callback2)(void*, int, struct json_object*, struct afb_req);
			};
			void *closure;
		} hooked;
	};
};

static int subcall_subscribe(struct afb_xreq *xreq, struct afb_event event)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	return afb_xreq_subscribe(subcall->caller, event);
}

static int subcall_unsubscribe(struct afb_xreq *xreq, struct afb_event event)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	return afb_xreq_unsubscribe(subcall->caller, event);
}

static void subcall_reply(struct afb_xreq *xreq, int status, struct json_object *obj)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	if (subcall->callback)
		subcall->callback(subcall->closure, status, obj);
	json_object_put(obj);
}

static void subcall_destroy(struct afb_xreq *xreq)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	json_object_put(subcall->xreq.json);
	afb_cred_unref(subcall->xreq.cred);
	xreq_unref(subcall->caller);
	free(subcall);
}

const struct afb_xreq_query_itf afb_xreq_subcall_itf = {
	.reply = subcall_reply,
	.unref = subcall_destroy,
	.subscribe = subcall_subscribe,
	.unsubscribe = subcall_unsubscribe
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
		subcall->xreq.api = api;
		subcall->xreq.verb = verb;
		subcall->caller = caller;
		xreq_addref(caller);
	}
	return subcall;
}

static void subcall_process(struct subcall *subcall)
{
	if (subcall->caller->queryitf->subcall) {
		subcall->caller->queryitf->subcall(
			subcall->caller, subcall->xreq.api, subcall->xreq.verb,
			subcall->xreq.json, subcall->callback, subcall->closure);
		xreq_unref(&subcall->xreq);
	} else
		afb_xreq_process(&subcall->xreq, subcall->caller->apiset);
}

static void subcall_sync_leave(struct subcall *subcall)
{
	struct jobloop *jobloop = __atomic_exchange_n(&subcall->jobloop, NULL, __ATOMIC_RELAXED);
	if (jobloop)
		jobs_leave(jobloop);
}

static void subcall_sync_reply(void *closure, int status, struct json_object *obj)
{
	struct subcall *subcall = closure;

	subcall->status = status;
	subcall->result = json_object_get(obj);
	subcall_sync_leave(subcall);
}

static void subcall_sync_enter(int signum, void *closure, struct jobloop *jobloop)
{
	struct subcall *subcall = closure;

	if (!signum) {
		subcall->jobloop = jobloop;
		subcall_process(subcall);
	} else {
		subcall->status = -1;
		subcall_sync_leave(subcall);
	}
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

static struct json_object *xreq_json_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	if (!xreq->json && xreq->queryitf->json)
		xreq->json = xreq->queryitf->json(xreq);
	return xreq->json;
}

static struct afb_arg xreq_get_cb(void *closure, const char *name)
{
	struct afb_xreq *xreq = closure;
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

static void xreq_success_cb(void *closure, struct json_object *obj, const char *info)
{
	struct afb_xreq *xreq = closure;

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

static void xreq_fail_cb(void *closure, const char *status, const char *info)
{
	struct afb_xreq *xreq = closure;

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

static void xreq_vsuccess_cb(void *closure, struct json_object *obj, const char *fmt, va_list args)
{
	vinfo(closure, obj, fmt, args, (void*)xreq_success_cb);
}

static void xreq_vfail_cb(void *closure, const char *status, const char *fmt, va_list args)
{
	vinfo(closure, (void*)status, fmt, args, (void*)xreq_fail_cb);
}

static void *xreq_context_get_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	return afb_context_get(&xreq->context);
}

static void xreq_context_set_cb(void *closure, void *value, void (*free_value)(void*))
{
	struct afb_xreq *xreq = closure;
	afb_context_set(&xreq->context, value, free_value);
}

static void xreq_addref_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	xreq_addref(xreq);
}

static void xreq_unref_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	xreq_unref(xreq);
}

static void xreq_session_close_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	afb_context_close(&xreq->context);
}

static int xreq_session_set_LOA_cb(void *closure, unsigned level)
{
	struct afb_xreq *xreq = closure;
	return afb_context_change_loa(&xreq->context, level);
}

static int xreq_subscribe_cb(void *closure, struct afb_event event)
{
	struct afb_xreq *xreq = closure;
	return afb_xreq_subscribe(xreq, event);
}

int afb_xreq_subscribe(struct afb_xreq *xreq, struct afb_event event)
{
	if (xreq->listener)
		return afb_evt_add_watch(xreq->listener, event);
	if (xreq->queryitf->subscribe)
		return xreq->queryitf->subscribe(xreq, event);
	ERROR("no event listener, subscription impossible");
	errno = EINVAL;
	return -1;
}

static int xreq_unsubscribe_cb(void *closure, struct afb_event event)
{
	struct afb_xreq *xreq = closure;
	return afb_xreq_unsubscribe(xreq, event);
}

int afb_xreq_unsubscribe(struct afb_xreq *xreq, struct afb_event event)
{
	if (xreq->listener)
		return afb_evt_remove_watch(xreq->listener, event);
	if (xreq->queryitf->unsubscribe)
		return xreq->queryitf->unsubscribe(xreq, event);
	ERROR("no event listener, unsubscription impossible");
	errno = EINVAL;
	return -1;
}

static void xreq_subcall_cb(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	struct afb_xreq *xreq = closure;
	struct subcall *subcall;

	subcall = subcall_alloc(xreq, api, verb, args);
	if (subcall == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error());
		json_object_put(args);
	} else {
		subcall->callback = callback;
		subcall->closure = cb_closure;
		subcall_process(subcall);
	}
}

static void xreq_subcall_req_reply_cb(void *closure, int status, struct json_object *result)
{
	struct subcall *subcall = closure;
	subcall->hooked.callback2(subcall->hooked.closure, status, result, to_req(subcall->caller));
}

static void xreq_subcall_req_cb(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*, struct afb_req), void *cb_closure)
{
	struct afb_xreq *xreq = closure;
	struct subcall *subcall;

	subcall = subcall_alloc(xreq, api, verb, args);
	if (subcall == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error(), to_req(xreq));
		json_object_put(args);
	} else {
		subcall->callback = xreq_subcall_req_reply_cb;
		subcall->closure = subcall;
		subcall->hooked.callback2 = callback;
		subcall->hooked.closure = cb_closure;
		subcall_process(subcall);
	}
}


static int xreq_subcallsync_cb(void *closure, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	int rc;
	struct subcall *subcall;
	struct afb_xreq *xreq = closure;
	struct json_object *resu;

	subcall = subcall_alloc(xreq, api, verb, args);
	if (!subcall) {
		rc = -1;
		resu = afb_msg_json_internal_error();
		json_object_put(args);
	} else {
		subcall->callback = subcall_sync_reply;
		subcall->closure = subcall;
		subcall->jobloop = NULL;
		subcall->result = NULL;
		subcall->status = 0;
		rc = jobs_enter(NULL, 0, subcall_sync_enter, subcall);
		resu = subcall->result;
		if (rc < 0 || subcall->status < 0) {
			resu = resu ?: afb_msg_json_internal_error();
			rc = -1;
		}
	}
	if (result)
		*result = resu;
	else
		json_object_put(resu);
	return rc;
}

static void xreq_vverbose_cb(void*closure, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	/* TODO: improves the implementation. example: on condition make a list of log messages that will be returned */
	vverbose(level, file, line, func, fmt, args);
}

static struct afb_stored_req *xreq_store_cb(void *closure)
{
	xreq_addref_cb(closure);
	return closure;
}

/******************************************************************************/

static struct json_object *xreq_hooked_json_cb(void *closure)
{
	struct json_object *r = xreq_json_cb(closure);
	struct afb_xreq *xreq = closure;
	return afb_hook_xreq_json(xreq, r);
}

static struct afb_arg xreq_hooked_get_cb(void *closure, const char *name)
{
	struct afb_arg r = xreq_get_cb(closure, name);
	struct afb_xreq *xreq = closure;
	return afb_hook_xreq_get(xreq, name, r);
}

static void xreq_hooked_success_cb(void *closure, struct json_object *obj, const char *info)
{
	struct afb_xreq *xreq = closure;
	afb_hook_xreq_success(xreq, obj, info);
	xreq_success_cb(closure, obj, info);
}

static void xreq_hooked_fail_cb(void *closure, const char *status, const char *info)
{
	struct afb_xreq *xreq = closure;
	afb_hook_xreq_fail(xreq, status, info);
	xreq_fail_cb(closure, status, info);
}

static void xreq_hooked_vsuccess_cb(void *closure, struct json_object *obj, const char *fmt, va_list args)
{
	vinfo(closure, obj, fmt, args, (void*)xreq_hooked_success_cb);
}

static void xreq_hooked_vfail_cb(void *closure, const char *status, const char *fmt, va_list args)
{
	vinfo(closure, (void*)status, fmt, args, (void*)xreq_hooked_fail_cb);
}

static void *xreq_hooked_context_get_cb(void *closure)
{
	void *r = xreq_context_get_cb(closure);
	struct afb_xreq *xreq = closure;
	return afb_hook_xreq_context_get(xreq, r);
}

static void xreq_hooked_context_set_cb(void *closure, void *value, void (*free_value)(void*))
{
	struct afb_xreq *xreq = closure;
	afb_hook_xreq_context_set(xreq, value, free_value);
	xreq_context_set_cb(closure, value, free_value);
}

static void xreq_hooked_addref_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	afb_hook_xreq_addref(xreq);
	xreq_addref_cb(closure);
}

static void xreq_hooked_unref_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	afb_hook_xreq_unref(xreq);
	xreq_unref_cb(closure);
}

static void xreq_hooked_session_close_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	afb_hook_xreq_session_close(xreq);
	xreq_session_close_cb(closure);
}

static int xreq_hooked_session_set_LOA_cb(void *closure, unsigned level)
{
	int r = xreq_session_set_LOA_cb(closure, level);
	struct afb_xreq *xreq = closure;
	return afb_hook_xreq_session_set_LOA(xreq, level, r);
}

static int xreq_hooked_subscribe_cb(void *closure, struct afb_event event)
{
	int r = xreq_subscribe_cb(closure, event);
	struct afb_xreq *xreq = closure;
	return afb_hook_xreq_subscribe(xreq, event, r);
}

static int xreq_hooked_unsubscribe_cb(void *closure, struct afb_event event)
{
	int r = xreq_unsubscribe_cb(closure, event);
	struct afb_xreq *xreq = closure;
	return afb_hook_xreq_unsubscribe(xreq, event, r);
}

static void xreq_hooked_subcall_reply_cb(void *closure, int status, struct json_object *result)
{
	struct subcall *subcall = closure;

	afb_hook_xreq_subcall_result(subcall->caller, status, result);
	subcall->hooked.callback(subcall->hooked.closure, status, result);
}

static void xreq_hooked_subcall_cb(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	struct afb_xreq *xreq = closure;
	struct subcall *subcall;

	afb_hook_xreq_subcall(xreq, api, verb, args);
	subcall = subcall_alloc(xreq, api, verb, args);
	if (subcall == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error());
		json_object_put(args);
	} else {
		subcall->callback = xreq_hooked_subcall_reply_cb;
		subcall->closure = subcall;
		subcall->hooked.callback = callback;
		subcall->hooked.closure = cb_closure;
		subcall_process(subcall);
	}
}

static void xreq_hooked_subcall_req_reply_cb(void *closure, int status, struct json_object *result)
{
	struct subcall *subcall = closure;

	afb_hook_xreq_subcall_req_result(subcall->caller, status, result);
	subcall->hooked.callback2(subcall->hooked.closure, status, result, to_req(subcall->caller));
}

static void xreq_hooked_subcall_req_cb(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*, struct afb_req), void *cb_closure)
{
	struct afb_xreq *xreq = closure;
	struct subcall *subcall;

	afb_hook_xreq_subcall_req(xreq, api, verb, args);
	subcall = subcall_alloc(xreq, api, verb, args);
	if (subcall == NULL) {
		if (callback)
			callback(cb_closure, 1, afb_msg_json_internal_error(), to_req(xreq));
		json_object_put(args);
	} else {
		subcall->callback = xreq_hooked_subcall_req_reply_cb;
		subcall->closure = subcall;
		subcall->hooked.callback2 = callback;
		subcall->hooked.closure = cb_closure;
		subcall_process(subcall);
	}
}

static int xreq_hooked_subcallsync_cb(void *closure, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	int r;
	struct afb_xreq *xreq = closure;
	afb_hook_xreq_subcallsync(xreq, api, verb, args);
	r = xreq_subcallsync_cb(closure, api, verb, args, result);
	return afb_hook_xreq_subcallsync_result(xreq, r, *result);
}

static void xreq_hooked_vverbose_cb(void*closure, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	struct afb_xreq *xreq = closure;
	va_list ap;
	va_copy(ap, args);
	xreq_vverbose_cb(closure, level, file, line, func, fmt, args);
	afb_hook_xreq_vverbose(xreq, level, file, line, func, fmt, ap);
	va_end(ap);
}

static struct afb_stored_req *xreq_hooked_store_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	struct afb_stored_req *r = xreq_store_cb(closure);
	afb_hook_xreq_store(xreq, r);
	return r;
}

/******************************************************************************/

const struct afb_req_itf xreq_itf = {
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
	.subcall_req = xreq_subcall_req_cb
};

const struct afb_req_itf xreq_hooked_itf = {
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
	.subcall_req = xreq_hooked_subcall_req_cb
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
	return afb_req_json(to_req(xreq));
}

void afb_xreq_success(struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	afb_req_success(to_req(xreq), obj, info);
}

void afb_xreq_success_f(struct afb_xreq *xreq, struct json_object *obj, const char *info, ...)
{
	char *message;
	va_list args;
	va_start(args, info);
	if (info == NULL || vasprintf(&message, info, args) < 0)
		message = NULL;
	va_end(args);
	afb_xreq_success(xreq, obj, message);
	free(message);
}

void afb_xreq_fail(struct afb_xreq *xreq, const char *status, const char *info)
{
	afb_req_fail(to_req(xreq), status, info);
}

void afb_xreq_fail_f(struct afb_xreq *xreq, const char *status, const char *info, ...)
{
	char *message;
	va_list args;
	va_start(args, info);
	if (info == NULL || vasprintf(&message, info, args) < 0)
		message = NULL;
	va_end(args);
	afb_xreq_fail(xreq, status, message);
	free(message);
}

const char *afb_xreq_raw(struct afb_xreq *xreq, size_t *size)
{
	struct json_object *obj = xreq_json_cb(xreq);
	const char *result = json_object_to_json_string(obj);
	if (size != NULL)
		*size = strlen(result);
	return result;
}

void afb_xreq_addref(struct afb_xreq *xreq)
{
	afb_req_addref(to_req(xreq));
}

void afb_xreq_unref(struct afb_xreq *xreq)
{
	afb_req_unref(to_req(xreq));
}

void afb_xreq_unhooked_subcall(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	xreq_subcall_cb(xreq, api, verb, args, callback, cb_closure);
}

void afb_xreq_subcall(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	afb_req_subcall(to_req(xreq), api, verb, args, callback, cb_closure);
}

int afb_xreq_unhooked_subcall_sync(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	return xreq_subcallsync_cb(xreq, api, verb, args, result);
}

int afb_xreq_subcall_sync(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	return afb_req_subcall_sync(to_req(xreq), api, verb, args, result);
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

	if (auth && !afb_auth_check(auth, xreq)) {
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

void afb_xreq_init(struct afb_xreq *xreq, const struct afb_xreq_query_itf *queryitf)
{
	memset(xreq, 0, sizeof *xreq);
	xreq->refcount = 1;
	xreq->queryitf = queryitf;
}

void afb_xreq_fail_unknown_api(struct afb_xreq *xreq)
{
	afb_xreq_fail_f(xreq, "unknown-api", "api %s not found (for verb %s)", xreq->api, xreq->verb);
}

void afb_xreq_fail_unknown_verb(struct afb_xreq *xreq)
{
	afb_xreq_fail_f(xreq, "unknown-verb", "verb %s unknown within api %s", xreq->verb, xreq->api);
}

static void process_sync(struct afb_xreq *xreq)
{
	struct afb_api api;

	/* init hooking */
	afb_hook_init_xreq(xreq);
	if (xreq->hookflags)
		afb_hook_xreq_begin(xreq);

	/* search the api */
	if (afb_apiset_get_started(xreq->apiset, xreq->api, &api) < 0) {
		if (errno == ENOENT)
			afb_xreq_fail_f(xreq, "unknown-api", "api %s not found", xreq->api);
		else
			afb_xreq_fail_f(xreq, "bad-api-state", "api %s not started correctly: %m", xreq->api);
	} else {
		xreq->context.api_key = api.closure;
		api.itf->call(api.closure, xreq);
	}
}

static void process_async(int signum, void *arg)
{
	struct afb_xreq *xreq = arg;

	if (signum != 0) {
		afb_xreq_fail_f(xreq, "aborted", "signal %s(%d) caught", strsignal(signum), signum);
	} else {
		process_sync(xreq);
	}
	xreq_unref(xreq);
}

void afb_xreq_process(struct afb_xreq *xreq, struct afb_apiset *apiset)
{
	xreq->apiset = apiset;

	xreq_addref(xreq);
	if (jobs_queue(NULL, afb_apiset_timeout_get(apiset), process_async, xreq) < 0) {
		/* TODO: allows or not to proccess it directly as when no threading? (see above) */
		ERROR("can't process job with threads: %m");
		afb_xreq_fail_f(xreq, "cancelled", "not able to create a job for the task");
		xreq_unref(xreq);
	}
	xreq_unref(xreq);
}

