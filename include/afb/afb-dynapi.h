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

#include "afb-dynapi-itf.h"

/*
 * Send a message described by 'fmt' and following parameters
 * to the journal for the verbosity 'level'.
 *
 * 'file', 'line' and 'func' are indicators of position of the code in source files
 * (see macros __FILE__, __LINE__ and __func__).
 *
 *
 * 'level' is defined by syslog standard:
 *      EMERGENCY         0        System is unusable
 *      ALERT             1        Action must be taken immediately
 *      CRITICAL          2        Critical conditions
 *      ERROR             3        Error conditions
 *      WARNING           4        Warning conditions
 *      NOTICE            5        Normal but significant condition
 *      INFO              6        Informational
 *      DEBUG             7        Debug-level messages
 */
static inline void afb_dynapi_verbose(struct afb_dynapi *dynapi, int level, const char *file, int line, const char *func, const char *fmt, ...) __attribute__((format(printf, 6, 7)));
static inline void afb_dynapi_verbose(struct afb_dynapi *dynapi, int level, const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	dynapi->itf->vverbose(dynapi, level, file, line, func, fmt, args);
	va_end(args);
}
static inline void afb_dynapi_vverbose(struct afb_dynapi *dynapi, int level, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	dynapi->itf->vverbose(dynapi, level, file, line, func, fmt, args);
}

/*
 * Retrieves the common systemd's event loop of AFB
 */
static inline struct sd_event *afb_dynapi_get_event_loop(struct afb_dynapi *dynapi)
{
	return dynapi->itf->get_event_loop(dynapi);
}

/*
 * Retrieves the common systemd's user/session d-bus of AFB
 */
static inline struct sd_bus *afb_dynapi_get_user_bus(struct afb_dynapi *dynapi)
{
	return dynapi->itf->get_user_bus(dynapi);
}

/*
 * Retrieves the common systemd's system d-bus of AFB
 */
static inline struct sd_bus *afb_dynapi_get_system_bus(struct afb_dynapi *dynapi)
{
	return dynapi->itf->get_system_bus(dynapi);
}

/*
 * Get the root directory file descriptor. This file descriptor can
 * be used with functions 'openat', 'fstatat', ...
 */
static inline int afb_dynapi_rootdir_get_fd(struct afb_dynapi *dynapi)
{
	return dynapi->itf->rootdir_get_fd(dynapi);
}

/*
 * Opens 'filename' within the root directory with 'flags' (see function openat)
 * using the 'locale' definition (example: "jp,en-US") that can be NULL.
 * Returns the file descriptor or -1 in case of error.
 */
static inline int afb_dynapi_rootdir_open_locale(struct afb_dynapi *dynapi, const char *filename, int flags, const char *locale)
{
	return dynapi->itf->rootdir_open_locale(dynapi, filename, flags, locale);
}

/*
 * Queue the job defined by 'callback' and 'argument' for being executed asynchronously
 * in this thread (later) or in an other thread.
 * If 'group' is not NUL, the jobs queued with a same value (as the pointer value 'group')
 * are executed in sequence in the order of there submission.
 * If 'timeout' is not 0, it represent the maximum execution time for the job in seconds.
 * At first, the job is called with 0 as signum and the given argument.
 * The job is executed with the monitoring of its time and some signals like SIGSEGV and
 * SIGFPE. When a such signal is catched, the job is terminated and reexecuted but with
 * signum being the signal number (SIGALRM when timeout expired).
 *
 * Returns 0 in case of success or -1 in case of error.
 */
static inline int afb_dynapi_queue_job(struct afb_dynapi *dynapi, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout)
{
	return dynapi->itf->queue_job(dynapi, callback, argument, group, timeout);
}

/*
 * Tells that it requires the API of "name" to exist
 * and if 'initialized' is not null to be initialized.
 * Calling this function is only allowed within init.
 * Returns 0 in case of success or -1 in case of error.
 */
static inline int afb_dynapi_require_api(struct afb_dynapi *dynapi, const char *name, int initialized)
{
	return dynapi->itf->require_api(dynapi, name, initialized);
}

/*
 * Set the name of the API to 'name'.
 * Calling this function is only allowed within preinit.
 * Returns 0 in case of success or -1 in case of error.
 */
static inline int afb_dynapi_rename_api(struct afb_dynapi *dynapi, const char *name)
{
	return dynapi->itf->rename_api(dynapi, name);
}

/*
 * Broadcasts widely the event of 'name' with the data 'object'.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Calling this function is only forbidden during preinit.
 *
 * Returns the count of clients that received the event.
 */
static inline int afb_dynapi_broadcast_event(struct afb_dynapi *dynapi, const char *name, struct json_object *object)
{
	return dynapi->itf->event_broadcast(dynapi, name, object);
}

/*
 * Creates an event of 'name' and returns it.
 *
 * Calling this function is only forbidden during preinit.
 *
 * See afb_event_is_valid to check if there is an error.
 */
static inline struct afb_eventid *afb_dynapi_make_eventid(struct afb_dynapi *dynapi, const char *name)
{
	return dynapi->itf->eventid_make(dynapi, name);
}

/**
 * Calls the 'verb' of the 'api' with the arguments 'args' and 'verb' in the name of the binding.
 * The result of the call is delivered to the 'callback' function with the 'callback_closure'.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * The 'callback' receives 3 arguments:
 *  1. 'closure' the user defined closure pointer 'callback_closure',
 *  2. 'status' a status being 0 on success or negative when an error occured,
 *  2. 'result' the resulting data as a JSON object.
 *
 * @param dynapi   The dynapi
 * @param api      The api name of the method to call
 * @param verb     The verb name of the method to call
 * @param args     The arguments to pass to the method
 * @param callback The to call on completion
 * @param callback_closure The closure to pass to the callback
 *
 * @see also 'afb_req_subcall'
 */
static inline void afb_dynapi_call(
	struct afb_dynapi *dynapi,
	const char *api,
	const char *verb,
	struct json_object *args,
	void (*callback)(void*closure, int status, struct json_object *result, struct afb_dynapi *dynapi),
	void *callback_closure)
{
	dynapi->itf->call(dynapi, api, verb, args, callback, callback_closure);
}

/**
 * Calls the 'verb' of the 'api' with the arguments 'args' and 'verb' in the name of the binding.
 * 'result' will receive the response.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * @param dynapi   The dynapi
 * @param api      The api name of the method to call
 * @param verb     The verb name of the method to call
 * @param args     The arguments to pass to the method
 * @param result   Where to store the result - should call json_object_put on it -
 *
 * @returns 0 in case of success or a negative value in case of error.
 *
 * @see also 'afb_req_subcall'
 */
static inline int afb_dynapi_call_sync(
	struct afb_dynapi *dynapi,
	const char *api,
	const char *verb,
	struct json_object *args,
	struct json_object **result)
{
	return dynapi->itf->call_sync(dynapi, api, verb, args, result);
}


static inline int afb_dynapi_new_api(
	struct afb_dynapi *dynapi,
	const char *api,
	const char *info,
	int (*preinit)(void*, struct afb_dynapi *),
	void *closure)
{
	return dynapi->itf->api_new_api(dynapi, api, info, preinit, closure);
}

static inline int afb_dynapi_set_verbs_v2(
	struct afb_dynapi *dynapi,
	const struct afb_verb_v2 *verbs)
{
	return dynapi->itf->api_set_verbs_v2(dynapi, verbs);
}

static inline int afb_dynapi_add_verb(
	struct afb_dynapi *dynapi,
	const char *verb,
	const char *info,
	void (*callback)(struct afb_request *request),
	const struct afb_auth *auth,
	uint32_t session)
{
	return dynapi->itf->api_add_verb(dynapi, verb, info, callback, auth, session);
}


static inline int afb_dynapi_sub_verb(
		struct afb_dynapi *dynapi,
		const char *verb)
{
	return dynapi->itf->api_sub_verb(dynapi, verb);
}


static inline int afb_dynapi_on_event(
		struct afb_dynapi *dynapi,
		void (*onevent)(struct afb_dynapi *dynapi, const char *event, struct json_object *object))
{
	return dynapi->itf->api_set_on_event(dynapi, onevent);
}


static inline int afb_dynapi_on_init(
		struct afb_dynapi *dynapi,
		int (*oninit)(struct afb_dynapi *dynapi))
{
	return dynapi->itf->api_set_on_init(dynapi, oninit);
}


static inline void afb_dynapi_seal(
		struct afb_dynapi *dynapi)
{
	dynapi->itf->api_seal(dynapi);
}


