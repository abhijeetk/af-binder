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

struct afb_export;
struct afb_apiset;

struct afb_service;
struct afb_binding_data_v2;
struct afb_binding_interface_v1;

extern struct afb_export *afb_export_create_v1(const char *apiname, int (*init)(struct afb_service), void (*onevent)(const char*, struct json_object*));
extern struct afb_export *afb_export_create_v2(const char *apiname, struct afb_binding_data_v2 *data, int (*init)(), void (*onevent)(const char*, struct json_object*));
extern void afb_export_destroy(struct afb_export *export);

extern const char *afb_export_apiname(const struct afb_export *export);
extern void afb_export_rename(struct afb_export *export, const char *apiname);
extern void afb_export_update_hook(struct afb_export *export);

extern int afb_export_unshare_session(struct afb_export *export);
extern void afb_export_set_apiset(struct afb_export *export, struct afb_apiset *apiset);
extern struct afb_apiset *afb_export_get_apiset(struct afb_export *export);
	
extern struct afb_binding_v1 *afb_export_register_v1(struct afb_export *export, struct afb_binding_v1 *(*regfun)(const struct afb_binding_interface_v1*));
extern int afb_export_handle_events_v12(struct afb_export *export, void (*on_event)(const char *event, struct json_object *object));

extern int afb_export_start(struct afb_export *export, int share_session, int onneed, struct afb_apiset *apiset);

extern int afb_export_verbosity_get(const struct afb_export *export);
extern void afb_export_verbosity_set(struct afb_export *export, int level);

