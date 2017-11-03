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

/* declared here */
struct afb_dynapi;
struct afb_dynapi_itf;

/* referenced here */
#include <stdarg.h>
struct sd_event;
struct sd_bus;
struct afb_request;
struct afb_eventid;
struct afb_auth;
struct afb_verb_v2;

/*
 * structure for the dynapi
 */
struct afb_dynapi
{
	/* interface for the dynapi */
	const struct afb_dynapi_itf *itf;

	/* user defined data */
	void *userdata;

	/* current verbosity level */
	int verbosity;

	/* the name of the api */
	const char *apiname;
};

/*
 * Definition of the interface for the API
 */
struct afb_dynapi_itf
{
	/* CAUTION: respect the order, add at the end */

	void (*vverbose)(
		void *dynapi,
		int level,
		const char *file,
		int line,
		const char * func,
		const char *fmt,
		va_list args);

	/* gets the common systemd's event loop */
	struct sd_event *(*get_event_loop)(
		void *dynapi);

	/* gets the common systemd's user d-bus */
	struct sd_bus *(*get_user_bus)(
		void *dynapi);

	/* gets the common systemd's system d-bus */
	struct sd_bus *(*get_system_bus)(
		void *dynapi);

	int (*rootdir_get_fd)(
		void *dynapi);

	int (*rootdir_open_locale)(
		void *dynapi,
		const char *filename,
		int flags,
		const char *locale);

	int (*queue_job)(
		void *dynapi,
		void (*callback)(int signum, void *arg),
		void *argument,
		void *group,
		int timeout);

	int (*require_api)(
		void *dynapi,
		const char *name,
		int initialized);

	int (*rename_api)(
		void *dynapi,
		const char *name);

	/* broadcasts event 'name' with 'object' */
	int (*event_broadcast)(
		void *dynapi,
		const char *name,
		struct json_object *object);

	/* creates an event of 'name' */
	struct afb_eventid *(*eventid_make)(
		void *dynapi,
		const char *name);

	void (*call)(
		struct afb_dynapi *dynapi,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*, struct afb_dynapi *),
		void *callback_closure);

	int (*call_sync)(
		void *dynapi,
		const char *api,
		const char *verb,
		struct json_object *args,
		struct json_object **result);

	int (*api_new_api)(
		void *dynapi,
		const char *api,
		const char *info,
		int noconcurrency,
		int (*preinit)(void*, struct afb_dynapi *),
		void *closure);

	int (*api_set_verbs_v2)(
		struct afb_dynapi *dynapi,
		const struct afb_verb_v2 *verbs);

	int (*api_add_verb)(
		struct afb_dynapi *dynapi,
		const char *verb,
		const char *info,
		void (*callback)(struct afb_request *request),
		void *vcbdata,
		const struct afb_auth *auth,
		uint32_t session);

	int (*api_sub_verb)(
		struct afb_dynapi *dynapi,
		const char *verb);

	int (*api_set_on_event)(
		struct afb_dynapi *dynapi,
		void (*onevent)(struct afb_dynapi *dynapi, const char *event, struct json_object *object));

	int (*api_set_on_init)(
		struct afb_dynapi *dynapi,
		int (*oninit)(struct afb_dynapi *dynapi));

	void (*api_seal)(
		struct afb_dynapi *dynapi);
};

