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
#define AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO

#include <string.h>
#include <dlfcn.h>
#include <assert.h>

#include <afb/afb-binding.h>

#include "afb-svc.h"
#include "afb-evt.h"
#include "afb-common.h"
#include "afb-context.h"
#include "afb-api-so.h"
#include "afb-xreq.h"
#include "verbose.h"


struct afb_ditf
{
	int version;
	const char *prefix;
	union {
		struct afb_binding_interface_v1 interface;
		struct afb_daemon daemon;
	};
};

extern void afb_ditf_init_v1(struct afb_ditf *ditf, const char *prefix);
extern void afb_ditf_init_v2(struct afb_ditf *ditf, const char *prefix);
extern void afb_ditf_rename(struct afb_ditf *ditf, const char *prefix);
extern void afb_ditf_update_hook(struct afb_ditf *ditf);

