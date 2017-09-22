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

/* defined here */
struct afb_arg;
struct afb_request;
struct afb_request_itf;

/* referenced here */
#include <stdarg.h>
struct json_object;
struct afb_req;
struct afb_event;
struct afb_eventid;
struct afb_stored_req;

/*
 * Describes an argument (or parameter) of a request
 */
struct afb_arg
{
	const char *name;	/* name of the argument or NULL if invalid */
	const char *value;	/* string representation of the value of the argument */
				/* original filename of the argument if path != NULL */
	const char *path;	/* if not NULL, path of the received file for the argument */
				/* when the request is finalized this file is removed */
};

struct afb_request
{
	const struct afb_request_itf *itf;
};

/*
 * Interface for handling requests.
 * It records the functions to be called for the request.
 * Don't use this structure directly.
 * Use the helper functions
 */
struct afb_request_itf
{
	/* CAUTION: respect the order, add at the end */

	struct json_object *(*json)(
			struct afb_request *request);

	struct afb_arg (*get)(
			struct afb_request *request,
			const char *name);

	void (*success)(
			struct afb_request *request,
			struct json_object *obj,
			const char *info);

	void (*fail)(
			struct afb_request *request,
			const char *status,
			const char *info);

	void (*vsuccess)(
			struct afb_request *request,
			struct json_object *obj,
			const char *fmt,
			va_list args);

	void (*vfail)(
			struct afb_request *request,
			const char *status,
			const char *fmt,
			va_list args);

	void *(*context_get)(
			struct afb_request *request);

	void (*context_set)(
			struct afb_request *request,
			void *value,
			void (*free_value)(void*));

	struct afb_request *(*addref)(
			struct afb_request *request);

	void (*unref)(
			struct afb_request *request);

	void (*session_close)(
			struct afb_request *request);

	int (*session_set_LOA)(
			struct afb_request *request,
			unsigned level);

	int (*subscribe)(
			struct afb_request *request,
			struct afb_event event);

	int (*unsubscribe)(
			struct afb_request *request,
			struct afb_event event);

	void (*subcall)(
			struct afb_request *request,
			const char *api,
			const char *verb,
			struct json_object *args,
			void (*callback)(void*, int, struct json_object*),
			void *cb_closure);

	int (*subcallsync)(
			struct afb_request *request,
			const char *api,
			const char *verb,
			struct json_object *args,
			struct json_object **result);

	void (*vverbose)(
			struct afb_request *request,
			int level,
			const char *file,
			int line,
			const char * func,
			const char *fmt,
			va_list args);

	struct afb_stored_req *(*store)(
			struct afb_request *request);

	void (*subcall_req)(
			struct afb_request *request,
			const char *api,
			const char *verb,
			struct json_object *args,
			void (*callback)(void*, int, struct json_object*, struct afb_req),
			void *cb_closure);

	int (*has_permission)(
			struct afb_request *request,
			const char *permission);

	char *(*get_application_id)(
			struct afb_request *request);

	void *(*context_make)(
			struct afb_request *request,
			int replace,
			void *(*create_value)(void *creation_closure),
			void (*free_value)(void*),
			void *creation_closure);
};

