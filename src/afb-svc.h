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

struct afb_svc;
struct afb_service;
struct afb_session;
struct afb_apiset;
struct afb_evt_listener;
struct afb_binding_data_v2;

struct json_object;

/*
 * Structure for recording service
 */
struct afb_svc
{
	/* api/prefix */
	const char *api;

	/* session of the service */
	struct afb_session *session;

	/* the apiset for the service */
	struct afb_apiset *apiset;

	/* event listener of the service or NULL */
	struct afb_evt_listener *listener;

	/* on event callback for the service */
	void (*on_event)(const char *event, struct json_object *object);

	/* hooking flags */
	int hookflags;
};

extern void afb_svc_destroy(struct afb_svc *svc, struct afb_service *service);

extern struct afb_svc *afb_svc_create(
			const char *api,
			struct afb_apiset *apiset,
			int share_session,
			void (*on_event)(const char *event, struct json_object *object),
			struct afb_service *service);

extern int afb_svc_start_v1(struct afb_svc *svc, int (*start)(struct afb_service));
extern int afb_svc_start_v2(struct afb_svc *svc, int (*start)());

extern void afb_svc_update_hook(struct afb_svc *svc);

