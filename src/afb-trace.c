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
#define AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <json-c/json.h>
#include <afb/afb-binding-v2.h>

#include "afb-hook.h"
#include "afb-cred.h"
#include "afb-session.h"
#include "afb-xreq.h"
#include "afb-ditf.h"
#include "afb-svc.h"
#include "afb-evt.h"
#include "afb-trace.h"

#include "wrap-json.h"

/*******************************************************************************/
/*****  default names                                                      *****/
/*******************************************************************************/

#if !defined(DEFAULT_EVENT_NAME)
#  define DEFAULT_EVENT_NAME "trace"
#endif
#if !defined(DEFAULT_TAG_NAME)
#  define DEFAULT_TAG_NAME "trace"
#endif

/*******************************************************************************/
/*****  types                                                              *****/
/*******************************************************************************/

/* structure for searching flags by names */
struct flag
{
	const char *name;	/** the name */
	int value;		/** the value */
};

/* struct for tags */
struct tag {
	struct tag *next;	/* link to the next */
	char tag[1];		/* name of the tag */
};

/* struct for events */
struct event {
	struct event *next;		/* link to the next event */
	struct afb_event event;		/* the event */
};

/* struct for sessions */
struct session {
	struct session *next;		/* link to the next session */
	struct afb_session *session;	/* the session */
	struct afb_trace *trace;	/* the tracer */
};

/* struct for recording hooks */
struct hook {
	struct hook *next;		/* link to next hook */
	void *handler;			/* the handler of the hook */
	struct event *event;		/* the associated event */
	struct tag *tag;		/* the associated tag */
	struct session *session;	/* the associated session */
};

/* types of hooks */
enum trace_type
{
	Trace_Type_Xreq,	/* xreq hooks */
	Trace_Type_Ditf,	/* ditf hooks */
	Trace_Type_Svc,		/* svc hooks */
	Trace_Type_Evt,		/* evt hooks */
	Trace_Type_Count	/* count of types of hooks */
};

/* client data */
struct afb_trace
{
	int refcount;				/* reference count */
	pthread_mutex_t mutex;			/* concurrency management */
	struct afb_daemon *daemon;		/* daemon */
	struct afb_session *bound;		/* bound to session */
	struct event *events;			/* list of events */
	struct tag *tags;			/* list of tags */
	struct session *sessions;		/* list of tags */
	struct hook *hooks[Trace_Type_Count];	/* hooks */
};

/*******************************************************************************/
/*****  utility functions                                                  *****/
/*******************************************************************************/

static void ctxt_error(char **errors, const char *format, ...)
{
	int len;
	char *errs;
	size_t sz;
	char buffer[1024];
	va_list ap;

	va_start(ap, format);
	len = vsnprintf(buffer, sizeof buffer, format, ap);
	va_end(ap);
	if (len > (int)(sizeof buffer - 2))
		len = (int)(sizeof buffer - 2);
	buffer[len++] = '\n';
	buffer[len++] = 0;

	errs = *errors;
	sz = errs ? strlen(errs) : 0;
	errs = realloc(errs, sz + (size_t)len);
	if (errs) {
		memcpy(errs + sz, buffer, len);
		*errors = errs;
	}
}

/* get the value of the flag of 'name' in the array 'flags' of 'count elements */
static int get_flag(const char *name, struct flag flags[], int count)
{
	/* dichotomic search */
	int lower = 0, upper = count;
	while (lower < upper) {
		int mid = (lower + upper) >> 1;
		int cmp = strcmp(name, flags[mid].name);
		if (!cmp)
			return flags[mid].value;
		if (cmp < 0)
			upper = mid;
		else
			lower = mid + 1;
	}
	return 0;
}

/* timestamp */
static struct json_object *timestamp()
{
	char ts[50];
	struct timespec tv;

	clock_gettime(CLOCK_MONOTONIC, &tv);
	snprintf(ts, sizeof ts, "%llu.%06lu", (long long unsigned)tv.tv_sec, (long unsigned)(tv.tv_nsec / 1000));
	return json_object_new_string(ts);
}

/* verbosity level name or NULL */
static const char *verbosity_level_name(int level)
{
	static const char *names[] = {
		"error",
		"warning",
		"notice",
		"info",
		"debug"
	};

	return level >= 3 && level <= 7 ? names[level - 3] : NULL;
}

/* generic hook */
static void emit(void *closure, const char *type, const char *fmt1, const char *fmt2, va_list ap2, ...)
{
	struct hook *hook = closure;
	struct json_object *event, *data1, *data2;
	va_list ap1;

	data1 = data2 = event = NULL;
	va_start(ap1, ap2);
	wrap_json_vpack(&data1, fmt1, ap1);
	va_end(ap1);
	if (fmt2)
		wrap_json_vpack(&data2, fmt2, ap2);

	wrap_json_pack(&event, "{so ss so so*}",
					"time", timestamp(),
					"tag", hook->tag->tag,
					type, data1,
					"data", data2);

	afb_evt_unhooked_push(hook->event->event, event);
}

/*******************************************************************************/
/*****  trace the requests                                                 *****/
/*******************************************************************************/

static struct flag xreq_flags[] = { /* must be sorted by names */
		{ "addref",		afb_hook_flag_req_addref },
		{ "all",		afb_hook_flags_req_all },
		{ "args",		afb_hook_flags_req_args },
		{ "begin",		afb_hook_flag_req_begin },
		{ "common",		afb_hook_flags_req_common },
		{ "context",		afb_hook_flags_req_context },
		{ "context_get",	afb_hook_flag_req_context_get },
		{ "context_set",	afb_hook_flag_req_context_set },
		{ "end",		afb_hook_flag_req_end },
		{ "event",		afb_hook_flags_req_event },
		{ "extra",		afb_hook_flags_req_extra },
		{ "fail",		afb_hook_flag_req_fail },
		{ "get",		afb_hook_flag_req_get },
		{ "json",		afb_hook_flag_req_json },
		{ "life",		afb_hook_flags_req_life },
		{ "ref",		afb_hook_flags_req_ref },
		{ "result",		afb_hook_flags_req_result },
		{ "session",		afb_hook_flags_req_session },
		{ "session_close",	afb_hook_flag_req_session_close },
		{ "session_set_LOA",	afb_hook_flag_req_session_set_LOA },
		{ "store",		afb_hook_flag_req_store },
		{ "stores",		afb_hook_flags_req_stores },
		{ "subcall",		afb_hook_flag_req_subcall },
		{ "subcall_req",	afb_hook_flag_req_subcall_req },
		{ "subcall_req_result",	afb_hook_flag_req_subcall_req_result },
		{ "subcall_result",	afb_hook_flag_req_subcall_result },
		{ "subcalls",		afb_hook_flags_req_subcalls },
		{ "subcallsync",	afb_hook_flag_req_subcallsync },
		{ "subcallsync_result",	afb_hook_flag_req_subcallsync_result },
		{ "subscribe",		afb_hook_flag_req_subscribe },
		{ "success",		afb_hook_flag_req_success },
		{ "unref",		afb_hook_flag_req_unref },
		{ "unstore",		afb_hook_flag_req_unstore },
		{ "unsubscribe",	afb_hook_flag_req_unsubscribe },
		{ "vverbose",		afb_hook_flag_req_vverbose },
};

/* get the xreq value for flag of 'name' */
static int get_xreq_flag(const char *name)
{
	return get_flag(name, xreq_flags, (int)(sizeof xreq_flags / sizeof *xreq_flags));
}

static void hook_xreq(void *closure, const struct afb_xreq *xreq, const char *action, const char *format, ...)
{
	struct json_object *cred = NULL;
	const char *session = NULL;
	va_list ap;

	if (xreq->context.session)
		session = afb_session_uuid(xreq->context.session);

	if (xreq->cred)
		wrap_json_pack(&cred, "{si ss si si ss* ss*}",
						"uid", (int)xreq->cred->uid,
						"user", xreq->cred->user,
						"gid", (int)xreq->cred->gid,
						"pid", (int)xreq->cred->pid,
						"label", xreq->cred->label,
						"id", xreq->cred->id
					);
	va_start(ap, format);
	emit(closure, "request", "{si ss ss ss so* ss*}", format, ap,
					"index", xreq->hookindex,
					"api", xreq->api,
					"verb", xreq->verb,
					"action", action,
					"credentials", cred,
					"session", session);
	va_end(ap);
}

static void hook_xreq_begin(void *closure, const struct afb_xreq *xreq)
{
	hook_xreq(closure, xreq, "begin", NULL);
}

static void hook_xreq_end(void *closure, const struct afb_xreq *xreq)
{
	hook_xreq(closure, xreq, "end", NULL);
}

static void hook_xreq_json(void *closure, const struct afb_xreq *xreq, struct json_object *obj)
{
	hook_xreq(closure, xreq, "json", "{sO?}",
						"result", obj);
}

static void hook_xreq_get(void *closure, const struct afb_xreq *xreq, const char *name, struct afb_arg arg)
{
	hook_xreq(closure, xreq, "get", "{ss? ss? ss? ss?}",
						"query", name,
						"name", arg.name,
						"value", arg.value,
						"path", arg.path);
}

static void hook_xreq_success(void *closure, const struct afb_xreq *xreq, struct json_object *obj, const char *info)
{
	hook_xreq(closure, xreq, "success", "{sO? ss?}",
						"result", obj,
						"info", info);
}

static void hook_xreq_fail(void *closure, const struct afb_xreq *xreq, const char *status, const char *info)
{
	hook_xreq(closure, xreq, "fail", "{ss? ss?}",
						"status", status,
						"info", info);
}

static void hook_xreq_context_get(void *closure, const struct afb_xreq *xreq, void *value)
{
	hook_xreq(closure, xreq, "context_get", NULL);
}

static void hook_xreq_context_set(void *closure, const struct afb_xreq *xreq, void *value, void (*free_value)(void*))
{
	hook_xreq(closure, xreq, "context_set", NULL);
}

static void hook_xreq_addref(void *closure, const struct afb_xreq *xreq)
{
	hook_xreq(closure, xreq, "addref", NULL);
}

static void hook_xreq_unref(void *closure, const struct afb_xreq *xreq)
{
	hook_xreq(closure, xreq, "unref", NULL);
}

static void hook_xreq_session_close(void *closure, const struct afb_xreq *xreq)
{
	hook_xreq(closure, xreq, "session_close", NULL);
}

static void hook_xreq_session_set_LOA(void *closure, const struct afb_xreq *xreq, unsigned level, int result)
{
	hook_xreq(closure, xreq, "session_set_LOA", "{si si}",
					"level", level,
					"result", result);
}

static void hook_xreq_subscribe(void *closure, const struct afb_xreq *xreq, struct afb_event event, int result)
{
	hook_xreq(closure, xreq, "subscribe", "{s{ss si} si}",
					"event",
						"name", afb_evt_event_name(event),
						"id", afb_evt_event_id(event),
					"result", result);
}

static void hook_xreq_unsubscribe(void *closure, const struct afb_xreq *xreq, struct afb_event event, int result)
{
	hook_xreq(closure, xreq, "unsubscribe", "{s{ss? si} si}",
					"event",
						"name", afb_evt_event_name(event),
						"id", afb_evt_event_id(event),
					"result", result);
}

static void hook_xreq_subcall(void *closure, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	hook_xreq(closure, xreq, "subcall", "{ss? ss? sO?}",
					"api", api,
					"verb", verb,
					"args", args);
}

static void hook_xreq_subcall_result(void *closure, const struct afb_xreq *xreq, int status, struct json_object *result)
{
	hook_xreq(closure, xreq, "subcall_result", "{si sO?}",
					"status", status,
					"result", result);
}

static void hook_xreq_subcallsync(void *closure, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	hook_xreq(closure, xreq, "subcallsync", "{ss? ss? sO?}",
					"api", api,
					"verb", verb,
					"args", args);
}

static void hook_xreq_subcallsync_result(void *closure, const struct afb_xreq *xreq, int status, struct json_object *result)
{
	hook_xreq(closure, xreq, "subcallsync_result", "{si sO?}",
					"status", status,
					"result", result);
}

static void hook_xreq_vverbose(void *closure, const struct afb_xreq *xreq, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	struct json_object *pos;
	int len;
	char *msg;
	va_list ap;

	pos = NULL;
	msg = NULL;

	va_copy(ap, args);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (file)
		wrap_json_pack(&pos, "{ss si ss*}", "file", file, "line", line, "function", func);

	hook_xreq(closure, xreq, "vverbose", "{si ss* ss? so*}",
					"level", level,
 					"type", verbosity_level_name(level),
					len < 0 ? "format" : "message", len < 0 ? fmt : msg,
					"position", pos);

	free(msg);
}

static void hook_xreq_store(void *closure, const struct afb_xreq *xreq, struct afb_stored_req *sreq)
{
	hook_xreq(closure, xreq, "store", NULL);
}

static void hook_xreq_unstore(void *closure, const struct afb_xreq *xreq)
{
	hook_xreq(closure, xreq, "unstore", NULL);
}

static void hook_xreq_subcall_req(void *closure, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args)
{
	hook_xreq(closure, xreq, "subcall_req", "{ss? ss? sO?}",
					"api", api,
					"verb", verb,
					"args", args);
}

static void hook_xreq_subcall_req_result(void *closure, const struct afb_xreq *xreq, int status, struct json_object *result)
{
	hook_xreq(closure, xreq, "subcall_req_result", "{si sO?}",
					"status", status,
					"result", result);
}

static struct afb_hook_xreq_itf hook_xreq_itf = {
	.hook_xreq_begin = hook_xreq_begin,
	.hook_xreq_end = hook_xreq_end,
	.hook_xreq_json = hook_xreq_json,
	.hook_xreq_get = hook_xreq_get,
	.hook_xreq_success = hook_xreq_success,
	.hook_xreq_fail = hook_xreq_fail,
	.hook_xreq_context_get = hook_xreq_context_get,
	.hook_xreq_context_set = hook_xreq_context_set,
	.hook_xreq_addref = hook_xreq_addref,
	.hook_xreq_unref = hook_xreq_unref,
	.hook_xreq_session_close = hook_xreq_session_close,
	.hook_xreq_session_set_LOA = hook_xreq_session_set_LOA,
	.hook_xreq_subscribe = hook_xreq_subscribe,
	.hook_xreq_unsubscribe = hook_xreq_unsubscribe,
	.hook_xreq_subcall = hook_xreq_subcall,
	.hook_xreq_subcall_result = hook_xreq_subcall_result,
	.hook_xreq_subcallsync = hook_xreq_subcallsync,
	.hook_xreq_subcallsync_result = hook_xreq_subcallsync_result,
	.hook_xreq_vverbose = hook_xreq_vverbose,
	.hook_xreq_store = hook_xreq_store,
	.hook_xreq_unstore = hook_xreq_unstore,
	.hook_xreq_subcall_req = hook_xreq_subcall_req,
	.hook_xreq_subcall_req_result = hook_xreq_subcall_req_result
};

/*******************************************************************************/
/*****  trace the daemon interface                                         *****/
/*******************************************************************************/

static struct flag ditf_flags[] = { /* must be sorted by names */
		{ "all",			afb_hook_flags_ditf_all },
		{ "common",			afb_hook_flags_ditf_common },
		{ "event_broadcast_after",	afb_hook_flag_ditf_event_broadcast_after },
		{ "event_broadcast_before",	afb_hook_flag_ditf_event_broadcast_before },
		{ "event_make",			afb_hook_flag_ditf_event_make },
		{ "extra",			afb_hook_flags_ditf_extra },
		{ "get_event_loop",		afb_hook_flag_ditf_get_event_loop },
		{ "get_system_bus",		afb_hook_flag_ditf_get_system_bus },
		{ "get_user_bus",		afb_hook_flag_ditf_get_user_bus },
		{ "queue_job",			afb_hook_flag_ditf_queue_job },
		{ "require_api",		afb_hook_flag_ditf_require_api },
		{ "require_api_result",		afb_hook_flag_ditf_require_api_result },
		{ "rootdir_get_fd",		afb_hook_flag_ditf_rootdir_get_fd },
		{ "rootdir_open_locale",	afb_hook_flag_ditf_rootdir_open_locale },
		{ "unstore_req",		afb_hook_flag_ditf_unstore_req },
		{ "vverbose",			afb_hook_flag_ditf_vverbose },
};

/* get the ditf value for flag of 'name' */
static int get_ditf_flag(const char *name)
{
	return get_flag(name, ditf_flags, (int)(sizeof ditf_flags / sizeof *ditf_flags));
}


static void hook_ditf(void *closure, const struct afb_ditf *ditf, const char *action, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	emit(closure, "daemon", "{ss ss}", format, ap,
					"api", ditf->api,
					"action", action);
	va_end(ap);
}

static void hook_ditf_event_broadcast_before(void *closure, const struct afb_ditf *ditf, const char *name, struct json_object *object)
{
	hook_ditf(closure, ditf, "event_broadcast_before", "{ss sO*}",
			"name", name, "data", object);
}

static void hook_ditf_event_broadcast_after(void *closure, const struct afb_ditf *ditf, const char *name, struct json_object *object, int result)
{
	hook_ditf(closure, ditf, "event_broadcast_after", "{ss sO* si}",
			"name", name, "data", object, "result", result);
}

static void hook_ditf_get_event_loop(void *closure, const struct afb_ditf *ditf, struct sd_event *result)
{
	hook_ditf(closure, ditf, "get_event_loop", NULL);
}

static void hook_ditf_get_user_bus(void *closure, const struct afb_ditf *ditf, struct sd_bus *result)
{
	hook_ditf(closure, ditf, "get_user_bus", NULL);
}

static void hook_ditf_get_system_bus(void *closure, const struct afb_ditf *ditf, struct sd_bus *result)
{
	hook_ditf(closure, ditf, "get_system_bus", NULL);
}

static void hook_ditf_vverbose(void*closure, const struct afb_ditf *ditf, int level, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	struct json_object *pos;
	int len;
	char *msg;
	va_list ap;

	pos = NULL;
	msg = NULL;

	va_copy(ap, args);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (file)
		wrap_json_pack(&pos, "{ss si ss*}", "file", file, "line", line, "function", function);

	hook_ditf(closure, ditf, "vverbose", "{si ss* ss? so*}",
					"level", level,
 					"type", verbosity_level_name(level),
					len < 0 ? "format" : "message", len < 0 ? fmt : msg,
					"position", pos);

	free(msg);
}

static void hook_ditf_event_make(void *closure, const struct afb_ditf *ditf, const char *name, struct afb_event result)
{
	hook_ditf(closure, ditf, "event_make", "{ss ss si}",
			"name", name, "event", afb_evt_event_name(result), "id", afb_evt_event_id(result));
}

static void hook_ditf_rootdir_get_fd(void *closure, const struct afb_ditf *ditf, int result)
{
	char path[PATH_MAX];

	if (result >= 0) {
		sprintf(path, "/proc/self/fd/%d", result);
		readlink(path, path, sizeof path);
	}

	hook_ditf(closure, ditf, "rootdir_get_fd", "{ss}",
			result < 0 ? "path" : "error",
			result < 0 ? strerror(errno) : path);
}

static void hook_ditf_rootdir_open_locale(void *closure, const struct afb_ditf *ditf, const char *filename, int flags, const char *locale, int result)
{
	char path[PATH_MAX];

	if (result >= 0) {
		sprintf(path, "/proc/self/fd/%d", result);
		readlink(path, path, sizeof path);
	}

	hook_ditf(closure, ditf, "rootdir_open_locale", "{ss si ss* ss}",
			"file", filename,
			"flags", flags,
			"locale", locale,
			result < 0 ? "path" : "error",
			result < 0 ? strerror(errno) : path);
}

static void hook_ditf_queue_job(void *closure, const struct afb_ditf *ditf, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout, int result)
{
	hook_ditf(closure, ditf, "queue_job", "{ss}", "result", result);
}

static void hook_ditf_unstore_req(void * closure,  const struct afb_ditf *ditf, struct afb_stored_req *sreq)
{
	hook_ditf(closure, ditf, "unstore_req", NULL);
}

static void hook_ditf_require_api(void *closure, const struct afb_ditf *ditf, const char *name, int initialized)
{
	hook_ditf(closure, ditf, "require_api", "{ss sb}", "name", name, "initialized", initialized);
}

static void hook_ditf_require_api_result(void *closure, const struct afb_ditf *ditf, const char *name, int initialized, int result)
{
	hook_ditf(closure, ditf, "require_api_result", "{ss sb si}", "name", name, "initialized", initialized, "result", result);
}

static struct afb_hook_ditf_itf hook_ditf_itf = {
	.hook_ditf_event_broadcast_before = hook_ditf_event_broadcast_before,
	.hook_ditf_event_broadcast_after = hook_ditf_event_broadcast_after,
	.hook_ditf_get_event_loop = hook_ditf_get_event_loop,
	.hook_ditf_get_user_bus = hook_ditf_get_user_bus,
	.hook_ditf_get_system_bus = hook_ditf_get_system_bus,
	.hook_ditf_vverbose = hook_ditf_vverbose,
	.hook_ditf_event_make = hook_ditf_event_make,
	.hook_ditf_rootdir_get_fd = hook_ditf_rootdir_get_fd,
	.hook_ditf_rootdir_open_locale = hook_ditf_rootdir_open_locale,
	.hook_ditf_queue_job = hook_ditf_queue_job,
	.hook_ditf_unstore_req = hook_ditf_unstore_req,
	.hook_ditf_require_api = hook_ditf_require_api,
	.hook_ditf_require_api_result = hook_ditf_require_api_result
};

/*******************************************************************************/
/*****  trace the services                                                 *****/
/*******************************************************************************/

static struct flag svc_flags[] = { /* must be sorted by names */
		{ "all",		afb_hook_flags_svc_all },
		{ "call",		afb_hook_flag_svc_call },
		{ "call_result",	afb_hook_flag_svc_call_result },
		{ "callsync",		afb_hook_flag_svc_callsync },
		{ "callsync_result",	afb_hook_flag_svc_callsync_result },
		{ "on_event_after",	afb_hook_flag_svc_on_event_after },
		{ "on_event_before",	afb_hook_flag_svc_on_event_before },
		{ "start_after",	afb_hook_flag_svc_start_after },
		{ "start_before",	afb_hook_flag_svc_start_before },
};

/* get the svc value for flag of 'name' */
static int get_svc_flag(const char *name)
{
	return get_flag(name, svc_flags, (int)(sizeof svc_flags / sizeof *svc_flags));
}

static void hook_svc(void *closure, const struct afb_svc *svc, const char *action, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	emit(closure, "service", "{ss ss}", format, ap,
					"api", svc->api,
					"action", action);
	va_end(ap);
}

static void hook_svc_start_before(void *closure, const struct afb_svc *svc)
{
	hook_svc(closure, svc, "start_before", NULL);
}

static void hook_svc_start_after(void *closure, const struct afb_svc *svc, int status)
{
	hook_svc(closure, svc, "start_after", "{si}", "result", status);
}

static void hook_svc_on_event_before(void *closure, const struct afb_svc *svc, const char *event, int eventid, struct json_object *object)
{
	hook_svc(closure, svc, "on_event_before", "{ss si sO*}",
			"event", event, "id", eventid, "data", object);
}

static void hook_svc_on_event_after(void *closure, const struct afb_svc *svc, const char *event, int eventid, struct json_object *object)
{
	hook_svc(closure, svc, "on_event_after", "{ss si sO*}",
			"event", event, "id", eventid, "data", object);
}

static void hook_svc_call(void *closure, const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args)
{
	hook_svc(closure, svc, "call", "{ss ss sO*}",
			"api", api, "verb", verb, "args", args);
}

static void hook_svc_call_result(void *closure, const struct afb_svc *svc, int status, struct json_object *result)
{
	hook_svc(closure, svc, "call_result", "{si sO*}",
			"status", status, "result", result);
}

static void hook_svc_callsync(void *closure, const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args)
{
	hook_svc(closure, svc, "callsync", "{ss ss sO*}",
			"api", api, "verb", verb, "args", args);
}

static void hook_svc_callsync_result(void *closure, const struct afb_svc *svc, int status, struct json_object *result)
{
	hook_svc(closure, svc, "callsync_result", "{si sO*}",
			"status", status, "result", result);
}

static struct afb_hook_svc_itf hook_svc_itf = {
	.hook_svc_start_before = hook_svc_start_before,
	.hook_svc_start_after = hook_svc_start_after,
	.hook_svc_on_event_before = hook_svc_on_event_before,
	.hook_svc_on_event_after = hook_svc_on_event_after,
	.hook_svc_call = hook_svc_call,
	.hook_svc_call_result = hook_svc_call_result,
	.hook_svc_callsync = hook_svc_callsync,
	.hook_svc_callsync_result = hook_svc_callsync_result
};

/*******************************************************************************/
/*****  trace the events                                                   *****/
/*******************************************************************************/

static struct flag evt_flags[] = { /* must be sorted by names */
		{ "all",		afb_hook_flags_evt_all },
		{ "broadcast_after",	afb_hook_flag_evt_broadcast_after },
		{ "broadcast_before",	afb_hook_flag_evt_broadcast_before },
		{ "common",		afb_hook_flags_evt_common },
		{ "create",		afb_hook_flag_evt_create },
		{ "drop",		afb_hook_flag_evt_drop },
		{ "extra",		afb_hook_flags_evt_extra },
		{ "name",		afb_hook_flag_evt_name },
		{ "push_after",		afb_hook_flag_evt_push_after },
		{ "push_before",	afb_hook_flag_evt_push_before },
};

/* get the evt value for flag of 'name' */
static int get_evt_flag(const char *name)
{
	return get_flag(name, evt_flags, (int)(sizeof evt_flags / sizeof *evt_flags));
}

static void hook_evt(void *closure, const char *evt, int id, const char *action, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	emit(closure, "event", "{si ss ss}", format, ap,
					"id", id,
					"name", evt,
					"action", action);
	va_end(ap);
}

static void hook_evt_create(void *closure, const char *evt, int id)
{
	hook_evt(closure, evt, id, "create", NULL);
}

static void hook_evt_push_before(void *closure, const char *evt, int id, struct json_object *obj)
{
	hook_evt(closure, evt, id, "push_before", "{sO*}", "data", obj);
}


static void hook_evt_push_after(void *closure, const char *evt, int id, struct json_object *obj, int result)
{
	hook_evt(closure, evt, id, "push_after", "{sO* si}", "data", obj, "result", result);
}

static void hook_evt_broadcast_before(void *closure, const char *evt, int id, struct json_object *obj)
{
	hook_evt(closure, evt, id, "broadcast_before", "{sO*}", "data", obj);
}

static void hook_evt_broadcast_after(void *closure, const char *evt, int id, struct json_object *obj, int result)
{
	hook_evt(closure, evt, id, "broadcast_after", "{sO* si}", "data", obj, "result", result);
}

static void hook_evt_name(void *closure, const char *evt, int id)
{
	hook_evt(closure, evt, id, "name", NULL);
}

static void hook_evt_drop(void *closure, const char *evt, int id)
{
	hook_evt(closure, evt, id, "drop", NULL);
}

static struct afb_hook_evt_itf hook_evt_itf = {
	.hook_evt_create = hook_evt_create,
	.hook_evt_push_before = hook_evt_push_before,
	.hook_evt_push_after = hook_evt_push_after,
	.hook_evt_broadcast_before = hook_evt_broadcast_before,
	.hook_evt_broadcast_after = hook_evt_broadcast_after,
	.hook_evt_name = hook_evt_name,
	.hook_evt_drop = hook_evt_drop
};

/*******************************************************************************/
/*****  abstract types                                                     *****/
/*******************************************************************************/

static
struct
{
	const char *name;
	void (*unref)(void*);
	int (*get_flag)(const char*);
}
abstracting[Trace_Type_Count] =
{
	[Trace_Type_Xreq] =
	{
		.name = "request",
		.unref =  (void(*)(void*))afb_hook_unref_xreq,
		.get_flag = get_xreq_flag
	},
	[Trace_Type_Ditf] =
	{
		.name = "daemon",
		.unref =  (void(*)(void*))afb_hook_unref_ditf,
		.get_flag = get_ditf_flag
	},
	[Trace_Type_Svc] =
	{
		.name = "service",
		.unref =  (void(*)(void*))afb_hook_unref_svc,
		.get_flag = get_svc_flag
	},
	[Trace_Type_Evt] =
	{
		.name = "event",
		.unref =  (void(*)(void*))afb_hook_unref_evt,
		.get_flag = get_evt_flag
	}
};

/*******************************************************************************/
/*****  handle trace data                                                  *****/
/*******************************************************************************/

/* drop hooks of 'trace' matching 'tag' and 'event' and 'session' */
static void trace_unhook(struct afb_trace *trace, struct tag *tag, struct event *event, struct session *session)
{
	int i;
	struct hook *hook, **prev;

	/* remove any event */
	for (i = 0 ; i < Trace_Type_Count ; i++) {
		prev = &trace->hooks[i];
		while ((hook = *prev)) {
			if ((tag && tag != hook->tag)
			 || (event && event != hook->event)
			 || (session && session != hook->session))
				prev = &hook->next;
			else {
				*prev = hook->next;
				abstracting[i].unref(hook->handler);
				free(hook);
			}
		}
	}
}

/* cleanup: removes unused tags, events and sessions of the 'trace' */
static void trace_cleanup(struct afb_trace *trace)
{
	int i;
	struct hook *hook;
	struct tag *tag, **ptag;
	struct event *event, **pevent;
	struct session *session, **psession;

	/* clean sessions */
	psession = &trace->sessions;
	while ((session = *psession)) {
		/* search for session */
		for (hook = NULL, i = 0 ; !hook && i < Trace_Type_Count ; i++) 
			for (hook = trace->hooks[i] ; hook && hook->session != session ; hook = hook->next);
		/* keep or free whether used or not */
		if (hook)
			psession = &session->next;
		else {
			*psession = session->next;
			if (__atomic_exchange_n(&session->trace, NULL, __ATOMIC_RELAXED))
				afb_session_set_cookie(session->session, session, NULL, NULL);
			free(session);
		}
	}
	/* clean tags */
	ptag = &trace->tags;
	while ((tag = *ptag)) {
		/* search for tag */
		for (hook = NULL, i = 0 ; !hook && i < Trace_Type_Count ; i++) 
			for (hook = trace->hooks[i] ; hook && hook->tag != tag ; hook = hook->next);
		/* keep or free whether used or not */
		if (hook)
			ptag = &tag->next;
		else {
			*ptag = tag->next;
			free(tag);
		}
	}
	/* clean events */
	pevent = &trace->events;
	while ((event = *pevent)) {
		/* search for event */
		for (hook = NULL, i = 0 ; !hook && i < Trace_Type_Count ; i++) 
			for (hook = trace->hooks[i] ; hook && hook->event != event ; hook = hook->next);
		/* keep or free whether used or not */
		if (hook)
			pevent = &event->next;
		else {
			*pevent = event->next;
			afb_event_drop(event->event);
			free(event);
		}
	}
}

/* callback at end of traced session */
static void free_session_cookie(void *cookie)
{
	struct session *session = cookie;
	struct afb_trace *trace = __atomic_exchange_n(&session->trace, NULL, __ATOMIC_RELAXED);
	if (trace) {
		pthread_mutex_lock(&trace->mutex);
		trace_unhook(trace, NULL, NULL, session);
		trace_cleanup(trace);
		pthread_mutex_unlock(&trace->mutex);
	}
}

/*
 * Get the tag of 'name' within 'trace'.
 * If 'alloc' isn't zero, create the tag and add it.
 */
static struct tag *trace_get_tag(struct afb_trace *trace, const char *name, int alloc)
{
	struct tag *tag;

	/* search the tag of 'name' */
	tag = trace->tags;
	while (tag && strcmp(name, tag->tag))
		tag = tag->next;

	if (!tag && alloc) {
		/* creation if needed */
		tag = malloc(sizeof * tag + strlen(name));
		if (tag) {
			strcpy(tag->tag, name);
			tag->next = trace->tags;
			trace->tags = tag;
		}
	}
	return tag;
}

/*
 * Get the event of 'name' within 'trace'.
 * If 'alloc' isn't zero, create the event and add it.
 */
static struct event *trace_get_event(struct afb_trace *trace, const char *name, int alloc)
{
	struct event *event;

	/* search the event */
	event = trace->events;
	while (event && strcmp(afb_event_name(event->event), name))
		event = event->next;

	if (!event && alloc) {
		event = malloc(sizeof * event);
		if (event) {
			event->event = trace->daemon->itf->event_make(trace->daemon->closure, name);
			if (afb_event_is_valid(event->event)) {
				event->next = trace->events;
				trace->events = event;
			} else {
				free(event);
				event = NULL;
			}
		}
	}
	return event;
}

/*
 * Get the session of 'value' within 'trace'.
 * If 'alloc' isn't zero, create the session and add it.
 */
static struct session *trace_get_session(struct afb_trace *trace, struct afb_session *value, int alloc)
{
	struct session *session;

	/* search the session */
	session = trace->sessions;
	while (session && session->session != value)
		session = session->next;

	if (!session && alloc) {
		session = malloc(sizeof * session);
		if (session) {
			session->session = value;
			session->trace = NULL;
			session->next = trace->sessions;
			trace->sessions = session;
		}
	}
	return session;
}

/*
 * Get the session of 'uuid' within 'trace'.
 * If 'alloc' isn't zero, create the session and add it.
 */
static struct session *trace_get_session_by_uuid(struct afb_trace *trace, const char *uuid, int alloc)
{
	struct afb_session *session;
	int created;

	session = afb_session_get(uuid, alloc ? &created : NULL);
	return session ? trace_get_session(trace, session, alloc) : NULL;
}

static struct hook *trace_make_detached_hook(struct afb_trace *trace, const char *event, const char *tag)
{
	struct hook *hook;

	tag = tag ?: DEFAULT_TAG_NAME;
	event = event ?: DEFAULT_EVENT_NAME;
	hook = malloc(sizeof *hook);
	if (hook) {
		hook->tag = trace_get_tag(trace, tag, 1);
		hook->event = trace_get_event(trace, event, 1);
		hook->session = NULL;
		hook->handler = NULL;
	}
	return hook;
}

static void trace_attach_hook(struct afb_trace *trace, struct hook *hook, enum trace_type type)
{
	struct session *session = hook->session;
	hook->next = trace->hooks[type];
	trace->hooks[type] = hook;
	if (session && !session->trace) {
		session->trace = trace;
		afb_session_set_cookie(session->session, session, session, free_session_cookie);
	}
}

/*******************************************************************************/
/*****  handle client requests                                             *****/
/*******************************************************************************/

struct context
{
	struct afb_trace *trace;
	struct afb_req req;
	char *errors;
};

struct desc
{
	struct context *context;
	const char *name;
	const char *tag;
	const char *session;
	const char *api;
	const char *verb;
	const char *pattern;
	int flags[Trace_Type_Count];
};


static void addhook(struct desc *desc, enum trace_type type)
{
	struct hook *hook;
	struct session *session;
	struct afb_session *bind;
	struct afb_trace *trace = desc->context->trace;

	/* check permission for bound traces */
	bind = trace->bound;
	if (bind != NULL) {
		if (type != Trace_Type_Xreq) {
			ctxt_error(&desc->context->errors, "tracing %s is forbidden", abstracting[type].name);
			return;
		}
		if (desc->session) {
			ctxt_error(&desc->context->errors, "setting session is forbidden");
			return;
		}
	}

	/* allocate the hook */
	hook = trace_make_detached_hook(trace, desc->name, desc->tag);
	if (!hook) {
		ctxt_error(&desc->context->errors, "allocation of hook failed");
		return;
	}

	/* create the hook handler */
	switch (type) {
	case Trace_Type_Xreq:
		if (desc->session) {
			session = trace_get_session_by_uuid(trace, desc->session, 1);
			if (!session) {
				ctxt_error(&desc->context->errors, "allocation of session failed");
				free(hook);
				return;
			}
			bind = session->session;
		}
		hook->handler = afb_hook_create_xreq(desc->api, desc->verb, bind,
				desc->flags[type], &hook_xreq_itf, hook);
		break;
	case Trace_Type_Ditf:
		hook->handler = afb_hook_create_ditf(desc->api, desc->flags[type], &hook_ditf_itf, hook);
		break;
	case Trace_Type_Svc:
		hook->handler = afb_hook_create_svc(desc->api, desc->flags[type], &hook_svc_itf, hook);
		break;
	case Trace_Type_Evt:
		hook->handler = afb_hook_create_evt(desc->pattern, desc->flags[type], &hook_evt_itf, hook);
		break;
	default:
		break;
	}
	if (!hook->handler) {
		ctxt_error(&desc->context->errors, "creation of hook failed");
		free(hook);
		return;
	}

	/* attach and activate the hook */
	afb_req_subscribe(desc->context->req, hook->event->event);
	trace_attach_hook(trace, hook, type);
}

static void addhooks(struct desc *desc)
{
	int i;

	for (i = 0 ; i < Trace_Type_Count ; i++) {
		if (desc->flags[i])
			addhook(desc, i);
	}
}

static void add_flags(void *closure, struct json_object *object, enum trace_type type)
{
	int value;
	const char *name, *queried;
	struct desc *desc = closure;

	if (wrap_json_unpack(object, "s", &name))
		ctxt_error(&desc->context->errors, "unexpected %s value %s",
					abstracting[type].name,
					json_object_to_json_string(object));
	else {
		queried = (name[0] == '*' && !name[1]) ? "all" : name;
		value = abstracting[type].get_flag(queried);
		if (value)
			desc->flags[type] |= value;
		else
			ctxt_error(&desc->context->errors, "unknown %s name %s",
					abstracting[type].name, name);
	}
}

static void add_xreq_flags(void *closure, struct json_object *object)
{
	add_flags(closure, object, Trace_Type_Xreq);
}

static void add_ditf_flags(void *closure, struct json_object *object)
{
	add_flags(closure, object, Trace_Type_Ditf);
}

static void add_svc_flags(void *closure, struct json_object *object)
{
	add_flags(closure, object, Trace_Type_Svc);
}

static void add_evt_flags(void *closure, struct json_object *object)
{
	add_flags(closure, object, Trace_Type_Evt);
}

/* add hooks */
static void add(void *closure, struct json_object *object)
{
	int rc;
	struct desc desc;
	struct json_object *request, *event, *daemon, *service, *sub;

	memcpy (&desc, closure, sizeof desc);
	request = event = daemon = service = sub = NULL;

	rc = wrap_json_unpack(object, "{s?s s?s s?s s?s s?s s?s s?o s?o s?o s?o s?o}",
			"name", &desc.name,
			"tag", &desc.tag,
			"api", &desc.api,
			"verb", &desc.verb,
			"session", &desc.session,
			"pattern", &desc.pattern,
			"request", &request,
			"daemon", &daemon,
			"service", &service,
			"event", &event,
			"for", &sub);

	if (!rc) {
		/* replace stars */
		if (desc.api && desc.api[0] == '*' && !desc.api[1])
			desc.api = NULL;

		if (desc.verb && desc.verb[0] == '*' && !desc.verb[1])
			desc.verb = NULL;

		if (desc.session && desc.session[0] == '*' && !desc.session[1])
			desc.session = NULL;

		/* get what is expected */
		if (request)
			wrap_json_optarray_for_all(request, add_xreq_flags, &desc);

		if (daemon)
			wrap_json_optarray_for_all(daemon, add_ditf_flags, &desc);

		if (service)
			wrap_json_optarray_for_all(service, add_svc_flags, &desc);

		if (event)
			wrap_json_optarray_for_all(event, add_evt_flags, &desc);

		/* apply */
		if (sub)
			wrap_json_optarray_for_all(sub, add, &desc);
		else
			addhooks(&desc);
	}
	else {
		wrap_json_optarray_for_all(object, add_xreq_flags, &desc);
		addhooks(&desc);
	}
}

/* drop hooks of given tag */
static void drop_tag(void *closure, struct json_object *object)
{
	int rc;
	struct context *context = closure;
	struct tag *tag;
	const char *name;

	rc = wrap_json_unpack(object, "s", &name);
	if (rc)
		ctxt_error(&context->errors, "unexpected tag value %s", json_object_to_json_string(object));
	else {
		tag = trace_get_tag(context->trace, name, 0);
		if (!tag)
			ctxt_error(&context->errors, "tag %s not found", name);
		else
			trace_unhook(context->trace, tag, NULL, NULL);
	}
}

/* drop hooks of given event */
static void drop_event(void *closure, struct json_object *object)
{
	int rc;
	struct context *context = closure;
	struct event *event;
	const char *name;

	rc = wrap_json_unpack(object, "s", &name);
	if (rc)
		ctxt_error(&context->errors, "unexpected event value %s", json_object_to_json_string(object));
	else {
		event = trace_get_event(context->trace, name, 0);
		if (!event)
			ctxt_error(&context->errors, "event %s not found", name);
		else
			trace_unhook(context->trace, NULL, event, NULL);
	}
}

/* drop hooks of given session */
static void drop_session(void *closure, struct json_object *object)
{
	int rc;
	struct context *context = closure;
	struct session *session;
	const char *uuid;

	rc = wrap_json_unpack(object, "s", &uuid);
	if (rc)
		ctxt_error(&context->errors, "unexpected session value %s", json_object_to_json_string(object));
	else {
		session = trace_get_session_by_uuid(context->trace, uuid, 0);
		if (!session)
			ctxt_error(&context->errors, "session %s not found", uuid);
		else
			trace_unhook(context->trace, NULL, NULL, session);
	}
}

/*******************************************************************************/
/*****  public interface                                                   *****/
/*******************************************************************************/

/* allocates an afb_trace instance */
struct afb_trace *afb_trace_create(struct afb_daemon *daemon, struct afb_session *bound)
{
	struct afb_trace *trace;

	assert(daemon);

	trace = calloc(1, sizeof *trace);
	if (trace) {
		trace->refcount = 1;
		trace->bound = bound;
		trace->daemon = daemon;
		pthread_mutex_init(&trace->mutex, NULL);
	}
	return trace;
}

/* add a reference to the trace */
void afb_trace_addref(struct afb_trace *trace)
{
	__atomic_add_fetch(&trace->refcount, 1, __ATOMIC_RELAXED);
}

/* drop one reference to the trace */
void afb_trace_unref(struct afb_trace *trace)
{
	if (trace && !__atomic_sub_fetch(&trace->refcount, 1, __ATOMIC_RELAXED)) {
		/* clean hooks */
		trace_unhook(trace, NULL, NULL, NULL);
		trace_cleanup(trace);
		pthread_mutex_destroy(&trace->mutex);
		free(trace);
	}
}

/* add traces */
int afb_trace_add(struct afb_req req, struct json_object *args, struct afb_trace *trace)
{
	struct context context;
	struct desc desc;

	memset(&context, 0, sizeof context);
	context.trace = trace;
	context.req = req;

	memset(&desc, 0, sizeof desc);
	desc.context = &context;

	pthread_mutex_lock(&trace->mutex);
	wrap_json_optarray_for_all(args, add, &desc);
	pthread_mutex_unlock(&trace->mutex);

	if (!context.errors)
		return 0;

	afb_req_fail(req, "error-detected", context.errors);
	free(context.errors);
	return -1;
}

/* drop traces */
extern int afb_trace_drop(struct afb_req req, struct json_object *args, struct afb_trace *trace)
{
	int rc;
	struct context context;
	struct json_object *tags, *events, *sessions;

	memset(&context, 0, sizeof context);
	context.trace = trace;
	context.req = req;

	/* special: boolean value */
	if (!wrap_json_unpack(args, "b", &rc)) {
		if (rc) {
			pthread_mutex_lock(&trace->mutex);
			trace_unhook(trace, NULL, NULL, NULL);
			trace_cleanup(trace);
			pthread_mutex_unlock(&trace->mutex);
		}
		return 0;
	}

	tags = events = sessions = NULL;
	rc = wrap_json_unpack(args, "{s?o s?o s?o}",
			"event", &events,
			"tag", &tags,
			"session", &sessions);

	if (rc < 0 || !(events || tags || sessions)) {
		afb_req_fail(req, "error-detected", "bad drop arguments");
		return -1;
	}

	pthread_mutex_lock(&trace->mutex);

	if (tags)
		wrap_json_optarray_for_all(tags, drop_tag, &context);

	if (events)
		wrap_json_optarray_for_all(events, drop_event, &context);

	if (sessions)
		wrap_json_optarray_for_all(sessions, drop_session, &context);

	trace_cleanup(trace);

	pthread_mutex_unlock(&trace->mutex);

	if (!context.errors)
		return 0;

	afb_req_fail(req, "error-detected", context.errors);
	free(context.errors);
	return -1;
}
