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

struct afb_apiset;
struct afb_dynapi;
struct afb_auth;
struct afb_request;
struct afb_verb_v2;

struct afb_api_dyn_verb
{
	void (*callback)(struct afb_request *request);
	const struct afb_auth *auth;
	const char *info;
	int session;
	char verb[1];
};

struct afb_api_dyn;

extern int afb_api_dyn_add(struct afb_apiset *apiset, const char *name, const char *info, int (*preinit)(void*, struct afb_dynapi*), void *closure);

extern void afb_api_dyn_set_verbs_v2(
		struct afb_api_dyn *dynapi,
		const struct afb_verb_v2 *verbs);

extern int afb_api_dyn_add_verb(
		struct afb_api_dyn *dynapi,
		const char *verb,
		const char *info,
		void (*callback)(struct afb_request *request),
		const struct afb_auth *auth,
		uint32_t session);

extern int afb_api_dyn_sub_verb(
		struct afb_api_dyn *dynapi,
		const char *verb);

