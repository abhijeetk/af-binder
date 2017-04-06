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

#pragma once

/* individual flags */
#define afb_hook_flag_req_json			1
#define afb_hook_flag_req_get			2
#define afb_hook_flag_req_success		4
#define afb_hook_flag_req_fail			8
#define afb_hook_flag_req_raw			16
#define afb_hook_flag_req_send			32
#define afb_hook_flag_req_context_get		64
#define afb_hook_flag_req_context_set		128
#define afb_hook_flag_req_addref		256
#define afb_hook_flag_req_unref			512
#define afb_hook_flag_req_session_close		1024
#define afb_hook_flag_req_session_set_LOA	2048
#define afb_hook_flag_req_subscribe		4096
#define afb_hook_flag_req_unsubscribe		8192
#define afb_hook_flag_req_subcall		16384
#define afb_hook_flag_req_subcall_result	32768
#define afb_hook_flag_req_begin                 65536
#define afb_hook_flag_req_end                   131072

/* common flags */
#define afb_hook_flags_req_life		(afb_hook_flag_req_begin|afb_hook_flag_req_end)
#define afb_hook_flags_req_args		(afb_hook_flag_req_json|afb_hook_flag_req_get)
#define afb_hook_flags_req_result	(afb_hook_flag_req_success|afb_hook_flag_req_fail)
#define afb_hook_flags_req_session	(afb_hook_flag_req_session_close|afb_hook_flag_req_session_set_LOA)
#define afb_hook_flags_req_event	(afb_hook_flag_req_subscribe|afb_hook_flag_req_unsubscribe)
#define afb_hook_flags_req_subcall	(afb_hook_flag_req_subcall|afb_hook_flag_req_subcall_result)

/* extra flags */
#define afb_hook_flags_req_ref		(afb_hook_flag_req_addref|afb_hook_flag_req_unref)
#define afb_hook_flags_req_context	(afb_hook_flag_req_context_get|afb_hook_flag_req_context_set)

/* internal flags */
#define afb_hook_flags_req_internal	(afb_hook_flag_req_raw|afb_hook_flag_req_send)

/* predefined groups */
#define afb_hook_flags_req_common	(afb_hook_flags_req_life|afb_hook_flags_req_args|afb_hook_flags_req_result\
					|afb_hook_flags_req_session|afb_hook_flags_req_event|afb_hook_flags_req_subcall)
#define afb_hook_flags_req_extra	(afb_hook_flags_req_common|afb_hook_flags_req_ref|afb_hook_flags_req_context)
#define afb_hook_flags_req_all		(afb_hook_flags_req_extra|afb_hook_flags_req_internal)

struct req;
struct afb_context;
struct json_object;
struct afb_arg;
struct afb_event;
struct afb_session;
struct afb_xreq;

struct afb_hook;

struct afb_hook_xreq_itf {
	void (*hook_xreq_begin)(void * closure, const struct afb_xreq *xreq);
	void (*hook_xreq_end)(void * closure, const struct afb_xreq *xreq);
	void (*hook_xreq_json)(void * closure, const struct afb_xreq *xreq, struct json_object *obj);
	void (*hook_xreq_get)(void * closure, const struct afb_xreq *xreq, const char *name, struct afb_arg arg);
	void (*hook_xreq_success)(void * closure, const struct afb_xreq *xreq, struct json_object *obj, const char *info);
	void (*hook_xreq_fail)(void * closure, const struct afb_xreq *xreq, const char *status, const char *info);
	void (*hook_xreq_raw)(void * closure, const struct afb_xreq *xreq, const char *buffer, size_t size);
	void (*hook_xreq_send)(void * closure, const struct afb_xreq *xreq, const char *buffer, size_t size);
	void (*hook_xreq_context_get)(void * closure, const struct afb_xreq *xreq, void *value);
	void (*hook_xreq_context_set)(void * closure, const struct afb_xreq *xreq, void *value, void (*free_value)(void*));
	void (*hook_xreq_addref)(void * closure, const struct afb_xreq *xreq);
	void (*hook_xreq_unref)(void * closure, const struct afb_xreq *xreq);
	void (*hook_xreq_session_close)(void * closure, const struct afb_xreq *xreq);
	void (*hook_xreq_session_set_LOA)(void * closure, const struct afb_xreq *xreq, unsigned level, int result);
	void (*hook_xreq_subscribe)(void * closure, const struct afb_xreq *xreq, struct afb_event event, int result);
	void (*hook_xreq_unsubscribe)(void * closure, const struct afb_xreq *xreq, struct afb_event event, int result);
	void (*hook_xreq_subcall)(void * closure, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args);
	void (*hook_xreq_subcall_result)(void * closure, const struct afb_xreq *xreq, int status, struct json_object *result);
};

extern void afb_hook_init_xreq(struct afb_xreq *xreq);

extern struct afb_hook *afb_hook_xreq_create(const char *api, const char *verb, struct afb_session *session, unsigned flags, struct afb_hook_xreq_itf *itf, void *closure);
extern struct afb_hook *afb_hook_addref(struct afb_hook *spec);
extern void afb_hook_unref(struct afb_hook *spec);

/* hooks for xreq */
extern void afb_hook_xreq_begin(const struct afb_xreq *xreq);
extern void afb_hook_xreq_end(const struct afb_xreq *xreq);
extern struct json_object *afb_hook_xreq_json(const struct afb_xreq *xreq, struct json_object *obj);
extern struct afb_arg afb_hook_xreq_get(const struct afb_xreq *xreq, const char *name, struct afb_arg arg);
extern void afb_hook_xreq_success(const struct afb_xreq *xreq, struct json_object *obj, const char *info);
extern void afb_hook_xreq_fail(const struct afb_xreq *xreq, const char *status, const char *info);
extern const char *afb_hook_xreq_raw(const struct afb_xreq *xreq, const char *buffer, size_t size);
extern void afb_hook_xreq_send(const struct afb_xreq *xreq, const char *buffer, size_t size);
extern void *afb_hook_xreq_context_get(const struct afb_xreq *xreq, void *value);
extern void afb_hook_xreq_context_set(const struct afb_xreq *xreq, void *value, void (*free_value)(void*));
extern void afb_hook_xreq_addref(const struct afb_xreq *xreq);
extern void afb_hook_xreq_unref(const struct afb_xreq *xreq);
extern void afb_hook_xreq_session_close(const struct afb_xreq *xreq);
extern int afb_hook_xreq_session_set_LOA(const struct afb_xreq *xreq, unsigned level, int result);
extern int afb_hook_xreq_subscribe(const struct afb_xreq *xreq, struct afb_event event, int result);
extern int afb_hook_xreq_unsubscribe(const struct afb_xreq *xreq, struct afb_event event, int result);
extern void afb_hook_xreq_subcall(const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args);
extern void afb_hook_xreq_subcall_result(const struct afb_xreq *xreq, int status, struct json_object *result);

