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

#define _GNU_SOURCE

#include <stdlib.h>
#include <dlfcn.h>

#include "afb-api-so-vdyn.h"
#include "afb-export.h"
#include "verbose.h"

/*
 * names of symbols
 */
static const char afb_api_so_vdyn_entry[] = "afbBindingVdyn";

/*
 * Description of a binding
 */
static int vdyn_preinit(void *closure, struct afb_dynapi *dynapi)
{
	int (*entry)(struct afb_dynapi*) = closure;
	return entry(dynapi);
}

int afb_api_so_vdyn_add(const char *path, void *handle, struct afb_apiset *apiset)
{
	int rc;
	int (*entry)(void*, struct afb_dynapi*);
	struct afb_export *export;

	entry = dlsym(handle, afb_api_so_vdyn_entry);
	if (!entry)
		return 0;

	INFO("binding [%s] looks like an AFB binding Vdyn", path);

	export = afb_export_create_vdyn(apiset, path, NULL);
	if (!export) {
		ERROR("can't create export for %s", path);
		return -1;
	}

	INFO("binding [%s] calling dynamic initialisation %s", path, afb_api_so_vdyn_entry);
	rc = afb_export_preinit_vdyn(export, vdyn_preinit, entry);
	afb_export_destroy(export);
	return rc < 0 ? rc : 1;
}

