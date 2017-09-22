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

#include "afb-request-itf.h"

typedef struct afb_request afb_request;

#include "afb-event.h"

/*
 * Gets from the request 'request' the argument of 'name'.
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
static inline struct afb_arg afb_request_get(struct afb_request *request, const char *name)
{
	return request->itf->get(request, name);
}

/*
 * Gets from the request 'request' the string value of the argument of 'name'.
 * Returns NULL if when there is no argument of 'name'.
 * Returns the value of the argument of 'name' otherwise.
 *
 * Shortcut for: afb_request_get(request, name).value
 */
static inline const char *afb_request_value(struct afb_request *request, const char *name)
{
	return afb_request_get(request, name).value;
}

/*
 * Gets from the request 'request' the path for file attached to the argument of 'name'.
 * Returns NULL if when there is no argument of 'name' or when there is no file.
 * Returns the path of the argument of 'name' otherwise.
 *
 * Shortcut for: afb_request_get(request, name).path
 */
static inline const char *afb_request_path(struct afb_request *request, const char *name)
{
	return afb_request_get(request, name).path;
}

/*
 * Gets from the request 'request' the json object hashing the arguments.
 * The returned object must not be released using 'json_object_put'.
 */
static inline struct json_object *afb_request_json(struct afb_request *request)
{
	return request->itf->json(request);
}

/*
 * Sends a reply of kind success to the request 'request'.
 * The status of the reply is automatically set to "success".
 * Its send the object 'obj' (can be NULL) with an
 * informationnal comment 'info (can also be NULL).
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
static inline void afb_request_success(struct afb_request *request, struct json_object *obj, const char *info)
{
	request->itf->success(request, obj, info);
}

/*
 * Same as 'afb_request_success' but the 'info' is a formatting
 * string followed by arguments.
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
static inline void afb_request_success_f(struct afb_request *request, struct json_object *obj, const char *info, ...) __attribute__((format(printf, 3, 4)));
static inline void afb_request_success_f(struct afb_request *request, struct json_object *obj, const char *info, ...)
{
	va_list args;
	va_start(args, info);
	request->itf->vsuccess(request, obj, info, args);
	va_end(args);
}

/*
 * Same as 'afb_request_success_f' but the arguments to the format 'info'
 * are given as a variable argument list instance.
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
static inline void afb_request_success_v(struct afb_request *request, struct json_object *obj, const char *info, va_list args)
{
	request->itf->vsuccess(request, obj, info, args);
}

/*
 * Sends a reply of kind failure to the request 'request'.
 * The status of the reply is set to 'status' and an
 * informationnal comment 'info' (can also be NULL) can be added.
 *
 * Note that calling afb_request_fail("success", info) is equivalent
 * to call afb_request_success(NULL, info). Thus even if possible it
 * is strongly recommanded to NEVER use "success" for status.
 */
static inline void afb_request_fail(struct afb_request *request, const char *status, const char *info)
{
	request->itf->fail(request, status, info);
}

/*
 * Same as 'afb_request_fail' but the 'info' is a formatting
 * string followed by arguments.
 */
static inline void afb_request_fail_f(struct afb_request *request, const char *status, const char *info, ...) __attribute__((format(printf, 3, 4)));
static inline void afb_request_fail_f(struct afb_request *request, const char *status, const char *info, ...)
{
	va_list args;
	va_start(args, info);
	request->itf->vfail(request, status, info, args);
	va_end(args);
}

/*
 * Same as 'afb_request_fail_f' but the arguments to the format 'info'
 * are given as a variable argument list instance.
 */
static inline void afb_request_fail_v(struct afb_request *request, const char *status, const char *info, va_list args)
{
	request->itf->vfail(request, status, info, args);
}

/*
 * Gets the pointer stored by the binding for the session of 'request'.
 * When the binding has not yet recorded a pointer, NULL is returned.
 */
static inline void *afb_request_context_get(struct afb_request *request)
{
	return request->itf->context_make(request, 0, 0, 0, 0);
}

/*
 * Stores for the binding the pointer 'context' to the session of 'request'.
 * The function 'free_context' will be called when the session is closed
 * or if binding stores an other pointer.
 */
static inline void afb_request_context_set(struct afb_request *request, void *context, void (*free_context)(void*))
{
	request->itf->context_make(request, 1, 0, free_context, context);
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
static inline void *afb_request_context(struct afb_request *request, int replace, void *(*create_context)(void *closure), void (*free_context)(void*), void *closure)
{
	return request->itf->context_make(request, replace, create_context, free_context, closure);
}

/*
 * Frees the pointer stored by the binding for the session of 'request'
 * and sets it to NULL.
 *
 * Shortcut for: afb_request_context_set(request, NULL, NULL)
 */
static inline void afb_request_context_clear(struct afb_request *request)
{
	request->itf->context_make(request, 1, 0, 0, 0);
}

/*
 * Adds one to the count of references of 'request'.
 * This function MUST be called by asynchronous implementations
 * of verbs if no reply was sent before returning.
 */
static inline struct afb_request *afb_request_addref(struct afb_request *request)
{
	return request->itf->addref(request);
}

/*
 * Substracts one to the count of references of 'request'.
 * This function MUST be called by asynchronous implementations
 * of verbs after sending the asynchronous reply.
 */
static inline void afb_request_unref(struct afb_request *request)
{
	request->itf->unref(request);
}

/*
 * Closes the session associated with 'request'
 * and delete all associated contexts.
 */
static inline void afb_request_session_close(struct afb_request *request)
{
	request->itf->session_close(request);
}

/*
 * Sets the level of assurance of the session of 'request'
 * to 'level'. The effect of this function is subject of
 * security policies.
 * Returns 1 on success or 0 if failed.
 */
static inline int afb_request_session_set_LOA(struct afb_request *request, unsigned level)
{
	return request->itf->session_set_LOA(request, level);
}

/*
 * Establishes for the client link identified by 'request' a subscription
 * to the 'event'.
 * Returns 0 in case of successful subscription or -1 in case of error.
 */
static inline int afb_request_subscribe(struct afb_request *request, struct afb_event event)
{
	return request->itf->subscribe(request, event);
}

/*
 * Revokes the subscription established to the 'event' for the client
 * link identified by 'request'.
 * Returns 0 in case of successful subscription or -1 in case of error.
 */
static inline int afb_request_unsubscribe(struct afb_request *request, struct afb_event event)
{
	return request->itf->unsubscribe(request, event);
}

/*
 * Makes a call to the method of name 'api' / 'verb' with the object 'args'.
 * This call is made in the context of the request 'request'.
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
 *  - 'afb_request_subcall_req' that is convenient to keep request alive automatically.
 *  - 'afb_request_subcall_sync' the synchronous version
 */
static inline void afb_request_subcall(struct afb_request *request, const char *api, const char *verb, struct json_object *args, void (*callback)(void *closure, int iserror, struct json_object *result), void *closure)
{
	request->itf->subcall(request, api, verb, args, callback, closure);
}

/*
 * Makes a call to the method of name 'api' / 'verb' with the object 'args'.
 * This call is made in the context of the request 'request'.
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
 *  - 'afb_request_subcall_req' that is convenient to keep request alive automatically.
 *  - 'afb_request_subcall' that doesn't keep request alive automatically.
 */
static inline int afb_request_subcall_sync(struct afb_request *request, const char *api, const char *verb, struct json_object *args, struct json_object **result)
{
	return request->itf->subcallsync(request, api, verb, args, result);
}

/*
 * Send associated to 'request' a message described by 'fmt' and following parameters
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
static inline void afb_request_verbose(struct afb_request *request, int level, const char *file, int line, const char * func, const char *fmt, ...) __attribute__((format(printf, 6, 7)));
static inline void afb_request_verbose(struct afb_request *request, int level, const char *file, int line, const char * func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	request->itf->vverbose(request, level, file, line, func, fmt, args);
	va_end(args);
}

/* macro for setting file, line and function automatically */
# if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DETAILS)
#define AFB_REQUEST_VERBOSE(request,level,...) afb_request_verbose(request,level,__FILE__,__LINE__,__func__,__VA_ARGS__)
#else
#define AFB_REQUEST_VERBOSE(request,level,...) afb_request_verbose(request,level,NULL,0,NULL,__VA_ARGS__)
#endif

/*
 * Check whether the 'permission' is granted or not to the client
 * identified by 'request'.
 *
 * Returns 1 if the permission is granted or 0 otherwise.
 */
static inline int afb_request_has_permission(struct afb_request *request, const char *permission)
{
	return request->itf->has_permission(request, permission);
}

/*
 * Get the application identifier of the client application for the
 * request 'request'.
 *
 * Returns the application identifier or NULL when the application
 * can not be identified.
 *
 * The returned value if not NULL must be freed by the caller
 */
static inline char *afb_request_get_application_id(struct afb_request *request)
{
	return request->itf->get_application_id(request);
}

