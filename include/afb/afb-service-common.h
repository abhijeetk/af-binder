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

/* avoid inclusion of <json-c/json.h> */
struct json_object;

/*
 * Interface for internal of services
 * It records the functions to be called for the request.
 * Don't use this structure directly.
 * Use the helper functions documented below.
 */
struct afb_service_itf
{
	/* CAUTION: respect the order, add at the end */

	void (*call)(void *closure, const char *api, const char *verb, struct json_object *args,
	             void (*callback)(void*, int, struct json_object*), void *callback_closure);
};

/*
 * Object that encapsulate accesses to service items
 */
struct afb_service
{
	const struct afb_service_itf *itf;
	void *closure;
};

