/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
 * Author José Bollo <jose.bollo@iot.bzh>
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

struct afb_binding_interface_v1;
struct afb_binding_data_v2;

struct afb_ditf
{
	int version;
	const char *api;
	union {
		struct afb_binding_interface_v1 *v1;
		struct afb_binding_data_v2 *v2;
	};
};

extern void afb_ditf_init_v1(struct afb_ditf *ditf, const char *api, struct afb_binding_interface_v1 *itf);
extern void afb_ditf_init_v2(struct afb_ditf *ditf, const char *api, struct afb_binding_data_v2 *data);
extern void afb_ditf_rename(struct afb_ditf *ditf, const char *api);
extern void afb_ditf_update_hook(struct afb_ditf *ditf);

