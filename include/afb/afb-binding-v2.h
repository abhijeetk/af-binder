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

struct afb_binding_interface;
struct afb_service;
struct json_object;

/*
 * A binding V2 MUST have an exported symbol of name
 *
 *              afbBindingV2
 *
 */
extern const struct afb_binding_v2 afbBindingV2;

/*
 * Description of one verb of the API provided by the binding
 * This enumeration is valid for bindings of type version 2
 */
struct afb_verb_v2
{
	const char *verb;                       /* name of the verb */
	void (*callback)(struct afb_req req);   /* callback function implementing the verb */
	const char * const *permissions;	/* required permissions */
	enum afb_session_v1 session;            /* authorisation and session requirements of the verb */
};

/*
 * Description of the bindings of type version 2
 */
struct afb_binding_v2
{
	const char *api;			/* api name for the binding */
	const char *specification;		/* textual specification of the binding */
	const struct afb_verb_v2 *verbs;	/* array of descriptions of verbs terminated by a NULL name */
	int (*init)(const struct afb_binding_interface *interface);
	int (*start)(const struct afb_binding_interface *interface, struct afb_service service);
	void (*onevent)(const char *event, struct json_object *object);
};

