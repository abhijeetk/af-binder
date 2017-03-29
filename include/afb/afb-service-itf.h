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

#include "afb-service-itf-v1.h"

/**
 * Calls the 'verb' of the 'api' with the arguments 'args' and 'verb' in the name of the binding.
 * The result of the call is delivered to the 'callback' function with the 'callback_closure'.
 *
 * The 'callback' receives 3 arguments:
 *  1. 'closure' the user defined closure pointer 'callback_closure',
 *  2. 'iserror' a boolean status being true (not null) when an error occured,
 *  2. 'result' the resulting data as a JSON object.
 *
 * @param service  The service as received during initialisation
 * @param api      The api name of the method to call
 * @param verb     The verb name of the method to call
 * @param args     The arguments to pass to the method
 * @param callback The to call on completion
 * @param callback_closure The closure to pass to the callback
 *
 * @returns 0 in case of success or -1 in case of error.
 *
 * @see also 'afb_req_subcall'
 */
static inline void afb_service_call(
	struct afb_service service,
	const char *api,
	const char *verb,
	struct json_object *args,
	void (*callback)(void*closure, int iserror, struct json_object *result),
	void *callback_closure)
{
	service.itf->call(service.closure, api, verb, args, callback, callback_closure);
}

