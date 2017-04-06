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
#define NO_BINDING_VERBOSE_MACRO

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <json-c/json.h>
#include <afb/afb-binding.h>

#include "afb-context.h"
#include "afb-xreq.h"
#include "afb-evt.h"
#include "afb-msg-json.h"
#include "afb-subcall.h"
#include "afb-hook.h"
#include "verbose.h"


static struct json_object *xreq_json_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	return xreq->json ? : (xreq->json = xreq->queryitf->json(xreq->query));
}

static struct afb_arg xreq_get_cb(void *closure, const char *name)
{
	struct afb_xreq *xreq = closure;
	if (xreq->queryitf->get)
		return xreq->queryitf->get(xreq->query, name);
	else
		return afb_msg_json_get_arg(xreq_json_cb(closure), name);
}

static void xreq_success_cb(void *closure, struct json_object *obj, const char *info)
{
	struct afb_xreq *xreq = closure;
	afb_xreq_success(xreq, obj, info);
}

void afb_xreq_success(struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	if (xreq->replied) {
		ERROR("reply called more than one time!!");
		json_object_put(obj);
	} else {
		xreq->replied = 1;
		if (xreq->queryitf->success)
			xreq->queryitf->success(xreq->query, obj, info);
		else
			xreq->queryitf->reply(xreq->query, 0, afb_msg_json_reply_ok(info, obj, &xreq->context, NULL));
	}
}

static void xreq_fail_cb(void *closure, const char *status, const char *info)
{
	struct afb_xreq *xreq = closure;
	afb_xreq_fail(xreq, status, info);
}

void afb_xreq_fail(struct afb_xreq *xreq, const char *status, const char *info)
{
	if (xreq->replied) {
		ERROR("reply called more than one time!!");
	} else {
		xreq->replied = 1;
		if (xreq->queryitf->fail)
			xreq->queryitf->fail(xreq->query, status, info);
		else
			xreq->queryitf->reply(xreq->query, 1, afb_msg_json_reply_error(status, info, &xreq->context, NULL));
	}
}

static const char *xreq_raw_cb(void *closure, size_t *size)
{
	struct afb_xreq *xreq = closure;
	return afb_xreq_raw(xreq, size);
}

const char *afb_xreq_raw(struct afb_xreq *xreq, size_t *size)
{
	const char *result = json_object_to_json_string(xreq_json_cb(xreq));
	if (size != NULL)
		*size = strlen(result);
	return result;
}

static void xreq_send_cb(void *closure, const char *buffer, size_t size)
{
	struct json_object *obj = json_tokener_parse(buffer);
	if (!obj == !buffer)
		xreq_success_cb(closure, obj, "fake send");
	else
		xreq_fail_cb(closure, "fake-send-failed", "fake send");
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
	afb_xreq_addref(xreq);
}

void afb_xreq_addref(struct afb_xreq *xreq)
{
	xreq->refcount++;
}

static void xreq_unref_cb(void *closure)
{
	struct afb_xreq *xreq = closure;
	afb_xreq_unref(xreq);
}

void afb_xreq_unref(struct afb_xreq *xreq)
{
	if (!--xreq->refcount) {
		xreq->queryitf->unref(xreq->query);
	}
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
		return xreq->queryitf->subscribe(xreq->query, event);
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
		return xreq->queryitf->unsubscribe(xreq->query, event);
	ERROR("no event listener, unsubscription impossible");
	errno = EINVAL;
	return -1;
}

static void xreq_subcall_cb(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	struct afb_xreq *xreq = closure;

	afb_xreq_subcall(xreq, api, verb, args, callback, cb_closure);
}

void afb_xreq_subcall(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	if (xreq->queryitf->subcall)
		xreq->queryitf->subcall(xreq->query, api, verb, args, callback, cb_closure);
	else
		afb_subcall(xreq, api, verb, args, callback, cb_closure);
}

static int xreq_subcallsync_cb(void *closure, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	struct afb_xreq *xreq = closure;
	return afb_subcall_sync(xreq, api, verb, args, result);
}

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

static const char *xreq_hooked_raw_cb(void *closure, size_t *size)
{
	size_t s;
	const char *r = xreq_raw_cb(closure, size ? : &s);
	struct afb_xreq *xreq = closure;
	return afb_hook_xreq_raw(xreq, r, *(size ? : &s));
}

static void xreq_hooked_send_cb(void *closure, const char *buffer, size_t size)
{
	struct afb_xreq *xreq = closure;
	afb_hook_xreq_send(xreq, buffer, size);
	xreq_send_cb(closure, buffer, size);
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
/*
static void xreq_hooked_unref_cb(void *closure)
{
	TODO
}
*/
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

/*
static void xreq_hooked_subcall_cb(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure)
{
	struct afb_xreq *xreq = closure;

	afb_xreq_subcall(xreq, api, verb, args, callback, cb_closure);
}

static int xreq_hooked_subcallsync_cb(void *closure, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	struct afb_xreq *xreq = closure;
	return afb_subcall_sync(xreq, api, verb, args, result);
}
*/
const struct afb_req_itf xreq_itf = {
	.json = xreq_json_cb,
	.get = xreq_get_cb,
	.success = xreq_success_cb,
	.fail = xreq_fail_cb,
	.raw = xreq_raw_cb,
	.send = xreq_send_cb,
	.context_get = xreq_context_get_cb,
	.context_set = xreq_context_set_cb,
	.addref = xreq_addref_cb,
	.unref = xreq_unref_cb,
	.session_close = xreq_session_close_cb,
	.session_set_LOA = xreq_session_set_LOA_cb,
	.subscribe = xreq_subscribe_cb,
	.unsubscribe = xreq_unsubscribe_cb,
	.subcall = xreq_subcall_cb,
	.subcallsync = xreq_subcallsync_cb
};

const struct afb_req_itf xreq_hooked_itf = {
	.json = xreq_hooked_json_cb,
	.get = xreq_hooked_get_cb,
	.success = xreq_hooked_success_cb,
	.fail = xreq_hooked_fail_cb,
	.raw = xreq_hooked_raw_cb,
	.send = xreq_hooked_send_cb,
	.context_get = xreq_hooked_context_get_cb,
	.context_set = xreq_hooked_context_set_cb,
	.addref = xreq_hooked_addref_cb,
.unref = xreq_unref_cb,
	.session_close = xreq_hooked_session_close_cb,
	.session_set_LOA = xreq_hooked_session_set_LOA_cb,
	.subscribe = xreq_hooked_subscribe_cb,
	.unsubscribe = xreq_hooked_unsubscribe_cb,
.subcall = xreq_subcall_cb,
.subcallsync = xreq_subcallsync_cb
};

static inline struct afb_req to_req(struct afb_xreq *xreq)
{
	return (struct afb_req){ .itf = xreq->hookflags ? &xreq_hooked_itf : &xreq_itf, .closure = xreq };
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

static int xcheck(struct afb_xreq *xreq, int sessionflags)
{
	if ((sessionflags & (AFB_SESSION_CREATE|AFB_SESSION_CLOSE|AFB_SESSION_RENEW|AFB_SESSION_CHECK|AFB_SESSION_LOA_EQ)) != 0) {
		if (!afb_context_check(&xreq->context)) {
			afb_context_close(&xreq->context);
			afb_xreq_fail_f(xreq, "failed", "invalid token's identity");
			return 0;
		}
	}

	if ((sessionflags & AFB_SESSION_CREATE) != 0) {
		if (afb_context_check_loa(&xreq->context, 1)) {
			afb_xreq_fail_f(xreq, "failed", "invalid creation state");
			return 0;
		}
		afb_context_change_loa(&xreq->context, 1);
		afb_context_refresh(&xreq->context);
	}

	if ((sessionflags & (AFB_SESSION_CREATE | AFB_SESSION_RENEW)) != 0)
		afb_context_refresh(&xreq->context);

	if ((sessionflags & AFB_SESSION_CLOSE) != 0) {
		afb_context_change_loa(&xreq->context, 0);
		afb_context_close(&xreq->context);
	}

	if ((sessionflags & AFB_SESSION_LOA_GE) != 0) {
		int loa = (sessionflags >> AFB_SESSION_LOA_SHIFT) & AFB_SESSION_LOA_MASK;
		if (!afb_context_check_loa(&xreq->context, loa)) {
			afb_xreq_fail_f(xreq, "failed", "invalid LOA");
			return 0;
		}
	}

	if ((sessionflags & AFB_SESSION_LOA_LE) != 0) {
		int loa = (sessionflags >> AFB_SESSION_LOA_SHIFT) & AFB_SESSION_LOA_MASK;
		if (afb_context_check_loa(&xreq->context, loa + 1)) {
			afb_xreq_fail_f(xreq, "failed", "invalid LOA");
			return 0;
		}
	}
	return 1;
}

void afb_xreq_call(struct afb_xreq *xreq, int sessionflags, void (*method)(struct afb_req req))
{
	if (xcheck(xreq, sessionflags))
		method(to_req(xreq));
}

void afb_xreq_begin(struct afb_xreq *xreq)
{
	afb_hook_init_xreq(xreq);
	if (xreq->hookflags)
		afb_hook_xreq_begin(xreq);
}


