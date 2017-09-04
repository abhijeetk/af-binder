/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
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

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fnmatch.h>
#include <sys/uio.h>

#include <json-c/json.h>

#include <afb/afb-req-common.h>
#include <afb/afb-event-itf.h>

#include "afb-context.h"
#include "afb-hook.h"
#include "afb-session.h"
#include "afb-cred.h"
#include "afb-xreq.h"
#include "afb-ditf.h"
#include "afb-svc.h"
#include "afb-evt.h"
#include "verbose.h"

/**
 * Definition of a hook for xreq
 */
struct afb_hook_xreq {
	struct afb_hook_xreq *next; /**< next hook */
	unsigned refcount; /**< reference count */
	char *api; /**< api hooked or NULL for any */
	char *verb; /**< verb hooked or NULL for any */
	struct afb_session *session; /**< session hooked or NULL if any */
	unsigned flags; /**< hook flags */
	struct afb_hook_xreq_itf *itf; /**< interface of hook */
	void *closure; /**< closure for callbacks */
};

/**
 * Definition of a hook for ditf
 */
struct afb_hook_ditf {
	struct afb_hook_ditf *next; /**< next hook */
	unsigned refcount; /**< reference count */
	char *api; /**< api hooked or NULL for any */
	unsigned flags; /**< hook flags */
	struct afb_hook_ditf_itf *itf; /**< interface of hook */
	void *closure; /**< closure for callbacks */
};

/**
 * Definition of a hook for svc
 */
struct afb_hook_svc {
	struct afb_hook_svc *next; /**< next hook */
	unsigned refcount; /**< reference count */
	char *api; /**< api hooked or NULL for any */
	unsigned flags; /**< hook flags */
	struct afb_hook_svc_itf *itf; /**< interface of hook */
	void *closure; /**< closure for callbacks */
};

/**
 * Definition of a hook for evt
 */
struct afb_hook_evt {
	struct afb_hook_evt *next; /**< next hook */
	unsigned refcount; /**< reference count */
	char *pattern; /**< event pattern name hooked or NULL for any */
	unsigned flags; /**< hook flags */
	struct afb_hook_evt_itf *itf; /**< interface of hook */
	void *closure; /**< closure for callbacks */
};

/**
 * Definition of a hook for global
 */
struct afb_hook_global {
	struct afb_hook_global *next; /**< next hook */
	unsigned refcount; /**< reference count */
	unsigned flags; /**< hook flags */
	struct afb_hook_global_itf *itf; /**< interface of hook */
	void *closure; /**< closure for callbacks */
};

/* synchronisation across threads */
static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

/* list of hooks for xreq */
static struct afb_hook_xreq *list_of_xreq_hooks = NULL;

/* list of hooks for ditf */
static struct afb_hook_ditf *list_of_ditf_hooks = NULL;

/* list of hooks for svc */
static struct afb_hook_svc *list_of_svc_hooks = NULL;

/* list of hooks for evt */
static struct afb_hook_evt *list_of_evt_hooks = NULL;

/* list of hooks for global */
static struct afb_hook_global *list_of_global_hooks = NULL;

/* hook id */
static unsigned next_hookid = 0;

/******************************************************************************
 * section: hook id
 *****************************************************************************/
static void init_hookid(struct afb_hookid *hookid)
{
	hookid->id = __atomic_add_fetch(&next_hookid, 1, __ATOMIC_RELAXED);
	clock_gettime(CLOCK_MONOTONIC, &hookid->time);
}

/******************************************************************************
 * section: default callbacks for tracing requests
 *****************************************************************************/

static char *_pbuf_(const char *fmt, va_list args, char **palloc, char *sbuf, size_t szsbuf, size_t *outlen)
{
	int rc;
	va_list cp;

	*palloc = NULL;
	va_copy(cp, args);
	rc = vsnprintf(sbuf, szsbuf, fmt, args);
	if ((size_t)rc >= szsbuf) {
		sbuf[szsbuf-1] = 0;
		sbuf[szsbuf-2] = sbuf[szsbuf-3] = sbuf[szsbuf-4] = '.';
		rc = vasprintf(palloc, fmt, cp);
		if (rc >= 0)
			sbuf = *palloc;
	}
	va_end(cp);
	if (rc >= 0 && outlen)
		*outlen = (size_t)rc;
	return sbuf;
}

#if 0 /* old behaviour: use NOTICE */
static void _hook_(const char *fmt1, const char *fmt2, va_list arg2, ...)
{
	char *tag, *data, *mem1, *mem2, buf1[256], buf2[2000];
	va_list arg1;

	data = _pbuf_(fmt2, arg2, &mem2, buf2, sizeof buf2, NULL);

	va_start(arg1, arg2);
	tag = _pbuf_(fmt1, arg1, &mem1, buf1, sizeof buf1, NULL);
	va_end(arg1);

	NOTICE("[HOOK %s] %s", tag, data);

	free(mem1);
	free(mem2);
}
#else /* new behaviour: emits directly to stderr */
static void _hook_(const char *fmt1, const char *fmt2, va_list arg2, ...)
{
	static const char chars[] = "HOOK: [] \n";
	char *mem1, *mem2, buf1[256], buf2[2000];
	struct iovec iov[5];
	va_list arg1;

	iov[0].iov_base = (void*)&chars[0];
	iov[0].iov_len = 7;

	va_start(arg1, arg2);
	iov[1].iov_base = _pbuf_(fmt1, arg1, &mem1, buf1, sizeof buf1, &iov[1].iov_len);
	va_end(arg1);

	iov[2].iov_base = (void*)&chars[7];
	iov[2].iov_len = 2;

	iov[3].iov_base = _pbuf_(fmt2, arg2, &mem2, buf2, sizeof buf2, &iov[3].iov_len);

	iov[4].iov_base = (void*)&chars[9];
	iov[4].iov_len = 1;

	writev(2, iov, 5);

	free(mem1);
	free(mem2);
}
#endif

static void _hook_xreq_(const struct afb_xreq *xreq, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	_hook_("xreq-%06d:%s/%s", format, ap, xreq->hookindex, xreq->api, xreq->verb);
	va_end(ap);
}

static void hook_xreq_begin_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq)
{
	if (!xreq->cred)
		_hook_xreq_(xreq, "BEGIN");
	else
		_hook_xreq_(xreq, "BEGIN uid=%d=%s gid=%d pid=%d label=%s id=%s",
			(int)xreq->cred->uid,
			xreq->cred->user,
			(int)xreq->cred->gid,
			(int)xreq->cred->pid,
			xreq->cred->label?:"(null)",
			xreq->cred->id?:"(null)"
		);
}

static void hook_xreq_end_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "END");
}

static void hook_xreq_json_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, struct json_object *obj)
{
	_hook_xreq_(xreq, "json() -> %s", json_object_to_json_string(obj));
}

static void hook_xreq_get_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, const char *name, struct afb_arg arg)
{
	_hook_xreq_(xreq, "get(%s) -> { name: %s, value: %s, path: %s }", name, arg.name, arg.value, arg.path);
}

static void hook_xreq_success_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	_hook_xreq_(xreq, "success(%s, %s)", json_object_to_json_string(obj), info);
}

static void hook_xreq_fail_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, const char *status, const char *info)
{
	_hook_xreq_(xreq, "fail(%s, %s)", status, info);
}

static void hook_xreq_context_get_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, void *value)
{
	_hook_xreq_(xreq, "context_get() -> %p", value);
}

static void hook_xreq_context_set_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, void *value, void (*free_value)(void*))
{
	_hook_xreq_(xreq, "context_set(%p, %p)", value, free_value);
}

static void hook_xreq_addref_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "addref()");
}

static void hook_xreq_unref_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "unref()");
}

static void hook_xreq_session_close_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "session_close()");
}

static void hook_xreq_session_set_LOA_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, unsigned level, int result)
{
	_hook_xreq_(xreq, "session_set_LOA(%u) -> %d", level, result);
}

static void hook_xreq_subscribe_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, struct afb_event event, int result)
{
	_hook_xreq_(xreq, "subscribe(%s:%d) -> %d", afb_evt_event_name(event), afb_evt_event_id(event), result);
}

static void hook_xreq_unsubscribe_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, struct afb_event event, int result)
{
	_hook_xreq_(xreq, "unsubscribe(%s:%d) -> %d", afb_evt_event_name(event), afb_evt_event_id(event), result);
}

static void hook_xreq_subcall_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	_hook_xreq_(xreq, "subcall(%s/%s, %s) ...", api, verb, json_object_to_json_string(args));
}

static void hook_xreq_subcall_result_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, int status, struct json_object *result)
{
	_hook_xreq_(xreq, "    ...subcall... -> %d: %s", status, json_object_to_json_string(result));
}

static void hook_xreq_subcallsync_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	_hook_xreq_(xreq, "subcallsync(%s/%s, %s) ...", api, verb, json_object_to_json_string(args));
}

static void hook_xreq_subcallsync_result_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, int status, struct json_object *result)
{
	_hook_xreq_(xreq, "    ...subcallsync... -> %d: %s", status, json_object_to_json_string(result));
}

static void hook_xreq_vverbose_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	int len;
	char *msg;
	va_list ap;

	va_copy(ap, args);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (len < 0)
		_hook_xreq_(xreq, "vverbose(%d, %s, %d, %s) -> %s ? ? ?", level, file, line, func, fmt);
	else {
		_hook_xreq_(xreq, "vverbose(%d, %s, %d, %s) -> %s", level, file, line, func, msg);
		free(msg);
	}
}

static void hook_xreq_store_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, struct afb_stored_req *sreq)
{
	_hook_xreq_(xreq, "store() -> %p", sreq);
}

static void hook_xreq_unstore_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "unstore()");
}

static void hook_xreq_subcall_req_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	_hook_xreq_(xreq, "subcall_req(%s/%s, %s) ...", api, verb, json_object_to_json_string(args));
}

static void hook_xreq_subcall_req_result_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, int status, struct json_object *result)
{
	_hook_xreq_(xreq, "    ...subcall_req... -> %d: %s", status, json_object_to_json_string(result));
}

static void hook_xreq_has_permission_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_xreq *xreq, const char *permission, int result)
{
	_hook_xreq_(xreq, "has_permission(%s) -> %d", permission, result);
}

static struct afb_hook_xreq_itf hook_xreq_default_itf = {
	.hook_xreq_begin = hook_xreq_begin_default_cb,
	.hook_xreq_end = hook_xreq_end_default_cb,
	.hook_xreq_json = hook_xreq_json_default_cb,
	.hook_xreq_get = hook_xreq_get_default_cb,
	.hook_xreq_success = hook_xreq_success_default_cb,
	.hook_xreq_fail = hook_xreq_fail_default_cb,
	.hook_xreq_context_get = hook_xreq_context_get_default_cb,
	.hook_xreq_context_set = hook_xreq_context_set_default_cb,
	.hook_xreq_addref = hook_xreq_addref_default_cb,
	.hook_xreq_unref = hook_xreq_unref_default_cb,
	.hook_xreq_session_close = hook_xreq_session_close_default_cb,
	.hook_xreq_session_set_LOA = hook_xreq_session_set_LOA_default_cb,
	.hook_xreq_subscribe = hook_xreq_subscribe_default_cb,
	.hook_xreq_unsubscribe = hook_xreq_unsubscribe_default_cb,
	.hook_xreq_subcall = hook_xreq_subcall_default_cb,
	.hook_xreq_subcall_result = hook_xreq_subcall_result_default_cb,
	.hook_xreq_subcallsync = hook_xreq_subcallsync_default_cb,
	.hook_xreq_subcallsync_result = hook_xreq_subcallsync_result_default_cb,
	.hook_xreq_vverbose = hook_xreq_vverbose_default_cb,
	.hook_xreq_store = hook_xreq_store_default_cb,
	.hook_xreq_unstore = hook_xreq_unstore_default_cb,
	.hook_xreq_subcall_req = hook_xreq_subcall_req_default_cb,
	.hook_xreq_subcall_req_result = hook_xreq_subcall_req_result_default_cb,
	.hook_xreq_has_permission = hook_xreq_has_permission_default_cb
};

/******************************************************************************
 * section: hooks for tracing requests
 *****************************************************************************/

#define _HOOK_XREQ_(what,...)   \
	struct afb_hook_xreq *hook; \
	struct afb_hookid hookid; \
	pthread_rwlock_rdlock(&rwlock); \
	init_hookid(&hookid); \
	hook = list_of_xreq_hooks; \
	while (hook) { \
		if (hook->itf->hook_xreq_##what \
		 && (hook->flags & afb_hook_flag_req_##what) != 0 \
		 && (!hook->session || hook->session == xreq->context.session) \
		 && (!hook->api || !strcasecmp(hook->api, xreq->api)) \
		 && (!hook->verb || !strcasecmp(hook->verb, xreq->verb))) { \
			hook->itf->hook_xreq_##what(hook->closure, &hookid, __VA_ARGS__); \
		} \
		hook = hook->next; \
	} \
	pthread_rwlock_unlock(&rwlock);


void afb_hook_xreq_begin(const struct afb_xreq *xreq)
{
	_HOOK_XREQ_(begin, xreq);
}

void afb_hook_xreq_end(const struct afb_xreq *xreq)
{
	_HOOK_XREQ_(end, xreq);
}

struct json_object *afb_hook_xreq_json(const struct afb_xreq *xreq, struct json_object *obj)
{
	_HOOK_XREQ_(json, xreq, obj);
	return obj;
}

struct afb_arg afb_hook_xreq_get(const struct afb_xreq *xreq, const char *name, struct afb_arg arg)
{
	_HOOK_XREQ_(get, xreq, name, arg);
	return arg;
}

void afb_hook_xreq_success(const struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	_HOOK_XREQ_(success, xreq, obj, info);
}

void afb_hook_xreq_fail(const struct afb_xreq *xreq, const char *status, const char *info)
{
	_HOOK_XREQ_(fail, xreq, status, info);
}

void *afb_hook_xreq_context_get(const struct afb_xreq *xreq, void *value)
{
	_HOOK_XREQ_(context_get, xreq, value);
	return value;
}

void afb_hook_xreq_context_set(const struct afb_xreq *xreq, void *value, void (*free_value)(void*))
{
	_HOOK_XREQ_(context_set, xreq, value, free_value);
}

void afb_hook_xreq_addref(const struct afb_xreq *xreq)
{
	_HOOK_XREQ_(addref, xreq);
}

void afb_hook_xreq_unref(const struct afb_xreq *xreq)
{
	_HOOK_XREQ_(unref, xreq);
}

void afb_hook_xreq_session_close(const struct afb_xreq *xreq)
{
	_HOOK_XREQ_(session_close, xreq);
}

int afb_hook_xreq_session_set_LOA(const struct afb_xreq *xreq, unsigned level, int result)
{
	_HOOK_XREQ_(session_set_LOA, xreq, level, result);
	return result;
}

int afb_hook_xreq_subscribe(const struct afb_xreq *xreq, struct afb_event event, int result)
{
	_HOOK_XREQ_(subscribe, xreq, event, result);
	return result;
}

int afb_hook_xreq_unsubscribe(const struct afb_xreq *xreq, struct afb_event event, int result)
{
	_HOOK_XREQ_(unsubscribe, xreq, event, result);
	return result;
}

void afb_hook_xreq_subcall(const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	_HOOK_XREQ_(subcall, xreq, api, verb, args);
}

void afb_hook_xreq_subcall_result(const struct afb_xreq *xreq, int status, struct json_object *result)
{
	_HOOK_XREQ_(subcall_result, xreq, status, result);
}

void afb_hook_xreq_subcallsync(const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	_HOOK_XREQ_(subcallsync, xreq, api, verb, args);
}

int afb_hook_xreq_subcallsync_result(const struct afb_xreq *xreq, int status, struct json_object *result)
{
	_HOOK_XREQ_(subcallsync_result, xreq, status, result);
	return status;
}

void afb_hook_xreq_vverbose(const struct afb_xreq *xreq, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	_HOOK_XREQ_(vverbose, xreq, level, file ?: "?", line, func ?: "?", fmt, args);
}

void afb_hook_xreq_store(const struct afb_xreq *xreq, struct afb_stored_req *sreq)
{
	_HOOK_XREQ_(store, xreq, sreq);
}

void afb_hook_xreq_unstore(const struct afb_xreq *xreq)
{
	_HOOK_XREQ_(unstore, xreq);
}

void afb_hook_xreq_subcall_req(const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	_HOOK_XREQ_(subcall_req, xreq, api, verb, args);
}

void afb_hook_xreq_subcall_req_result(const struct afb_xreq *xreq, int status, struct json_object *result)
{
	_HOOK_XREQ_(subcall_req_result, xreq, status, result);
}

int afb_hook_xreq_has_permission(const struct afb_xreq *xreq, const char *permission, int result)
{
	_HOOK_XREQ_(has_permission, xreq, permission, result);
	return result;
}

/******************************************************************************
 * section: hooking xreqs
 *****************************************************************************/

void afb_hook_init_xreq(struct afb_xreq *xreq)
{
	static int reqindex;

	int f, flags;
	int add;
	struct afb_hook_xreq *hook;

	/* scan hook list to get the expected flags */
	flags = 0;
	pthread_rwlock_rdlock(&rwlock);
	hook = list_of_xreq_hooks;
	while (hook) {
		f = hook->flags & afb_hook_flags_req_all;
		add = f != 0
		   && (!hook->session || hook->session == xreq->context.session)
		   && (!hook->api || !strcasecmp(hook->api, xreq->api))
		   && (!hook->verb || !strcasecmp(hook->verb, xreq->verb));
		if (add)
			flags |= f;
		hook = hook->next;
	}
	pthread_rwlock_unlock(&rwlock);

	/* store the hooking data */
	xreq->hookflags = flags;
	if (flags) {
		pthread_rwlock_wrlock(&rwlock);
		if (++reqindex < 0)
			reqindex = 1;
		xreq->hookindex = reqindex;
		pthread_rwlock_unlock(&rwlock);
	}
}

struct afb_hook_xreq *afb_hook_create_xreq(const char *api, const char *verb, struct afb_session *session, int flags, struct afb_hook_xreq_itf *itf, void *closure)
{
	struct afb_hook_xreq *hook;

	/* alloc the result */
	hook = calloc(1, sizeof *hook);
	if (hook == NULL)
		return NULL;

	/* get a copy of the names */
	hook->api = api ? strdup(api) : NULL;
	hook->verb = verb ? strdup(verb) : NULL;
	if ((api && !hook->api) || (verb && !hook->verb)) {
		free(hook->api);
		free(hook->verb);
		free(hook);
		return NULL;
	}

	/* initialise the rest */
	hook->session = session;
	if (session)
		afb_session_addref(session);
	hook->refcount = 1;
	hook->flags = flags;
	hook->itf = itf ? itf : &hook_xreq_default_itf;
	hook->closure = closure;

	/* record the hook */
	pthread_rwlock_wrlock(&rwlock);
	hook->next = list_of_xreq_hooks;
	list_of_xreq_hooks = hook;
	pthread_rwlock_unlock(&rwlock);

	/* returns it */
	return hook;
}

struct afb_hook_xreq *afb_hook_addref_xreq(struct afb_hook_xreq *hook)
{
	pthread_rwlock_wrlock(&rwlock);
	hook->refcount++;
	pthread_rwlock_unlock(&rwlock);
	return hook;
}

void afb_hook_unref_xreq(struct afb_hook_xreq *hook)
{
	struct afb_hook_xreq **prv;

	if (hook) {
		pthread_rwlock_wrlock(&rwlock);
		if (--hook->refcount)
			hook = NULL;
		else {
			/* unlink */
			prv = &list_of_xreq_hooks;
			while (*prv && *prv != hook)
				prv = &(*prv)->next;
			if(*prv)
				*prv = hook->next;
		}
		pthread_rwlock_unlock(&rwlock);
		if (hook) {
			/* free */
			free(hook->api);
			free(hook->verb);
			if (hook->session)
				afb_session_unref(hook->session);
			free(hook);
		}
	}
}

/******************************************************************************
 * section: default callbacks for tracing daemon interface
 *****************************************************************************/

static void _hook_ditf_(const struct afb_ditf *ditf, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	_hook_("ditf-%s", format, ap, ditf->api);
	va_end(ap);
}

static void hook_ditf_event_broadcast_before_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, const char *name, struct json_object *object)
{
	_hook_ditf_(ditf, "event_broadcast.before(%s, %s)....", name, json_object_to_json_string(object));
}

static void hook_ditf_event_broadcast_after_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, const char *name, struct json_object *object, int result)
{
	_hook_ditf_(ditf, "event_broadcast.after(%s, %s) -> %d", name, json_object_to_json_string(object), result);
}

static void hook_ditf_get_event_loop_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, struct sd_event *result)
{
	_hook_ditf_(ditf, "get_event_loop() -> %p", result);
}

static void hook_ditf_get_user_bus_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, struct sd_bus *result)
{
	_hook_ditf_(ditf, "get_user_bus() -> %p", result);
}

static void hook_ditf_get_system_bus_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, struct sd_bus *result)
{
	_hook_ditf_(ditf, "get_system_bus() -> %p", result);
}

static void hook_ditf_vverbose_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, int level, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	int len;
	char *msg;
	va_list ap;

	va_copy(ap, args);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (len < 0)
		_hook_ditf_(ditf, "vverbose(%d, %s, %d, %s) -> %s ? ? ?", level, file, line, function, fmt);
	else {
		_hook_ditf_(ditf, "vverbose(%d, %s, %d, %s) -> %s", level, file, line, function, msg);
		free(msg);
	}
}

static void hook_ditf_event_make_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, const char *name, struct afb_event result)
{
	_hook_ditf_(ditf, "event_make(%s) -> %s:%d", name, afb_evt_event_name(result), afb_evt_event_id(result));
}

static void hook_ditf_rootdir_get_fd_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, int result)
{
	char path[PATH_MAX];
	if (result < 0)
		_hook_ditf_(ditf, "rootdir_get_fd() -> %d, %m", result);
	else {
		sprintf(path, "/proc/self/fd/%d", result);
		readlink(path, path, sizeof path);
		_hook_ditf_(ditf, "rootdir_get_fd() -> %d = %s", result, path);
	}
}

static void hook_ditf_rootdir_open_locale_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, const char *filename, int flags, const char *locale, int result)
{
	char path[PATH_MAX];
	if (!locale)
		locale = "(null)";
	if (result < 0)
		_hook_ditf_(ditf, "rootdir_open_locale(%s, %d, %s) -> %d, %m", filename, flags, locale, result);
	else {
		sprintf(path, "/proc/self/fd/%d", result);
		readlink(path, path, sizeof path);
		_hook_ditf_(ditf, "rootdir_open_locale(%s, %d, %s) -> %d = %s", filename, flags, locale, result, path);
	}
}

static void hook_ditf_queue_job_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout, int result)
{
	_hook_ditf_(ditf, "queue_job(%p, %p, %p, %d) -> %d", callback, argument, group, timeout, result);
}

static void hook_ditf_unstore_req_cb(void *closure, const struct afb_hookid *hookid,  const struct afb_ditf *ditf, struct afb_stored_req *sreq)
{
	_hook_ditf_(ditf, "unstore_req(%p)", sreq);
}

static void hook_ditf_require_api_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, const char *name, int initialized)
{
	_hook_ditf_(ditf, "require_api(%s, %d)...", name, initialized);
}

static void hook_ditf_require_api_result_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, const char *name, int initialized, int result)
{
	_hook_ditf_(ditf, "...require_api(%s, %d) -> %d", name, initialized, result);
}

static void hook_ditf_rename_api_cb(void *closure, const struct afb_hookid *hookid, const struct afb_ditf *ditf, const char *oldname, const char *newname, int result)
{
	_hook_ditf_(ditf, "rename_api(%s -> %s) -> %d", oldname, newname, result);
}

static struct afb_hook_ditf_itf hook_ditf_default_itf = {
	.hook_ditf_event_broadcast_before = hook_ditf_event_broadcast_before_cb,
	.hook_ditf_event_broadcast_after = hook_ditf_event_broadcast_after_cb,
	.hook_ditf_get_event_loop = hook_ditf_get_event_loop_cb,
	.hook_ditf_get_user_bus = hook_ditf_get_user_bus_cb,
	.hook_ditf_get_system_bus = hook_ditf_get_system_bus_cb,
	.hook_ditf_vverbose = hook_ditf_vverbose_cb,
	.hook_ditf_event_make = hook_ditf_event_make_cb,
	.hook_ditf_rootdir_get_fd = hook_ditf_rootdir_get_fd_cb,
	.hook_ditf_rootdir_open_locale = hook_ditf_rootdir_open_locale_cb,
	.hook_ditf_queue_job = hook_ditf_queue_job_cb,
	.hook_ditf_unstore_req = hook_ditf_unstore_req_cb,
	.hook_ditf_require_api = hook_ditf_require_api_cb,
	.hook_ditf_require_api_result = hook_ditf_require_api_result_cb,
	.hook_ditf_rename_api = hook_ditf_rename_api_cb
};

/******************************************************************************
 * section: hooks for tracing daemon interface (ditf)
 *****************************************************************************/

#define _HOOK_DITF_(what,...)   \
	struct afb_hook_ditf *hook; \
	struct afb_hookid hookid; \
	pthread_rwlock_rdlock(&rwlock); \
	init_hookid(&hookid); \
	hook = list_of_ditf_hooks; \
	while (hook) { \
		if (hook->itf->hook_ditf_##what \
		 && (hook->flags & afb_hook_flag_ditf_##what) != 0 \
		 && (!hook->api || !strcasecmp(hook->api, ditf->api))) { \
			hook->itf->hook_ditf_##what(hook->closure, &hookid, __VA_ARGS__); \
		} \
		hook = hook->next; \
	} \
	pthread_rwlock_unlock(&rwlock);

void afb_hook_ditf_event_broadcast_before(const struct afb_ditf *ditf, const char *name, struct json_object *object)
{
	_HOOK_DITF_(event_broadcast_before, ditf, name, object);
}

int afb_hook_ditf_event_broadcast_after(const struct afb_ditf *ditf, const char *name, struct json_object *object, int result)
{
	_HOOK_DITF_(event_broadcast_after, ditf, name, object, result);
	return result;
}

struct sd_event *afb_hook_ditf_get_event_loop(const struct afb_ditf *ditf, struct sd_event *result)
{
	_HOOK_DITF_(get_event_loop, ditf, result);
	return result;
}

struct sd_bus *afb_hook_ditf_get_user_bus(const struct afb_ditf *ditf, struct sd_bus *result)
{
	_HOOK_DITF_(get_user_bus, ditf, result);
	return result;
}

struct sd_bus *afb_hook_ditf_get_system_bus(const struct afb_ditf *ditf, struct sd_bus *result)
{
	_HOOK_DITF_(get_system_bus, ditf, result);
	return result;
}

void afb_hook_ditf_vverbose(const struct afb_ditf *ditf, int level, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	_HOOK_DITF_(vverbose, ditf, level, file, line, function, fmt, args);
}

struct afb_event afb_hook_ditf_event_make(const struct afb_ditf *ditf, const char *name, struct afb_event result)
{
	_HOOK_DITF_(event_make, ditf, name, result);
	return result;
}

int afb_hook_ditf_rootdir_get_fd(const struct afb_ditf *ditf, int result)
{
	_HOOK_DITF_(rootdir_get_fd, ditf, result);
	return result;
}

int afb_hook_ditf_rootdir_open_locale(const struct afb_ditf *ditf, const char *filename, int flags, const char *locale, int result)
{
	_HOOK_DITF_(rootdir_open_locale, ditf, filename, flags, locale, result);
	return result;
}

int afb_hook_ditf_queue_job(const struct afb_ditf *ditf, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout, int result)
{
	_HOOK_DITF_(queue_job, ditf, callback, argument, group, timeout, result);
	return result;
}

void afb_hook_ditf_unstore_req(const struct afb_ditf *ditf, struct afb_stored_req *sreq)
{
	_HOOK_DITF_(unstore_req, ditf, sreq);
}

void afb_hook_ditf_require_api(const struct afb_ditf *ditf, const char *name, int initialized)
{
	_HOOK_DITF_(require_api, ditf, name, initialized);
}

int afb_hook_ditf_require_api_result(const struct afb_ditf *ditf, const char *name, int initialized, int result)
{
	_HOOK_DITF_(require_api_result, ditf, name, initialized, result);
	return result;
}

int afb_hook_ditf_rename_api(const struct afb_ditf *ditf, const char *oldname, const char *newname, int result)
{
	_HOOK_DITF_(rename_api, ditf, oldname, newname, result);
	return result;
}

/******************************************************************************
 * section: hooking ditf
 *****************************************************************************/

int afb_hook_flags_ditf(const char *api)
{
	int flags;
	struct afb_hook_ditf *hook;

	pthread_rwlock_rdlock(&rwlock);
	flags = 0;
	hook = list_of_ditf_hooks;
	while (hook) {
		if (!api || !hook->api || !strcasecmp(hook->api, api))
			flags |= hook->flags;
		hook = hook->next;
	}
	pthread_rwlock_unlock(&rwlock);
	return flags;
}

struct afb_hook_ditf *afb_hook_create_ditf(const char *api, int flags, struct afb_hook_ditf_itf *itf, void *closure)
{
	struct afb_hook_ditf *hook;

	/* alloc the result */
	hook = calloc(1, sizeof *hook);
	if (hook == NULL)
		return NULL;

	/* get a copy of the names */
	hook->api = api ? strdup(api) : NULL;
	if (api && !hook->api) {
		free(hook);
		return NULL;
	}

	/* initialise the rest */
	hook->refcount = 1;
	hook->flags = flags;
	hook->itf = itf ? itf : &hook_ditf_default_itf;
	hook->closure = closure;

	/* record the hook */
	pthread_rwlock_wrlock(&rwlock);
	hook->next = list_of_ditf_hooks;
	list_of_ditf_hooks = hook;
	pthread_rwlock_unlock(&rwlock);

	/* returns it */
	return hook;
}

struct afb_hook_ditf *afb_hook_addref_ditf(struct afb_hook_ditf *hook)
{
	pthread_rwlock_wrlock(&rwlock);
	hook->refcount++;
	pthread_rwlock_unlock(&rwlock);
	return hook;
}

void afb_hook_unref_ditf(struct afb_hook_ditf *hook)
{
	struct afb_hook_ditf **prv;

	if (hook) {
		pthread_rwlock_wrlock(&rwlock);
		if (--hook->refcount)
			hook = NULL;
		else {
			/* unlink */
			prv = &list_of_ditf_hooks;
			while (*prv && *prv != hook)
				prv = &(*prv)->next;
			if(*prv)
				*prv = hook->next;
		}
		pthread_rwlock_unlock(&rwlock);
		if (hook) {
			/* free */
			free(hook->api);
			free(hook);
		}
	}
}

/******************************************************************************
 * section: default callbacks for tracing service interface (svc)
 *****************************************************************************/

static void _hook_svc_(const struct afb_svc *svc, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	_hook_("svc-%s", format, ap, svc->api);
	va_end(ap);
}

static void hook_svc_start_before_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_svc *svc)
{
	_hook_svc_(svc, "start.before");
}

static void hook_svc_start_after_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_svc *svc, int status)
{
	_hook_svc_(svc, "start.after -> %d", status);
}

static void hook_svc_on_event_before_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_svc *svc, const char *event, int eventid, struct json_object *object)
{
	_hook_svc_(svc, "on_event.before(%s, %d, %s)", event, eventid, json_object_to_json_string(object));
}

static void hook_svc_on_event_after_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_svc *svc, const char *event, int eventid, struct json_object *object)
{
	_hook_svc_(svc, "on_event.after(%s, %d, %s)", event, eventid, json_object_to_json_string(object));
}

static void hook_svc_call_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args)
{
	_hook_svc_(svc, "call(%s/%s, %s) ...", api, verb, json_object_to_json_string(args));
}

static void hook_svc_call_result_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_svc *svc, int status, struct json_object *result)
{
	_hook_svc_(svc, "    ...call... -> %d: %s", status, json_object_to_json_string(result));
}

static void hook_svc_callsync_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args)
{
	_hook_svc_(svc, "callsync(%s/%s, %s) ...", api, verb, json_object_to_json_string(args));
}

static void hook_svc_callsync_result_default_cb(void *closure, const struct afb_hookid *hookid, const struct afb_svc *svc, int status, struct json_object *result)
{
	_hook_svc_(svc, "    ...callsync... -> %d: %s", status, json_object_to_json_string(result));
}

static struct afb_hook_svc_itf hook_svc_default_itf = {
	.hook_svc_start_before = hook_svc_start_before_default_cb,
	.hook_svc_start_after = hook_svc_start_after_default_cb,
	.hook_svc_on_event_before = hook_svc_on_event_before_default_cb,
	.hook_svc_on_event_after = hook_svc_on_event_after_default_cb,
	.hook_svc_call = hook_svc_call_default_cb,
	.hook_svc_call_result = hook_svc_call_result_default_cb,
	.hook_svc_callsync = hook_svc_callsync_default_cb,
	.hook_svc_callsync_result = hook_svc_callsync_result_default_cb
};

/******************************************************************************
 * section: hooks for tracing service interface (svc)
 *****************************************************************************/

#define _HOOK_SVC_(what,...)   \
	struct afb_hook_svc *hook; \
	struct afb_hookid hookid; \
	pthread_rwlock_rdlock(&rwlock); \
	init_hookid(&hookid); \
	hook = list_of_svc_hooks; \
	while (hook) { \
		if (hook->itf->hook_svc_##what \
		 && (hook->flags & afb_hook_flag_svc_##what) != 0 \
		 && (!hook->api || !strcasecmp(hook->api, svc->api))) { \
			hook->itf->hook_svc_##what(hook->closure, &hookid, __VA_ARGS__); \
		} \
		hook = hook->next; \
	} \
	pthread_rwlock_unlock(&rwlock);

void afb_hook_svc_start_before(const struct afb_svc *svc)
{
	_HOOK_SVC_(start_before, svc);
}

int afb_hook_svc_start_after(const struct afb_svc *svc, int status)
{
	_HOOK_SVC_(start_after, svc, status);
	return status;
}

void afb_hook_svc_on_event_before(const struct afb_svc *svc, const char *event, int eventid, struct json_object *object)
{
	_HOOK_SVC_(on_event_before, svc, event, eventid, object);
}

void afb_hook_svc_on_event_after(const struct afb_svc *svc, const char *event, int eventid, struct json_object *object)
{
	_HOOK_SVC_(on_event_after, svc, event, eventid, object);
}

void afb_hook_svc_call(const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args)
{
	_HOOK_SVC_(call, svc, api, verb, args);
}

void afb_hook_svc_call_result(const struct afb_svc *svc, int status, struct json_object *result)
{
	_HOOK_SVC_(call_result, svc, status, result);
}

void afb_hook_svc_callsync(const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args)
{
	_HOOK_SVC_(callsync, svc, api, verb, args);
}

int afb_hook_svc_callsync_result(const struct afb_svc *svc, int status, struct json_object *result)
{
	_HOOK_SVC_(callsync_result, svc, status, result);
	return status;
}

/******************************************************************************
 * section: hooking services (svc)
 *****************************************************************************/

int afb_hook_flags_svc(const char *api)
{
	int flags;
	struct afb_hook_svc *hook;

	pthread_rwlock_rdlock(&rwlock);
	flags = 0;
	hook = list_of_svc_hooks;
	while (hook) {
		if (!api || !hook->api || !strcasecmp(hook->api, api))
			flags |= hook->flags;
		hook = hook->next;
	}
	pthread_rwlock_unlock(&rwlock);
	return flags;
}

struct afb_hook_svc *afb_hook_create_svc(const char *api, int flags, struct afb_hook_svc_itf *itf, void *closure)
{
	struct afb_hook_svc *hook;

	/* alloc the result */
	hook = calloc(1, sizeof *hook);
	if (hook == NULL)
		return NULL;

	/* get a copy of the names */
	hook->api = api ? strdup(api) : NULL;
	if (api && !hook->api) {
		free(hook);
		return NULL;
	}

	/* initialise the rest */
	hook->refcount = 1;
	hook->flags = flags;
	hook->itf = itf ? itf : &hook_svc_default_itf;
	hook->closure = closure;

	/* record the hook */
	pthread_rwlock_wrlock(&rwlock);
	hook->next = list_of_svc_hooks;
	list_of_svc_hooks = hook;
	pthread_rwlock_unlock(&rwlock);

	/* returns it */
	return hook;
}

struct afb_hook_svc *afb_hook_addref_svc(struct afb_hook_svc *hook)
{
	pthread_rwlock_wrlock(&rwlock);
	hook->refcount++;
	pthread_rwlock_unlock(&rwlock);
	return hook;
}

void afb_hook_unref_svc(struct afb_hook_svc *hook)
{
	struct afb_hook_svc **prv;

	if (hook) {
		pthread_rwlock_wrlock(&rwlock);
		if (--hook->refcount)
			hook = NULL;
		else {
			/* unlink */
			prv = &list_of_svc_hooks;
			while (*prv && *prv != hook)
				prv = &(*prv)->next;
			if(*prv)
				*prv = hook->next;
		}
		pthread_rwlock_unlock(&rwlock);
		if (hook) {
			/* free */
			free(hook->api);
			free(hook);
		}
	}
}

/******************************************************************************
 * section: default callbacks for tracing service interface (evt)
 *****************************************************************************/

static void _hook_evt_(const char *evt, int id, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	_hook_("evt-%s:%d", format, ap, evt, id);
	va_end(ap);
}

static void hook_evt_create_default_cb(void *closure, const struct afb_hookid *hookid, const char *evt, int id)
{
	_hook_evt_(evt, id, "create");
}

static void hook_evt_push_before_default_cb(void *closure, const struct afb_hookid *hookid, const char *evt, int id, struct json_object *obj)
{
	_hook_evt_(evt, id, "push.before(%s)", json_object_to_json_string(obj));
}


static void hook_evt_push_after_default_cb(void *closure, const struct afb_hookid *hookid, const char *evt, int id, struct json_object *obj, int result)
{
	_hook_evt_(evt, id, "push.after(%s) -> %d", json_object_to_json_string(obj), result);
}

static void hook_evt_broadcast_before_default_cb(void *closure, const struct afb_hookid *hookid, const char *evt, int id, struct json_object *obj)
{
	_hook_evt_(evt, id, "broadcast.before(%s)", json_object_to_json_string(obj));
}

static void hook_evt_broadcast_after_default_cb(void *closure, const struct afb_hookid *hookid, const char *evt, int id, struct json_object *obj, int result)
{
	_hook_evt_(evt, id, "broadcast.after(%s) -> %d", json_object_to_json_string(obj), result);
}

static void hook_evt_name_default_cb(void *closure, const struct afb_hookid *hookid, const char *evt, int id)
{
	_hook_evt_(evt, id, "name");
}

static void hook_evt_drop_default_cb(void *closure, const struct afb_hookid *hookid, const char *evt, int id)
{
	_hook_evt_(evt, id, "drop");
}

static struct afb_hook_evt_itf hook_evt_default_itf = {
	.hook_evt_create = hook_evt_create_default_cb,
	.hook_evt_push_before = hook_evt_push_before_default_cb,
	.hook_evt_push_after = hook_evt_push_after_default_cb,
	.hook_evt_broadcast_before = hook_evt_broadcast_before_default_cb,
	.hook_evt_broadcast_after = hook_evt_broadcast_after_default_cb,
	.hook_evt_name = hook_evt_name_default_cb,
	.hook_evt_drop = hook_evt_drop_default_cb
};

/******************************************************************************
 * section: hooks for tracing events interface (evt)
 *****************************************************************************/

#define _HOOK_EVT_(what,...)   \
	struct afb_hook_evt *hook; \
	struct afb_hookid hookid; \
	pthread_rwlock_rdlock(&rwlock); \
	init_hookid(&hookid); \
	hook = list_of_evt_hooks; \
	while (hook) { \
		if (hook->itf->hook_evt_##what \
		 && (hook->flags & afb_hook_flag_evt_##what) != 0 \
		 && (!hook->pattern || !fnmatch(hook->pattern, evt, FNM_CASEFOLD))) { \
			hook->itf->hook_evt_##what(hook->closure, &hookid, __VA_ARGS__); \
		} \
		hook = hook->next; \
	} \
	pthread_rwlock_unlock(&rwlock);

void afb_hook_evt_create(const char *evt, int id)
{
	_HOOK_EVT_(create, evt, id);
}

void afb_hook_evt_push_before(const char *evt, int id, struct json_object *obj)
{
	_HOOK_EVT_(push_before, evt, id, obj);
}

int afb_hook_evt_push_after(const char *evt, int id, struct json_object *obj, int result)
{
	_HOOK_EVT_(push_after, evt, id, obj, result);
	return result;
}

void afb_hook_evt_broadcast_before(const char *evt, int id, struct json_object *obj)
{
	_HOOK_EVT_(broadcast_before, evt, id, obj);
}

int afb_hook_evt_broadcast_after(const char *evt, int id, struct json_object *obj, int result)
{
	_HOOK_EVT_(broadcast_after, evt, id, obj, result);
	return result;
}

void afb_hook_evt_name(const char *evt, int id)
{
	_HOOK_EVT_(name, evt, id);
}

void afb_hook_evt_drop(const char *evt, int id)
{
	_HOOK_EVT_(drop, evt, id);
}

/******************************************************************************
 * section: hooking services (evt)
 *****************************************************************************/

int afb_hook_flags_evt(const char *name)
{
	int flags;
	struct afb_hook_evt *hook;

	pthread_rwlock_rdlock(&rwlock);
	flags = 0;
	hook = list_of_evt_hooks;
	while (hook) {
		if (!name || !hook->pattern || !fnmatch(hook->pattern, name, FNM_CASEFOLD))
			flags |= hook->flags;
		hook = hook->next;
	}
	pthread_rwlock_unlock(&rwlock);
	return flags;
}

struct afb_hook_evt *afb_hook_create_evt(const char *pattern, int flags, struct afb_hook_evt_itf *itf, void *closure)
{
	struct afb_hook_evt *hook;

	/* alloc the result */
	hook = calloc(1, sizeof *hook);
	if (hook == NULL)
		return NULL;

	/* get a copy of the names */
	hook->pattern = pattern ? strdup(pattern) : NULL;
	if (pattern && !hook->pattern) {
		free(hook);
		return NULL;
	}

	/* initialise the rest */
	hook->refcount = 1;
	hook->flags = flags;
	hook->itf = itf ? itf : &hook_evt_default_itf;
	hook->closure = closure;

	/* record the hook */
	pthread_rwlock_wrlock(&rwlock);
	hook->next = list_of_evt_hooks;
	list_of_evt_hooks = hook;
	pthread_rwlock_unlock(&rwlock);

	/* returns it */
	return hook;
}

struct afb_hook_evt *afb_hook_addref_evt(struct afb_hook_evt *hook)
{
	pthread_rwlock_wrlock(&rwlock);
	hook->refcount++;
	pthread_rwlock_unlock(&rwlock);
	return hook;
}

void afb_hook_unref_evt(struct afb_hook_evt *hook)
{
	struct afb_hook_evt **prv;

	if (hook) {
		pthread_rwlock_wrlock(&rwlock);
		if (--hook->refcount)
			hook = NULL;
		else {
			/* unlink */
			prv = &list_of_evt_hooks;
			while (*prv && *prv != hook)
				prv = &(*prv)->next;
			if(*prv)
				*prv = hook->next;
		}
		pthread_rwlock_unlock(&rwlock);
		if (hook) {
			/* free */
			free(hook->pattern);
			free(hook);
		}
	}
}

/******************************************************************************
 * section: default callbacks for globals (global)
 *****************************************************************************/

static void _hook_global_(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	_hook_("global", format, ap);
	va_end(ap);
}

static void hook_global_vverbose_default_cb(void *closure, const struct afb_hookid *hookid, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	int len;
	char *msg;
	va_list ap;

	va_copy(ap, args);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (len < 0)
		_hook_global_("vverbose(%d, %s, %d, %s) -> %s ? ? ?", level, file, line, func, fmt);
	else {
		_hook_global_("vverbose(%d, %s, %d, %s) -> %s", level, file, line, func, msg);
		free(msg);
	}
}

static struct afb_hook_global_itf hook_global_default_itf = {
	.hook_global_vverbose = hook_global_vverbose_default_cb
};

/******************************************************************************
 * section: hooks for tracing globals (global)
 *****************************************************************************/

#define _HOOK_GLOBAL_(what,...)   \
	struct afb_hook_global *hook; \
	struct afb_hookid hookid; \
	pthread_rwlock_rdlock(&rwlock); \
	init_hookid(&hookid); \
	hook = list_of_global_hooks; \
	while (hook) { \
		if (hook->itf->hook_global_##what \
		 && (hook->flags & afb_hook_flag_global_##what) != 0) { \
			hook->itf->hook_global_##what(hook->closure, &hookid, __VA_ARGS__); \
		} \
		hook = hook->next; \
	} \
	pthread_rwlock_unlock(&rwlock);

static void afb_hook_global_vverbose(int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	_HOOK_GLOBAL_(vverbose, level, file ?: "?", line, func ?: "?", fmt ?: "", args);
}

/******************************************************************************
 * section: hooking globals (global)
 *****************************************************************************/

static void update_global()
{
	struct afb_hook_global *hook;
	int flags = 0;

	pthread_rwlock_rdlock(&rwlock);
	hook = list_of_global_hooks;
	while (hook) {
		flags = hook->flags;
		hook = hook->next;
	}
	verbose_observer = (flags & afb_hook_flag_global_vverbose) ? afb_hook_global_vverbose : NULL;
	pthread_rwlock_unlock(&rwlock);
}

struct afb_hook_global *afb_hook_create_global(int flags, struct afb_hook_global_itf *itf, void *closure)
{
	struct afb_hook_global *hook;

	/* alloc the result */
	hook = calloc(1, sizeof *hook);
	if (hook == NULL)
		return NULL;

	/* initialise the rest */
	hook->refcount = 1;
	hook->flags = flags;
	hook->itf = itf ? itf : &hook_global_default_itf;
	hook->closure = closure;

	/* record the hook */
	pthread_rwlock_wrlock(&rwlock);
	hook->next = list_of_global_hooks;
	list_of_global_hooks = hook;
	pthread_rwlock_unlock(&rwlock);

	/* update hooking */
	update_global();

	/* returns it */
	return hook;
}

struct afb_hook_global *afb_hook_addref_global(struct afb_hook_global *hook)
{
	pthread_rwlock_wrlock(&rwlock);
	hook->refcount++;
	pthread_rwlock_unlock(&rwlock);
	return hook;
}

void afb_hook_unref_global(struct afb_hook_global *hook)
{
	struct afb_hook_global **prv;

	if (hook) {
		pthread_rwlock_wrlock(&rwlock);
		if (--hook->refcount)
			hook = NULL;
		else {
			/* unlink */
			prv = &list_of_global_hooks;
			while (*prv && *prv != hook)
				prv = &(*prv)->next;
			if(*prv)
				*prv = hook->next;
		}
		pthread_rwlock_unlock(&rwlock);
		if (hook) {
			/* free */
			free(hook);

			/* update hooking */
			update_global();
		}
	}
}

