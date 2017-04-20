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

struct afb_api;
struct afb_apiset;

extern struct afb_apiset *afb_apiset_addref(struct afb_apiset *set);
extern void afb_apiset_unref(struct afb_apiset *set);
extern struct afb_apiset *afb_apiset_create(const char *name, int timeout);
extern int afb_apiset_timeout_get(struct afb_apiset *set);
extern void afb_apiset_timeout_set(struct afb_apiset *set, int to);
extern void afb_apiset_subset_set(struct afb_apiset *set, struct afb_apiset *subset);
extern struct afb_apiset *afb_apiset_subset_get(struct afb_apiset *set);
extern int afb_apiset_default_api_exist(struct afb_apiset *set);
extern int afb_apiset_default_api_get(struct afb_apiset *set, struct afb_api *api);
extern void afb_apiset_default_api_set(struct afb_apiset *set, struct afb_api api);
extern void afb_apiset_default_api_drop(struct afb_apiset *set);
extern int afb_apiset_add(struct afb_apiset *set, const char *name, struct afb_api api);
extern int afb_apiset_del(struct afb_apiset *set, const char *name);
extern int afb_apiset_get(struct afb_apiset *set, const char *name, struct afb_api *api);
extern int afb_apiset_start_service(struct afb_apiset *set, const char *name, int share_session, int onneed);
extern int afb_apiset_start_all_services(struct afb_apiset *set, int share_session);
extern void afb_apiset_update_hooks(struct afb_apiset *set, const char *name);
extern void afb_apiset_set_verbosity(struct afb_apiset *set, const char *name, int level);
extern int afb_apiset_get_verbosity(struct afb_apiset *set, const char *name);
extern const char **afb_apiset_get_names(struct afb_apiset *set);

