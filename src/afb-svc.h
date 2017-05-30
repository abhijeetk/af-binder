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
struct afb_apiset;
struct afb_binding_data_v2;

extern struct afb_svc *afb_svc_create_v1(
			const char *api,
			struct afb_apiset *apiset,
			int share_session,
			int (*start)(struct afb_service service),
			void (*on_event)(const char *event, struct json_object *object));

extern struct afb_svc *afb_svc_create_v2(
			const char *api,
			struct afb_apiset *apiset,
			int share_session,
			int (*start)(),
			void (*on_event)(const char *event, struct json_object *object),
			struct afb_binding_data_v2 *data);
