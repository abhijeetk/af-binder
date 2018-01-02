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

#pragma once

#include <cstdlib>
#include <cstdarg>
#include <functional>

/* ensure version */
#ifndef AFB_BINDING_VERSION
# define AFB_BINDING_VERSION   2
#endif

/* check the version */
#if AFB_BINDING_VERSION < 2
# error "AFB_BINDING_VERSION must be at least 2"
#endif

/* get C definitions of bindings */
extern "C" {
#include "afb-binding.h"
}

namespace afb {
/*************************************************************************/
/* pre-declaration of classes                                            */
/*************************************************************************/

class arg;
class event;
class req;
class stored_req;

/*************************************************************************/
/* declaration of functions                                              */
/*************************************************************************/

struct sd_event *get_event_loop();
struct sd_bus *get_system_bus();
struct sd_bus *get_user_bus();

int broadcast_event(const char *name, json_object *object = nullptr);

event make_event(const char *name);

void verbose(int level, const char *file, int line, const char * func, const char *fmt, va_list args);

void verbose(int level, const char *file, int line, const char * func, const char *fmt, ...);

int rootdir_get_fd();

int rootdir_open_locale_fd(const char *filename, int flags, const char *locale = nullptr);

int queue_job(void (*callback)(int signum, void *arg), void *argument, void *group, int timeout);

int require_api(const char *apiname, bool initialized = true);

int rename_api(const char *apiname);

int verbosity();

bool wants_errors();
bool wants_warnings();
bool wants_notices();
bool wants_infos();
bool wants_debugs();

void call(const char *api, const char *verb, struct json_object *args, void (*callback)(void*closure, int iserror, struct json_object *result), void *closure);

template <class T> void call(const char *api, const char *verb, struct json_object *args, void (*callback)(T*closure, int iserror, struct json_object *result), T *closure);

bool callsync(const char *api, const char *verb, struct json_object *args, struct json_object *&result);

/*************************************************************************/
/* effective declaration of classes                                      */
/*************************************************************************/

/* events */
class event
{
	struct afb_event event_;
public:
	event() { event_.itf = nullptr; event_.closure = nullptr; }
	event(const struct afb_event &e);
	event(const event &other);
	event &operator=(const event &other);

	operator const struct afb_event&() const;

	operator bool() const;
	bool is_valid() const;

	void invalidate();

	int broadcast(json_object *object) const;
	int push(json_object *object) const;

	void unref();
	void addref();
	const char *name() const;
};

/* args */
class arg
{
	struct afb_arg arg_;
public:
	arg() = delete;
	arg(const struct afb_arg &a);
	arg(const arg &other);
	arg &operator=(const arg &other);

	operator const struct afb_arg&() const;

	bool has_name() const;
	bool has_value() const;
	bool has_path() const;

	const char *name() const;
	const char *value() const;
	const char *path() const;
};

/* req(uest) */
class req
{
	struct afb_req req_;
public:
	class stored
	{
		struct afb_stored_req *sreq_;

		friend class req;
		stored() = delete;
		stored(struct afb_stored_req *sr);
	public:
		stored(const stored &other);
		stored &operator =(const stored &other);
		req unstore() const;
	};

	class stored;

public:
	req() = delete;
	req(const struct afb_req &r);
	req(const req &other);
	req &operator=(const req &other);

	operator const struct afb_req&() const;

	operator bool() const;
	bool is_valid() const;

	arg get(const char *name) const;

	const char *value(const char *name) const;

	const char *path(const char *name) const;

	json_object *json() const;

	void success(json_object *obj = nullptr, const char *info = nullptr) const;
	void successf(json_object *obj, const char *info, ...) const;

	void fail(const char *status = "failed", const char *info = nullptr) const;
	void failf(const char *status, const char *info, ...) const;

	void *context_get() const;

	void context_set(void *context, void (*free_context)(void*)) const;

	void *context(void *(*create_context)(), void (*free_context)(void*)) const;

	template < class T > T *context() const;

	void context_clear() const;

	void addref() const;

	void unref() const;

	void session_close() const;

	bool session_set_LOA(unsigned level) const;

	stored store() const;

	bool subscribe(const event &event) const;

	bool unsubscribe(const event &event) const;

	void subcall(const char *api, const char *verb, json_object *args, void (*callback)(void *closure, int iserror, json_object *result), void *closure) const;

	void subcall(const char *api, const char *verb, json_object *args, void (*callback)(void *closure, int iserror, json_object *result, afb_req req), void *closure) const;

	template <class T> void subcall(const char *api, const char *verb, json_object *args, void (*callback)(T *closure, int iserror, json_object *result), T *closure) const;

	bool subcallsync(const char *api, const char *verb, json_object *args, struct json_object *&result) const;

	void verbose(int level, const char *file, int line, const char * func, const char *fmt, va_list args) const;

	void verbose(int level, const char *file, int line, const char * func, const char *fmt, ...) const;

	bool has_permission(const char *permission) const;

	char *get_application_id() const;

	int get_uid() const;
};

/*************************************************************************/
/* effective declaration of classes                                      */
/*************************************************************************/
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/* effective declaration of classes                                      */
/*************************************************************************/

/* events */
inline event::event(const struct afb_event &e) : event_(e) { }
inline event::event(const event &other) : event_(other.event_) { }
inline event &event::operator=(const event &other) { event_ = other.event_; return *this; }

inline event::operator const struct afb_event&() const { return event_; }

inline event::operator bool() const { return is_valid(); }
inline bool event::is_valid() const { return afb_event_is_valid(event_); } 

inline void event::invalidate() { event_.itf = NULL; event_.closure = NULL; }

inline int event::broadcast(json_object *object) const { return afb_event_broadcast(event_, object); } 
inline int event::push(json_object *object) const { return afb_event_push(event_, object); }

inline void event::unref() { afb_event_unref(event_); invalidate(); }
inline void event::addref() { afb_event_addref(event_); }
inline const char *event::name() const { return afb_event_name(event_); }

/* args */
inline arg::arg(const struct afb_arg &a) : arg_(a) {}
inline arg::arg(const arg &other) : arg_(other.arg_) {}
inline arg &arg::operator=(const arg &other) { arg_ = other.arg_; return *this; }

inline arg::operator const struct afb_arg&() const { return arg_; }

inline bool arg::has_name() const { return !!arg_.name; }
inline bool arg::has_value() const { return !!arg_.value; }
inline bool arg::has_path() const { return !!arg_.path; }

inline const char *arg::name() const { return arg_.name; }
inline const char *arg::value() const { return arg_.value; }
inline const char *arg::path() const { return arg_.path; }

/* req(uests)s */

inline req::stored::stored(struct afb_stored_req *sr) : sreq_(sr) {}

inline req::stored::stored(const req::stored &other) : sreq_(other.sreq_) {}

inline req::stored &req::stored::operator =(const req::stored &other) { sreq_ = other.sreq_; return *this; }

inline req req::stored::unstore() const { return req(afb_daemon_unstore_req_v2(sreq_)); }


inline req::req(const struct afb_req &r) : req_(r) {}
inline req::req(const req &other) : req_(other.req_) {}
inline req &req::operator=(const req &other) { req_ = other.req_; return *this; }

inline req::operator const struct afb_req&() const { return req_; }

inline req::operator bool() const { return !!afb_req_is_valid(req_); }
inline bool req::is_valid() const { return !!afb_req_is_valid(req_); }

inline arg req::get(const char *name) const { return arg(afb_req_get(req_, name)); }

inline const char *req::value(const char *name) const { return afb_req_value(req_, name); }

inline const char *req::path(const char *name) const { return afb_req_path(req_, name); }

inline json_object *req::json() const { return afb_req_json(req_); }

inline void req::success(json_object *obj, const char *info) const { afb_req_success(req_, obj, info); }
inline void req::successf(json_object *obj, const char *info, ...) const
{
	va_list args;
	va_start(args, info);
	afb_req_success_v(req_, obj, info, args);
	va_end(args);
}

inline void req::fail(const char *status, const char *info) const { afb_req_fail(req_, status, info); }
inline void req::failf(const char *status, const char *info, ...) const
{
	va_list args;
	va_start(args, info);
	afb_req_fail_v(req_, status, info, args);
	va_end(args);
}

inline void *req::context_get() const { return afb_req_context_get(req_); }

inline void req::context_set(void *context, void (*free_context)(void*)) const { afb_req_context_set(req_, context, free_context); }

inline void *req::context(void *(*create_context)(), void (*free_context)(void*)) const { return afb_req_context(req_, create_context, free_context); }

template < class T >
inline T *req::context() const
{
	T* (*creater)() = [](){return new T();};
	void (*freer)(T*) = [](T*t){delete t;};
	return reinterpret_cast<T*>(afb_req_context(req_,
			reinterpret_cast<void *(*)()>(creater),
			reinterpret_cast<void (*)(void*)>(freer)));
}

inline void req::context_clear() const { afb_req_context_clear(req_); }

inline void req::addref() const { afb_req_addref(req_); }

inline void req::unref() const { afb_req_unref(req_); }

inline void req::session_close() const { afb_req_session_close(req_); }

inline bool req::session_set_LOA(unsigned level) const { return !!afb_req_session_set_LOA(req_, level); }

inline req::stored req::store() const { return stored(afb_req_store_v2(req_)); }

inline bool req::subscribe(const event &event) const { return !afb_req_subscribe(req_, event); }

inline bool req::unsubscribe(const event &event) const { return !afb_req_unsubscribe(req_, event); }

inline void req::subcall(const char *api, const char *verb, json_object *args, void (*callback)(void *closure, int iserror, json_object *result), void *closure) const
{
	afb_req_subcall(req_, api, verb, args, callback, closure);
}

inline void req::subcall(const char *api, const char *verb, json_object *args, void (*callback)(void *closure, int iserror, json_object *result, struct afb_req req), void *closure) const
{
	afb_req_subcall_req(req_, api, verb, args, callback, closure);
}

template <class T>
inline void req::subcall(const char *api, const char *verb, json_object *args, void (*callback)(T *closure, int iserror, json_object *result), T *closure) const
{
	afb_req_subcall(req_, api, verb, args, reinterpret_cast<void(*)(void*,int,json_object*)>(callback), reinterpret_cast<void*>(closure));
}

inline bool req::subcallsync(const char *api, const char *verb, json_object *args, struct json_object *&result) const
{
	return !!afb_req_subcall_sync(req_, api, verb, args, &result);
}

inline void req::verbose(int level, const char *file, int line, const char * func, const char *fmt, va_list args) const
{
	req_.itf->vverbose(req_.closure, level, file, line, func, fmt, args);
}

inline void req::verbose(int level, const char *file, int line, const char * func, const char *fmt, ...) const
{
	va_list args;
	va_start(args, fmt);
	req_.itf->vverbose(req_.closure, level, file, line, func, fmt, args);
	va_end(args);
}

inline bool req::has_permission(const char *permission) const
{
	return bool(req_.itf->has_permission(req_.closure, permission));
}

inline char *req::get_application_id() const
{
	return req_.itf->get_application_id(req_.closure);
}

inline int req::get_uid() const
{
	return req_.itf->get_uid(req_.closure);
}

/* commons */
inline struct sd_event *get_event_loop()
	{ return afb_daemon_get_event_loop_v2(); }

inline struct sd_bus *get_system_bus()
	{ return afb_daemon_get_system_bus_v2(); }

inline struct sd_bus *get_user_bus()
	{ return afb_daemon_get_user_bus_v2(); }

inline int broadcast_event(const char *name, json_object *object)
	{ return afb_daemon_broadcast_event_v2(name, object); }

inline event make_event(const char *name)
	{ return afb_daemon_make_event_v2(name); }

inline void verbose(int level, const char *file, int line, const char * func, const char *fmt, va_list args)
	{ afb_daemon_verbose_v2(level, file, line, func, fmt, args); }

inline void verbose(int level, const char *file, int line, const char * func, const char *fmt, ...)
	{ va_list args; va_start(args, fmt); verbose(level, file, line, func, fmt, args); va_end(args); }

inline int rootdir_get_fd()
	{ return afb_daemon_rootdir_get_fd_v2(); }

inline int rootdir_open_locale_fd(const char *filename, int flags, const char *locale)
	{ return afb_daemon_rootdir_open_locale_v2(filename, flags, locale); }

inline int queue_job(void (*callback)(int signum, void *arg), void *argument, void *group, int timeout)
	{ return afb_daemon_queue_job_v2(callback, argument, group, timeout); }

inline int require_api(const char *apiname, bool initialized)
	{ return afb_daemon_require_api_v2(apiname, int(initialized)); }

inline int rename_api(const char *apiname)
	{ return afb_daemon_rename_api_v2(apiname); }

inline int verbosity()
	{ return afb_get_verbosity(); }

inline bool wants_errors()
	{ return afb_verbose_error(); }

inline bool wants_warnings()
	{ return afb_verbose_warning(); }

inline bool wants_notices()
	{ return afb_verbose_notice(); }

inline bool wants_infos()
	{ return afb_verbose_info(); }

inline bool wants_debugs()
	{ return afb_verbose_debug(); }

inline void call(const char *api, const char *verb, struct json_object *args, void (*callback)(void*closure, int iserror, struct json_object *result), void *closure)
{
	afb_service_call(api, verb, args, callback, closure);
}

template <class T>
inline void call(const char *api, const char *verb, struct json_object *args, void (*callback)(T*closure, int iserror, struct json_object *result), T *closure)
{
	afb_service_call(api, verb, args, reinterpret_cast<void(*)(void*,int,json_object*)>(callback), reinterpret_cast<void*>(closure));
}

inline bool callsync(const char *api, const char *verb, struct json_object *args, struct json_object *&result)
{
	return !!afb_service_call_sync(api, verb, args, &result);
}

/*************************************************************************/
/* declaration of the binding's authorization s                          */
/*************************************************************************/

constexpr afb_auth auth_no()
{
	afb_auth r = { afb_auth_No, 0, 0};
	r.type = afb_auth_No;
	return r;
}

constexpr afb_auth auth_yes()
{
	afb_auth r = { afb_auth_No, 0, 0};
	r.type = afb_auth_Yes;
	return r;
}

constexpr afb_auth auth_token()
{
	afb_auth r = { afb_auth_No, 0, 0};
	r.type = afb_auth_Token;
	return r;
}

constexpr afb_auth auth_LOA(unsigned loa)
{
	afb_auth r = { afb_auth_No, 0, 0};
	r.type = afb_auth_LOA;
	r.loa = loa;
	return r;
}

constexpr afb_auth auth_permission(const char *permission)
{
	afb_auth r = { afb_auth_No, 0, 0};
	r.type = afb_auth_Permission;
	r.text = permission;
	return r;
}

constexpr afb_auth auth_not(const afb_auth *other)
{
	afb_auth r = { afb_auth_No, 0, 0};
	r.type = afb_auth_Not;
	r.first = other;
	return r;
}

constexpr afb_auth auth_not(const afb_auth &other)
{
	return auth_not(&other);
}

constexpr afb_auth auth_or(const afb_auth *first, const afb_auth *next)
{
	afb_auth r = { afb_auth_No, 0, 0};
	r.type = afb_auth_Or;
	r.first = first;
	r.next = next;
	return r;
}

constexpr afb_auth auth_or(const afb_auth &first, const afb_auth &next)
{
	return auth_or(&first, &next);
}

constexpr afb_auth auth_and(const afb_auth *first, const afb_auth *next)
{
	afb_auth r = { afb_auth_No, 0, 0};
	r.type = afb_auth_And;
	r.first = first;
	r.next = next;
	return r;
}

constexpr afb_auth auth_and(const afb_auth &first, const afb_auth &next)
{
	return auth_and(&first, &next);
}

constexpr afb_verb_v2 verb(const char *name, void (*callback)(afb_req), const char *info = nullptr, unsigned session = 0, const afb_auth *auth = nullptr)
{
	afb_verb_v2 r = { 0, 0, 0, 0, 0 };
	r.verb = name;
	r.callback = callback;
	r.info = info;
	r.session = session;
	r.auth = auth;
	return r;
}

constexpr afb_verb_v2 verbend()
{
	afb_verb_v2 r = { 0, 0, 0, 0, 0 };
	r.verb = nullptr;
	r.callback = nullptr;
	r.info = nullptr;
	r.session = 0;
	r.auth = nullptr;
	return r;
}

constexpr afb_binding_v2 binding(const char *name, const struct afb_verb_v2 *verbs, const char *info = nullptr, int (*init)() = nullptr, const char *specification = nullptr, void (*onevent)(const char*, struct json_object*) = nullptr, bool noconcurrency = false, int (*preinit)() = nullptr)
{
	afb_binding_v2 r = { 0, 0, 0, 0, 0, 0, 0, 0 };
	r.api = name;
	r.specification = specification;
	r.info = info;
	r.verbs = verbs;
	r.preinit = preinit;
	r.init = init;
	r.onevent = onevent;
	r.noconcurrency = noconcurrency ? 1 : 0;
	return r;
};

/*************************************************************************/
/***                         E N D                                     ***/
/*************************************************************************/
}
