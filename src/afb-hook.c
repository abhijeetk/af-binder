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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include <json-c/json.h>

#include <afb/afb-req-itf.h>
#include <afb/afb-event-itf.h>

#include "afb-context.h"
#include "afb-hook.h"
#include "afb-session.h"
#include "afb-xreq.h"
#include "verbose.h"

/**
 * Definition of a hook
 */
struct afb_hook {
	struct afb_hook *next; /**< next hook */
	unsigned refcount; /**< reference count */
	char *api; /**< api hooked or NULL for any */
	char *verb; /**< verb hooked or NULL for any */
	struct afb_session *session; /**< session hooked or NULL if any */
	unsigned flags; /**< hook flags */
	struct afb_hook_xreq_itf *reqitf; /**< interface of hook */
	void *closure; /**< closure for callbacks */
};

/*
 * Structure for handling subcalls callbacks
 */
struct hook_subcall {
	struct afb_xreq *xreq; /* hookd request */
	void (*callback)(void*, int, struct json_object*); /* client callback */
	void *cb_closure; /* cient closure */
};

/* synchronisation across threads */
static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

/* list of hooks */
static struct afb_hook *list_of_hooks = NULL;

/******************************************************************************
 * section: default callbacks for tracing requests
 *****************************************************************************/

static void _hook_xreq_(const struct afb_xreq *xreq, const char *format, ...)
{
	int len;
	char *buffer;
	va_list ap;

	va_start(ap, format);
	len = vasprintf(&buffer, format, ap);
	va_end(ap);

	if (len < 0)
		NOTICE("hook xreq-%06d:%s/%s allocation error", xreq->hookindex, xreq->api, xreq->verb);
	else {
		NOTICE("hook xreq-%06d:%s/%s %s", xreq->hookindex, xreq->api, xreq->verb, buffer);
		free(buffer);
	}
}

static void hook_xreq_begin_default_cb(void * closure, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "BEGIN");
}

static void hook_xreq_end_default_cb(void * closure, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "END");
}

static void hook_xreq_json_default_cb(void * closure, const struct afb_xreq *xreq, struct json_object *obj)
{
	_hook_xreq_(xreq, "json() -> %s", json_object_to_json_string(obj));
}

static void hook_xreq_get_default_cb(void * closure, const struct afb_xreq *xreq, const char *name, struct afb_arg arg)
{
	_hook_xreq_(xreq, "get(%s) -> { name: %s, value: %s, path: %s }", name, arg.name, arg.value, arg.path);
}

static void hook_xreq_success_default_cb(void * closure, const struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	_hook_xreq_(xreq, "success(%s, %s)", json_object_to_json_string(obj), info);
}

static void hook_xreq_fail_default_cb(void * closure, const struct afb_xreq *xreq, const char *status, const char *info)
{
	_hook_xreq_(xreq, "fail(%s, %s)", status, info);
}

static void hook_xreq_raw_default_cb(void * closure, const struct afb_xreq *xreq, const char *buffer, size_t size)
{
	_hook_xreq_(xreq, "raw() -> %.*s", (int)size, buffer);
}

static void hook_xreq_send_default_cb(void * closure, const struct afb_xreq *xreq, const char *buffer, size_t size)
{
	_hook_xreq_(xreq, "send(%.*s)", (int)size, buffer);
}

static void hook_xreq_context_get_default_cb(void * closure, const struct afb_xreq *xreq, void *value)
{
	_hook_xreq_(xreq, "context_get() -> %p", value);
}

static void hook_xreq_context_set_default_cb(void * closure, const struct afb_xreq *xreq, void *value, void (*free_value)(void*))
{
	_hook_xreq_(xreq, "context_set(%p, %p)", value, free_value);
}

static void hook_xreq_addref_default_cb(void * closure, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "addref()");
}

static void hook_xreq_unref_default_cb(void * closure, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "unref()");
}

static void hook_xreq_session_close_default_cb(void * closure, const struct afb_xreq *xreq)
{
	_hook_xreq_(xreq, "session_close()");
}

static void hook_xreq_session_set_LOA_default_cb(void * closure, const struct afb_xreq *xreq, unsigned level, int result)
{
	_hook_xreq_(xreq, "session_set_LOA(%u) -> %d", level, result);
}

static void hook_xreq_subscribe_default_cb(void * closure, const struct afb_xreq *xreq, struct afb_event event, int result)
{
	_hook_xreq_(xreq, "subscribe(%s:%p) -> %d", afb_event_name(event), event.closure, result);
}

static void hook_xreq_unsubscribe_default_cb(void * closure, const struct afb_xreq *xreq, struct afb_event event, int result)
{
	_hook_xreq_(xreq, "unsubscribe(%s:%p) -> %d", afb_event_name(event), event.closure, result);
}

static void hook_xreq_subcall_default_cb(void * closure, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	_hook_xreq_(xreq, "subcall(%s/%s, %s) ...", api, verb, json_object_to_json_string(args));
}

static void hook_xreq_subcall_result_default_cb(void * closure, const struct afb_xreq *xreq, int status, struct json_object *result)
{
	_hook_xreq_(xreq, "    ...subcall... -> %d: %s", status, json_object_to_json_string(result));
}

static struct afb_hook_xreq_itf hook_xreq_default_itf = {
	.hook_xreq_begin = hook_xreq_begin_default_cb,
	.hook_xreq_end = hook_xreq_end_default_cb,
	.hook_xreq_json = hook_xreq_json_default_cb,
	.hook_xreq_get = hook_xreq_get_default_cb,
	.hook_xreq_success = hook_xreq_success_default_cb,
	.hook_xreq_fail = hook_xreq_fail_default_cb,
	.hook_xreq_raw = hook_xreq_raw_default_cb,
	.hook_xreq_send = hook_xreq_send_default_cb,
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
};

/******************************************************************************
 * section: hooks for tracing requests
 *****************************************************************************/

#define _HOOK_XREQ_(what,...)   \
	struct afb_hook *hook; \
	pthread_rwlock_rdlock(&rwlock); \
	hook = list_of_hooks; \
	while (hook) { \
		if (hook->reqitf->hook_xreq_##what \
		 && (hook->flags & afb_hook_flag_req_##what) != 0 \
		 && (!hook->session || hook->session == xreq->context.session) \
		 && (!hook->api || !strcasecmp(hook->api, xreq->api)) \
		 && (!hook->verb || !strcasecmp(hook->verb, xreq->verb))) { \
			hook->reqitf->hook_xreq_##what(hook->closure, __VA_ARGS__); \
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

const char *afb_hook_xreq_raw(const struct afb_xreq *xreq, const char *buffer, size_t size)
{
	_HOOK_XREQ_(raw, xreq, buffer, size);
	return buffer;
}

void afb_hook_xreq_send(const struct afb_xreq *xreq, const char *buffer, size_t size)
{
	_HOOK_XREQ_(send, xreq, buffer, size);
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

/******************************************************************************
 * section: 
 *****************************************************************************/

void afb_hook_init_xreq(struct afb_xreq *xreq)
{
	static int reqindex;

	int f, flags;
	int add;
	struct afb_hook *hook;

	/* scan hook list to get the expected flags */
	flags = 0;
	pthread_rwlock_rdlock(&rwlock);
	hook = list_of_hooks;
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

struct afb_hook *afb_hook_xreq_create(const char *api, const char *verb, struct afb_session *session, unsigned flags, struct afb_hook_xreq_itf *itf, void *closure)
{
	struct afb_hook *hook;

	/* alloc the result */
	hook = malloc(sizeof *hook);
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
	hook->reqitf = itf ? itf : &hook_xreq_default_itf;
	hook->closure = closure;

	/* record the hook */
	pthread_rwlock_wrlock(&rwlock);
	hook->next = list_of_hooks;
	list_of_hooks = hook;
	pthread_rwlock_unlock(&rwlock);

	/* returns it */
	return hook;
}

struct afb_hook *afb_hook_addref(struct afb_hook *hook)
{
	pthread_rwlock_wrlock(&rwlock);
	hook->refcount++;
	pthread_rwlock_unlock(&rwlock);
	return hook;
}

void afb_hook_unref(struct afb_hook *hook)
{
	struct afb_hook **prv;

	if (hook) {
		pthread_rwlock_wrlock(&rwlock);
		if (--hook->refcount)
			hook = NULL;
		else {
			/* unlink */
			prv = &list_of_hooks;
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

