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

#include <stdarg.h>

struct req;
struct afb_context;
struct json_object;
struct afb_arg;
struct afb_event;
struct afb_session;
struct afb_xreq;
struct afb_ditf;
struct afb_svc;
struct afb_stored_req;
struct sd_bus;
struct sd_event;

/*********************************************************
* section hooking xreq
*********************************************************/
struct afb_hook_xreq;

/* individual flags */
#define afb_hook_flag_req_begin			0x000001
#define afb_hook_flag_req_end			0x000002
#define afb_hook_flag_req_json			0x000004
#define afb_hook_flag_req_get			0x000008
#define afb_hook_flag_req_success		0x000010
#define afb_hook_flag_req_fail			0x000020
#define afb_hook_flag_req_context_get		0x000040
#define afb_hook_flag_req_context_set		0x000080
#define afb_hook_flag_req_addref		0x000100
#define afb_hook_flag_req_unref			0x000200
#define afb_hook_flag_req_session_close		0x000400
#define afb_hook_flag_req_session_set_LOA	0x000800
#define afb_hook_flag_req_subscribe		0x001000
#define afb_hook_flag_req_unsubscribe		0x002000
#define afb_hook_flag_req_subcall		0x004000
#define afb_hook_flag_req_subcall_result	0x008000
#define afb_hook_flag_req_subcallsync		0x010000
#define afb_hook_flag_req_subcallsync_result	0x020000
#define afb_hook_flag_req_vverbose		0x040000
#define afb_hook_flag_req_store			0x080000
#define afb_hook_flag_req_unstore		0x100000
#define afb_hook_flag_req_subcall_req		0x200000
#define afb_hook_flag_req_subcall_req_result	0x400000

/* common flags */
#define afb_hook_flags_req_life		(afb_hook_flag_req_begin|afb_hook_flag_req_end)
#define afb_hook_flags_req_args		(afb_hook_flag_req_json|afb_hook_flag_req_get)
#define afb_hook_flags_req_result	(afb_hook_flag_req_success|afb_hook_flag_req_fail)
#define afb_hook_flags_req_session	(afb_hook_flag_req_session_close|afb_hook_flag_req_session_set_LOA)
#define afb_hook_flags_req_event	(afb_hook_flag_req_subscribe|afb_hook_flag_req_unsubscribe)
#define afb_hook_flags_req_subcalls	(afb_hook_flag_req_subcall|afb_hook_flag_req_subcall_result\
					|afb_hook_flag_req_subcall_req|afb_hook_flag_req_subcall_req_result\
					|afb_hook_flag_req_subcallsync|afb_hook_flag_req_subcallsync_result)

/* extra flags */
#define afb_hook_flags_req_ref		(afb_hook_flag_req_addref|afb_hook_flag_req_unref)
#define afb_hook_flags_req_context	(afb_hook_flag_req_context_get|afb_hook_flag_req_context_set)
#define afb_hook_flags_req_stores	(afb_hook_flag_req_store|afb_hook_flag_req_unstore)

/* predefined groups */
#define afb_hook_flags_req_common	(afb_hook_flags_req_life|afb_hook_flags_req_args|afb_hook_flags_req_result\
					|afb_hook_flags_req_session|afb_hook_flags_req_event|afb_hook_flags_req_subcalls\
					|afb_hook_flag_req_vverbose)
#define afb_hook_flags_req_extra	(afb_hook_flags_req_common|afb_hook_flags_req_ref|afb_hook_flags_req_context\
					|afb_hook_flags_req_stores)
#define afb_hook_flags_req_all		(afb_hook_flags_req_extra)

struct afb_hook_xreq_itf {
	void (*hook_xreq_begin)(void * closure, const struct afb_xreq *xreq);
	void (*hook_xreq_end)(void * closure, const struct afb_xreq *xreq);
	void (*hook_xreq_json)(void * closure, const struct afb_xreq *xreq, struct json_object *obj);
	void (*hook_xreq_get)(void * closure, const struct afb_xreq *xreq, const char *name, struct afb_arg arg);
	void (*hook_xreq_success)(void * closure, const struct afb_xreq *xreq, struct json_object *obj, const char *info);
	void (*hook_xreq_fail)(void * closure, const struct afb_xreq *xreq, const char *status, const char *info);
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
	void (*hook_xreq_subcallsync)(void * closure, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args);
	void (*hook_xreq_subcallsync_result)(void * closure, const struct afb_xreq *xreq, int status, struct json_object *result);
	void (*hook_xreq_vverbose)(void * closure, const struct afb_xreq *xreq, int level, const char *file, int line, const char *func, const char *fmt, va_list args);
	void (*hook_xreq_store)(void * closure, const struct afb_xreq *xreq, struct afb_stored_req *sreq);
	void (*hook_xreq_unstore)(void * closure, const struct afb_xreq *xreq);
	void (*hook_xreq_subcall_req)(void * closure, const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args);
	void (*hook_xreq_subcall_req_result)(void * closure, const struct afb_xreq *xreq, int status, struct json_object *result);
};

extern void afb_hook_init_xreq(struct afb_xreq *xreq);

extern struct afb_hook_xreq *afb_hook_create_xreq(const char *api, const char *verb, struct afb_session *session, int flags, struct afb_hook_xreq_itf *itf, void *closure);
extern struct afb_hook_xreq *afb_hook_addref_xreq(struct afb_hook_xreq *spec);
extern void afb_hook_unref_xreq(struct afb_hook_xreq *spec);

/* hooks for xreq */
extern void afb_hook_xreq_begin(const struct afb_xreq *xreq);
extern void afb_hook_xreq_end(const struct afb_xreq *xreq);
extern struct json_object *afb_hook_xreq_json(const struct afb_xreq *xreq, struct json_object *obj);
extern struct afb_arg afb_hook_xreq_get(const struct afb_xreq *xreq, const char *name, struct afb_arg arg);
extern void afb_hook_xreq_success(const struct afb_xreq *xreq, struct json_object *obj, const char *info);
extern void afb_hook_xreq_fail(const struct afb_xreq *xreq, const char *status, const char *info);
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
extern void afb_hook_xreq_subcallsync(const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args);
extern int afb_hook_xreq_subcallsync_result(const struct afb_xreq *xreq, int status, struct json_object *result);
extern void afb_hook_xreq_vverbose(const struct afb_xreq *xreq, int level, const char *file, int line, const char *func, const char *fmt, va_list args);
extern void afb_hook_xreq_store(const struct afb_xreq *xreq, struct afb_stored_req *sreq);
extern void afb_hook_xreq_unstore(const struct afb_xreq *xreq);
extern void afb_hook_xreq_subcall_req(const struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args);
extern void afb_hook_xreq_subcall_req_result(const struct afb_xreq *xreq, int status, struct json_object *result);

/*********************************************************
* section hooking ditf (daemon interface)
*********************************************************/

#define afb_hook_flag_ditf_vverbose			0x000001
#define afb_hook_flag_ditf_event_make			0x000002
#define afb_hook_flag_ditf_event_broadcast_before	0x000004
#define afb_hook_flag_ditf_event_broadcast_after	0x000008
#define afb_hook_flag_ditf_get_event_loop		0x000010
#define afb_hook_flag_ditf_get_user_bus			0x000020
#define afb_hook_flag_ditf_get_system_bus		0x000040
#define afb_hook_flag_ditf_rootdir_get_fd		0x000080
#define afb_hook_flag_ditf_rootdir_open_locale		0x000100
#define afb_hook_flag_ditf_queue_job			0x000200
#define afb_hook_flag_ditf_unstore_req			0x000400
#define afb_hook_flag_ditf_require_api			0x000800
#define afb_hook_flag_ditf_require_api_result		0x001000

#define afb_hook_flags_ditf_common	(afb_hook_flag_ditf_vverbose\
					|afb_hook_flag_ditf_event_make\
					|afb_hook_flag_ditf_event_broadcast_before\
					|afb_hook_flag_ditf_event_broadcast_after)
#define afb_hook_flags_ditf_extra	(afb_hook_flag_ditf_get_event_loop\
					|afb_hook_flag_ditf_get_user_bus\
					|afb_hook_flag_ditf_get_system_bus\
					|afb_hook_flag_ditf_rootdir_get_fd\
					|afb_hook_flag_ditf_rootdir_open_locale\
					|afb_hook_flag_ditf_queue_job\
					|afb_hook_flag_ditf_unstore_req\
					|afb_hook_flag_ditf_require_api\
					|afb_hook_flag_ditf_require_api_result)

#define afb_hook_flags_ditf_all		(afb_hook_flags_ditf_common|afb_hook_flags_ditf_extra)

struct afb_hook_ditf_itf {
	void (*hook_ditf_event_broadcast_before)(void *closure, const struct afb_ditf *ditf, const char *name, struct json_object *object);
	void (*hook_ditf_event_broadcast_after)(void *closure, const struct afb_ditf *ditf, const char *name, struct json_object *object, int result);
	void (*hook_ditf_get_event_loop)(void *closure, const struct afb_ditf *ditf, struct sd_event *result);
	void (*hook_ditf_get_user_bus)(void *closure, const struct afb_ditf *ditf, struct sd_bus *result);
	void (*hook_ditf_get_system_bus)(void *closure, const struct afb_ditf *ditf, struct sd_bus *result);
	void (*hook_ditf_vverbose)(void*closure, const struct afb_ditf *ditf, int level, const char *file, int line, const char *function, const char *fmt, va_list args);
	void (*hook_ditf_event_make)(void *closure, const struct afb_ditf *ditf, const char *name, struct afb_event result);
	void (*hook_ditf_rootdir_get_fd)(void *closure, const struct afb_ditf *ditf, int result);
	void (*hook_ditf_rootdir_open_locale)(void *closure, const struct afb_ditf *ditf, const char *filename, int flags, const char *locale, int result);
	void (*hook_ditf_queue_job)(void *closure, const struct afb_ditf *ditf, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout, int result);
	void (*hook_ditf_unstore_req)(void *closure, const struct afb_ditf *ditf, struct afb_stored_req *sreq);
	void (*hook_ditf_require_api)(void *closure, const struct afb_ditf *ditf, const char *name, int initialized);
	void (*hook_ditf_require_api_result)(void *closure, const struct afb_ditf *ditf, const char *name, int initialized, int result);
};

extern void afb_hook_ditf_event_broadcast_before(const struct afb_ditf *ditf, const char *name, struct json_object *object);
extern int afb_hook_ditf_event_broadcast_after(const struct afb_ditf *ditf, const char *name, struct json_object *object, int result);
extern struct sd_event *afb_hook_ditf_get_event_loop(const struct afb_ditf *ditf, struct sd_event *result);
extern struct sd_bus *afb_hook_ditf_get_user_bus(const struct afb_ditf *ditf, struct sd_bus *result);
extern struct sd_bus *afb_hook_ditf_get_system_bus(const struct afb_ditf *ditf, struct sd_bus *result);
extern void afb_hook_ditf_vverbose(const struct afb_ditf *ditf, int level, const char *file, int line, const char *function, const char *fmt, va_list args);
extern struct afb_event afb_hook_ditf_event_make(const struct afb_ditf *ditf, const char *name, struct afb_event result);
extern int afb_hook_ditf_rootdir_get_fd(const struct afb_ditf *ditf, int result);
extern int afb_hook_ditf_rootdir_open_locale(const struct afb_ditf *ditf, const char *filename, int flags, const char *locale, int result);
extern int afb_hook_ditf_queue_job(const struct afb_ditf *ditf, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout, int result);
extern void afb_hook_ditf_unstore_req(const struct afb_ditf *ditf, struct afb_stored_req *sreq);
extern void afb_hook_ditf_require_api(const struct afb_ditf *ditf, const char *name, int initialized);
extern int afb_hook_ditf_require_api_result(const struct afb_ditf *ditf, const char *name, int initialized, int result);

extern int afb_hook_flags_ditf(const char *api);
extern struct afb_hook_ditf *afb_hook_create_ditf(const char *api, int flags, struct afb_hook_ditf_itf *itf, void *closure);
extern struct afb_hook_ditf *afb_hook_addref_ditf(struct afb_hook_ditf *hook);
extern void afb_hook_unref_ditf(struct afb_hook_ditf *hook);

/*********************************************************
* section hooking svc (service interface)
*********************************************************/

#define afb_hook_flag_svc_start_before			0x000001
#define afb_hook_flag_svc_start_after			0x000002
#define afb_hook_flag_svc_on_event_before		0x000004
#define afb_hook_flag_svc_on_event_after		0x000008
#define afb_hook_flag_svc_call				0x000010
#define afb_hook_flag_svc_call_result			0x000020
#define afb_hook_flag_svc_callsync			0x000040
#define afb_hook_flag_svc_callsync_result		0x000080

#define afb_hook_flags_svc_all		(afb_hook_flag_svc_start_before|afb_hook_flag_svc_start_after\
					|afb_hook_flag_svc_on_event_before|afb_hook_flag_svc_on_event_after\
					|afb_hook_flag_svc_call|afb_hook_flag_svc_call_result\
					|afb_hook_flag_svc_callsync|afb_hook_flag_svc_callsync_result)

struct afb_hook_svc_itf {
	void (*hook_svc_start_before)(void *closure, const struct afb_svc *svc);
	void (*hook_svc_start_after)(void *closure, const struct afb_svc *svc, int status);
	void (*hook_svc_on_event_before)(void *closure, const struct afb_svc *svc, const char *event, int eventid, struct json_object *object);
	void (*hook_svc_on_event_after)(void *closure, const struct afb_svc *svc, const char *event, int eventid, struct json_object *object);
	void (*hook_svc_call)(void *closure, const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args);
	void (*hook_svc_call_result)(void *closure, const struct afb_svc *svc, int status, struct json_object *result);
	void (*hook_svc_callsync)(void *closure, const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args);
	void (*hook_svc_callsync_result)(void *closure, const struct afb_svc *svc, int status, struct json_object *result);
};

extern void afb_hook_svc_start_before(const struct afb_svc *svc);
extern int afb_hook_svc_start_after(const struct afb_svc *svc, int status);
extern void afb_hook_svc_on_event_before(const struct afb_svc *svc, const char *event, int eventid, struct json_object *object);
extern void afb_hook_svc_on_event_after(const struct afb_svc *svc, const char *event, int eventid, struct json_object *object);
extern void afb_hook_svc_call(const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args);
extern void afb_hook_svc_call_result(const struct afb_svc *svc, int status, struct json_object *result);
extern void afb_hook_svc_callsync(const struct afb_svc *svc, const char *api, const char *verb, struct json_object *args);
extern int afb_hook_svc_callsync_result(const struct afb_svc *svc, int status, struct json_object *result);

extern int afb_hook_flags_svc(const char *api);
extern struct afb_hook_svc *afb_hook_create_svc(const char *api, int flags, struct afb_hook_svc_itf *itf, void *closure);
extern struct afb_hook_svc *afb_hook_addref_svc(struct afb_hook_svc *hook);
extern void afb_hook_unref_svc(struct afb_hook_svc *hook);

/*********************************************************
* section hooking evt (event interface)
*********************************************************/

#define afb_hook_flag_evt_create			0x000001
#define afb_hook_flag_evt_push_before			0x000002
#define afb_hook_flag_evt_push_after			0x000004
#define afb_hook_flag_evt_broadcast_before		0x000008
#define afb_hook_flag_evt_broadcast_after		0x000010
#define afb_hook_flag_evt_name				0x000020
#define afb_hook_flag_evt_drop				0x000040

#define afb_hook_flags_evt_common	(afb_hook_flag_evt_push_before|afb_hook_flag_evt_broadcast_before)
#define afb_hook_flags_evt_extra	(afb_hook_flags_evt_common\
					|afb_hook_flag_evt_push_after|afb_hook_flag_evt_broadcast_after\
					|afb_hook_flag_evt_create|afb_hook_flag_evt_drop)
#define afb_hook_flags_evt_all		(afb_hook_flags_evt_extra|afb_hook_flag_evt_name)

struct afb_hook_evt_itf {
	void (*hook_evt_create)(void *closure, const char *evt, int id);
	void (*hook_evt_push_before)(void *closure, const char *evt, int id, struct json_object *obj);
	void (*hook_evt_push_after)(void *closure, const char *evt, int id, struct json_object *obj, int result);
	void (*hook_evt_broadcast_before)(void *closure, const char *evt, int id, struct json_object *obj);
	void (*hook_evt_broadcast_after)(void *closure, const char *evt, int id, struct json_object *obj, int result);
	void (*hook_evt_name)(void *closure, const char *evt, int id);
	void (*hook_evt_drop)(void *closure, const char *evt, int id);
};

extern void afb_hook_evt_create(const char *evt, int id);
extern void afb_hook_evt_push_before(const char *evt, int id, struct json_object *obj);
extern int afb_hook_evt_push_after(const char *evt, int id, struct json_object *obj, int result);
extern void afb_hook_evt_broadcast_before(const char *evt, int id, struct json_object *obj);
extern int afb_hook_evt_broadcast_after(const char *evt, int id, struct json_object *obj, int result);
extern void afb_hook_evt_name(const char *evt, int id);
extern void afb_hook_evt_drop(const char *evt, int id);

extern int afb_hook_flags_evt(const char *name);
extern struct afb_hook_evt *afb_hook_create_evt(const char *name, int flags, struct afb_hook_evt_itf *itf, void *closure);
extern struct afb_hook_evt *afb_hook_addref_evt(struct afb_hook_evt *hook);
extern void afb_hook_unref_evt(struct afb_hook_evt *hook);

