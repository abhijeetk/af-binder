/*
 * Copyright (C) 2016, 2017, 2018 "IoT.bzh"
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

struct afb_xreq;
struct afb_apiset;
struct json_object;

struct afb_api_itf
{
	void (*call)(void *closure, struct afb_xreq *xreq);
	int (*service_start)(void *closure, int share_session, int onneed, struct afb_apiset *apiset);
	void (*update_hooks)(void *closure);
	int (*get_verbosity)(void *closure);
	void (*set_verbosity)(void *closure, int level);
	struct json_object *(*describe)(void *closure);
	void (*unref)(void *closure);
};

struct afb_api
{
	void *closure;
	struct afb_api_itf *itf;
	const void *group;
};

extern int afb_api_is_valid_name(const char *name, int hookable);

#define AFB_API_UNHOOKABLE_PREFIX_CHAR    '$'
#define AFB_API_UNHOOKABLE_PREFIX_STRING  "$"
#define afb_api_is_hookable(api)          ((api)[0] != AFB_API_UNHOOKABLE_PREFIX_CHAR)

