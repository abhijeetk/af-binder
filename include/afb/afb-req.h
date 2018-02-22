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

#include "afb-req-itf.h"
#include "afb-event.h"

/*
 * Converts the 'req' to an afb_request.
 */
static inline struct afb_request *afb_req_to_request(struct afb_req req)
{
	return req.closure;
}

/*
 * Checks whether the request 'req' is valid or not.
 *
 * Returns 0 if not valid or 1 if valid.
 */
static inline int afb_req_is_valid(struct afb_req req)
{
	return !!req.itf;
}

/*
 * Gets from the request 'req' the argument of 'name'.
 * Returns a PLAIN structure of type 'struct afb_arg'.
 * When the argument of 'name' is not found, all fields of result are set to NULL.
 * When the argument of 'name' is found, the fields are filled,
 * in particular, the field 'result.name' is set to 'name'.
 *
 * There is a special name value: the empty string.
 * The argument of name "" is defined only if the request was made using
 * an HTTP POST of Content-Type "application/json". In that case, the
 * argument of name "" receives the value of the body of the HTTP request.
 */
static inline struct afb_arg afb_req_get(struct afb_req req, const char *name)
{
	return req.itf->get(req.closure, name);
}

/*
 * Gets from the request 'req' the string value of the argument of 'name'.
 * Returns NULL if when there is no argument of 'name'.
 * Returns the value of the argument of 'name' otherwise.
 *
 * Shortcut for: afb_req_get(req, name).value
 */
static inline const char *afb_req_value(struct afb_req req, const char *name)
{
	return afb_req_get(req, name).value;
}

/*
 * Gets from the request 'req' the path for file attached to the argument of 'name'.
 * Returns NULL if when there is no argument of 'name' or when there is no file.
 * Returns the path of the argument of 'name' otherwise.
 *
 * Shortcut for: afb_req_get(req, name).path
 */
static inline const char *afb_req_path(struct afb_req req, const char *name)
{
	return afb_req_get(req, name).path;
}

/*
 * Gets from the request 'req' the json object hashing the arguments.
 * The returned object must not be released using 'json_object_put'.
 */
static inline struct json_object *afb_req_json(struct afb_req req)
{
	return req.itf->json(req.closure);
}

/*
 * Sends a reply of kind success to the request 'req'.
 * The status of the reply is automatically set to "success".
 * Its send the object 'obj' (can be NULL) with an
 * informationnal comment 'info (can also be NULL).
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
static inline void afb_req_success(struct afb_req req, struct json_object *obj, const char *info)
{
	req.itf->success(req.closure, obj, info);
}

/*
 * Same as 'afb_req_success' but the 'info' is a formatting
 * string followed by arguments.
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
static inline void afb_req_success_f(struct afb_req req, struct json_object *obj, const char *info, ...) __attribute__((format(printf, 3, 4)));
static inline void afb_req_success_f(struct afb_req req, struct json_object *obj, const char *info, ...)
{
	va_list args;
	va_start(args, info);
	req.itf->vsuccess(req.closure, obj, info, args);
	va_end(args);
}

/*
 * Same as 'afb_req_success_f' but the arguments to the format 'info'
 * are given as a variable argument list instance.
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
static inline void afb_req_success_v(struct afb_req req, struct json_object *obj, const char *info, va_list args)
{
	req.itf->vsuccess(req.closure, obj, info, args);
}

/*
 * Sends a reply of kind failure to the request 'req'.
 * The status of the reply is set to 'status' and an
 * informationnal comment 'info' (can also be NULL) can be added.
 *
 * Note that calling afb_req_fail("success", info) is equivalent
 * to call afb_req_success(NULL, info). Thus even if possible it
 * is strongly recommanded to NEVER use "success" for status.
 */
static inline void afb_req_fail(struct afb_req req, const char *status, const char *info)
{
	req.itf->fail(req.closure, status, info);
}

/*
 * Same as 'afb_req_fail' but the 'info' is a formatting
 * string followed by arguments.
 */
static inline void afb_req_fail_f(struct afb_req req, const char *status, const char *info, ...) __attribute__((format(printf, 3, 4)));
static inline void afb_req_fail_f(struct afb_req req, const char *status, const char *info, ...)
{
	va_list args;
	va_start(args, info);
	req.itf->vfail(req.closure, status, info, args);
	va_end(args);
}

/*
 * Same as 'afb_req_fail_f' but the arguments to the format 'info'
 * are given as a variable argument list instance.
 */
static inline void afb_req_fail_v(struct afb_req req, const char *status, const char *info, va_list args)
{
	req.itf->vfail(req.closure, status, info, args);
}

/*
 * Gets the pointer stored by the binding for the session of 'req'.
 * When the binding has not yet recorded a pointer, NULL is returned.
 */
static inline void *afb_req_context_get(struct afb_req req)
{
	return req.itf->context_get(req.closure);
}

/*
 * Stores for the binding the pointer 'context' to the session of 'req'.
 * The function 'free_context' will be called when the session is closed
 * or if binding stores an other pointer.
 */
static inline void afb_req_context_set(struct afb_req req, void *context, void (*free_context)(void*))
{
	req.itf->context_set(req.closure, context, free_context);
}

/*
 * Gets the pointer stored by the binding for the session of 'req'.
 * If the stored pointer is NULL, indicating that no pointer was
 * already stored, afb_req_context creates a new context by calling
 * the function 'create_context' and stores it with the freeing function
 * 'free_context'.
 */
static inline void *afb_req_context(struct afb_req req, void *(*create_context)(), void (*free_context)(void*))
{
	return req.itf->context_make(req.closure, 0, (void *(*)(void*))(void*)create_context, free_context, 0);
}

/*
 * Gets the pointer stored by the binding for the session of 'request'.
 * If no previous pointer is stored or if 'replace' is not zero, a new value
 * is generated using the function 'create_context' called with the 'closure'.
 * If 'create_context' is NULL the generated value is 'closure'.
 * When a value is created, the function 'free_context' is recorded and will
 * be called (with the created value as argument) to free the created value when
 * it is not more used.
 * This function is atomic: it ensures that 2 threads will not race together.
 */
static inline void *afb_req_context_make(struct afb_req req, int replace, void *(*create_context)(void *closure), void (*free_context)(void*), void *closure)
{
	return req.itf->context_make(req.closure, replace, create_context, free_context, closure);
}

/*
 * Frees the pointer stored by the binding for the session of 'req'
 * and sets it to NULL.
 *
 * Shortcut for: afb_req_context_set(req, NULL, NULL)
 */
static inline void afb_req_context_clear(struct afb_req req)
{
	afb_req_context_set(req, 0, 0);
}

/*
 * Adds one to the count of references of 'req'.
 * This function MUST be called by asynchronous implementations
 * of verbs if no reply was sent before returning.
 */
static inline void afb_req_addref(struct afb_req req)
{
	req.itf->addref(req.closure);
}

/*
 * Substracts one to the count of references of 'req'.
 * This function MUST be called by asynchronous implementations
 * of verbs after sending the asynchronous reply.
 */
static inline void afb_req_unref(struct afb_req req)
{
	req.itf->unref(req.closure);
}

/*
 * Closes the session associated with 'req'
 * and delete all associated contexts.
 */
static inline void afb_req_session_close(struct afb_req req)
{
	req.itf->session_close(req.closure);
}

/*
 * Sets the level of assurance of the session of 'req'
 * to 'level'. The effect of this function is subject of
 * security policies.
 * Returns 1 on success or 0 if failed.
 */
static inline int afb_req_session_set_LOA(struct afb_req req, unsigned level)
{
	return req.itf->session_set_LOA(req.closure, level);
}

/*
 * Establishes for the client link identified by 'req' a subscription
 * to the 'event'.
 * Returns 0 in case of successful subscription or -1 in case of error.
 */
static inline int afb_req_subscribe(struct afb_req req, struct afb_event event)
{
	return req.itf->subscribe(req.closure, event);
}

/*
 * Revokes the subscription established to the 'event' for the client
 * link identified by 'req'.
 * Returns 0 in case of successful subscription or -1 in case of error.
 */
static inline int afb_req_unsubscribe(struct afb_req req, struct afb_event event)
{
	return req.itf->unsubscribe(req.closure, event);
}

/*
 * Makes a call to the method of name 'api' / 'verb' with the object 'args'.
 * This call is made in the context of the request 'req'.
 * On completion, the function 'callback' is invoked with the
 * 'closure' given at call and two other parameters: 'iserror' and 'result'.
 * 'status' is 0 on success or negative when on an error reply.
 * 'result' is the json object of the reply, you must not call json_object_put
 * on the result.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * See also:
 *  - 'afb_req_subcall_req' that is convenient to keep request alive automatically.
 *  - 'afb_req_subcall_sync' the synchronous version
 */
static inline void afb_req_subcall(struct afb_req req, const char *api, const char *verb, struct json_object *args, void (*callback)(void *closure, int iserror, struct json_object *result), void *closure)
{
	req.itf->subcall(req.closure, api, verb, args, callback, closure);
}

/*
 * Makes a call to the method of name 'api' / 'verb' with the object 'args'.
 * This call is made in the context of the request 'req'.
 * On completion, the function 'callback' is invoked with the
 * original request 'req', the 'closure' given at call and two
 * other parameters: 'iserror' and 'result'.
 * 'status' is 0 on success or negative when on an error reply.
 * 'result' is the json object of the reply, you must not call json_object_put
 * on the result.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * See also:
 *  - 'afb_req_subcall' that doesn't keep request alive automatically.
 *  - 'afb_req_subcall_sync' the synchronous version
 */
static inline void afb_req_subcall_req(struct afb_req req, const char *api, const char *verb, struct json_object *args, void (*callback)(void *closure, int iserror, struct json_object *result, struct afb_req req), void *closure)
{
	req.itf->subcall_req(req.closure, api, verb, args, callback, closure);
}

/*
 * Makes a call to the method of name 'api' / 'verb' with the object 'args'.
 * This call is made in the context of the request 'req'.
 * This call is synchronous, it waits untill completion of the request.
 * It returns 0 on success or a negative value on error answer.
 * The object pointed by 'result' is filled and must be released by the caller
 * after its use by calling 'json_object_put'.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * See also:
 *  - 'afb_req_subcall_req' that is convenient to keep request alive automatically.
 *  - 'afb_req_subcall' that doesn't keep request alive automatically.
 */
static inline int afb_req_subcall_sync(struct afb_req req, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	return req.itf->subcallsync(req.closure, api, verb, args, result);
}

/*
 * Send associated to 'req' a message described by 'fmt' and following parameters
 * to the journal for the verbosity 'level'.
 *
 * 'file', 'line' and 'func' are indicators of position of the code in source files
 * (see macros __FILE__, __LINE__ and __func__).
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
static inline void afb_req_verbose(struct afb_req req, int level, const char *file, int line, const char * func, const char *fmt, ...) __attribute__((format(printf, 6, 7)));
static inline void afb_req_verbose(struct afb_req req, int level, const char *file, int line, const char * func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	req.itf->vverbose(req.closure, level, file, line, func, fmt, args);
	va_end(args);
}

/* macro for setting file, line and function automatically */
# if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DETAILS)
#define AFB_REQ_VERBOSE(req,level,...) afb_req_verbose(req,level,__FILE__,__LINE__,__func__,__VA_ARGS__)
#else
#define AFB_REQ_VERBOSE(req,level,...) afb_req_verbose(req,level,NULL,0,NULL,__VA_ARGS__)
#endif

/*
 * Check whether the 'permission' is granted or not to the client
 * identified by 'req'.
 *
 * Returns 1 if the permission is granted or 0 otherwise.
 */
static inline int afb_req_has_permission(struct afb_req req, const char *permission)
{
	return req.itf->has_permission(req.closure, permission);
}

/*
 * Get the application identifier of the client application for the
 * request 'req'.
 *
 * Returns the application identifier or NULL when the application
 * can not be identified.
 *
 * The returned value if not NULL must be freed by the caller
 */
static inline char *afb_req_get_application_id(struct afb_req req)
{
	return req.itf->get_application_id(req.closure);
}

/*
 * Get the user identifier (UID) of the client application for the
 * request 'req'.
 *
 * Returns -1 when the application can not be identified.
 */
static inline int afb_req_get_uid(struct afb_req req)
{
	return req.itf->get_uid(req.closure);
}

