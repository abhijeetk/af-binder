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

#include <json-c/json.h>

#include <afb/afb-binding.h>

#include "afb-api.h"
#include "afb-api-so-v1.h"
#include "afb-apiset.h"
#include "afb-svc.h"
#include "afb-evt.h"
#include "afb-common.h"
#include "afb-context.h"
#include "afb-api-so.h"
#include "afb-xreq.h"
#include "afb-ditf.h"
#include "verbose.h"

/*
 * names of symbols
 */
static const char afb_api_so_v1_register[] = "afbBindingV1Register";
static const char afb_api_so_v1_service_init[] = "afbBindingV1ServiceInit";
static const char afb_api_so_v1_service_event[] = "afbBindingV1ServiceEvent";

/*
 * Description of a binding
 */
struct api_so_v1 {
	struct afb_binding *binding;	/* descriptor */
	void *handle;			/* context of dlopen */
	struct afb_svc *service;	/* handler for service started */
	struct afb_ditf ditf;		/* daemon interface */
};

static const struct afb_verb_desc_v1 *search(struct api_so_v1 *desc, const char *name)
{
	const struct afb_verb_desc_v1 *verb;

	verb = desc->binding->v1.verbs;
	while (verb->name && strcasecmp(verb->name, name))
		verb++;
	return verb->name ? verb : NULL;
}

static void call_cb(void *closure, struct afb_xreq *xreq)
{
	const struct afb_verb_desc_v1 *verb;
	struct api_so_v1 *desc = closure;

	verb = search(desc, xreq->verb);
	afb_xreq_call_verb_v1(xreq, verb);
}

static int service_start_cb(void *closure, int share_session, int onneed, struct afb_apiset *apiset)
{
	int (*init)(struct afb_service service);
	void (*onevent)(const char *event, struct json_object *object);

	struct api_so_v1 *desc = closure;

	/* check state */
	if (desc->service != NULL) {
		/* not an error when onneed */
		if (onneed != 0)
			return 0;

		/* already started: it is an error */
		ERROR("Service %s already started", desc->binding->v1.prefix);
		return -1;
	}

	/* get the initialisation */
	init = dlsym(desc->handle, afb_api_so_v1_service_init);
	if (init == NULL) {
		/* not an error when onneed */
		if (onneed != 0)
			return 0;

		/* no initialisation method */
		ERROR("Binding %s is not a service", desc->binding->v1.prefix);
		return -1;
	}

	/* get the event handler if any */
	onevent = dlsym(desc->handle, afb_api_so_v1_service_event);
	desc->service = afb_svc_create_v1(apiset, share_session, init, onevent);
	if (desc->service == NULL) {
		/* starting error */
		ERROR("Starting service %s failed", desc->binding->v1.prefix);
		return -1;
	}

	return 0;
}

static void update_hooks_cb(void *closure)
{
	struct api_so_v1 *desc = closure;
	afb_ditf_update_hook(&desc->ditf);
}

static int get_verbosity_cb(void *closure)
{
	struct api_so_v1 *desc = closure;
	return desc->ditf.interface.verbosity;
}

static void set_verbosity_cb(void *closure, int level)
{
	struct api_so_v1 *desc = closure;
	desc->ditf.interface.verbosity = level;
}

struct json_object *describe_cb(void *closure)
{
	struct api_so_v1 *desc = closure;
	const struct afb_verb_desc_v1 *verb;
	struct json_object *r, *v, *f, *a;

	r = json_object_new_object();
	json_object_object_add(r, "version", json_object_new_int(1));
	json_object_object_add(r, "info", json_object_new_string(desc->binding->v1.info));
	v = json_object_new_object();
	json_object_object_add(r, "verbs", v);
	verb = desc->binding->v1.verbs;
	while (verb->name) {
		f = json_object_new_object();
		a = json_object_new_array();
		json_object_object_add(f, "name", json_object_new_string(verb->name));
		json_object_object_add(f, "info", json_object_new_string(verb->info));
		if (verb->session & AFB_SESSION_CLOSE)
			json_object_array_add(a, json_object_new_string("session-close"));
		if (verb->session & AFB_SESSION_RENEW)
			json_object_array_add(a, json_object_new_string("session-renew"));
		if (verb->session & AFB_SESSION_CHECK)
			json_object_array_add(a, json_object_new_string("session-check"));
		if (verb->session & AFB_SESSION_LOA_EQ) {
			const char *rel = "?";
			char buffer[80];
			switch (verb->session & AFB_SESSION_LOA_EQ) {
			case AFB_SESSION_LOA_GE: rel = ">="; break;
			case AFB_SESSION_LOA_LE: rel = "<="; break;
			case AFB_SESSION_LOA_EQ: rel = "=="; break;
			}
			snprintf(buffer, sizeof buffer, "LOA%s%d", rel, (int)((verb->session >> AFB_SESSION_LOA_SHIFT) & AFB_SESSION_LOA_MASK));
			json_object_array_add(a, json_object_new_string(buffer));
		}
		json_object_object_add(f, "flags", a);
		json_object_object_add(v, verb->name, f);
		verb++;
	}
	return r;
}

static struct afb_api_itf so_v1_api_itf = {
	.call = call_cb,
	.service_start = service_start_cb,
	.update_hooks = update_hooks_cb,
	.get_verbosity = get_verbosity_cb,
	.set_verbosity = set_verbosity_cb,
	.describe = describe_cb
};

int afb_api_so_v1_add(const char *path, void *handle, struct afb_apiset *apiset)
{
	struct api_so_v1 *desc;
	struct afb_binding *(*register_function) (const struct afb_binding_interface *interface);
	struct afb_api afb_api;

	/* retrieves the register function */
	register_function = dlsym(handle, afb_api_so_v1_register);
	if (!register_function)
		return 0;
	INFO("binding [%s] is a valid AFB binding V1", path);

	/* allocates the description */
	desc = calloc(1, sizeof *desc);
	if (desc == NULL) {
		ERROR("out of memory");
		goto error;
	}
	desc->handle = handle;

	/* init the interface */
	afb_ditf_init_v1(&desc->ditf, path);

	/* init the binding */
	NOTICE("binding [%s] calling registering function %s", path, afb_api_so_v1_register);
	desc->binding = register_function(&desc->ditf.interface);
	if (desc->binding == NULL) {
		ERROR("binding [%s] register function failed. continuing...", path);
		goto error2;
	}

	/* check the returned structure */
	if (desc->binding->type != AFB_BINDING_VERSION_1) {
		ERROR("binding [%s] invalid type %d...", path, desc->binding->type);
		goto error2;
	}
	if (desc->binding->v1.prefix == NULL || *desc->binding->v1.prefix == 0) {
		ERROR("binding [%s] bad prefix...", path);
		goto error2;
	}
	if (!afb_api_is_valid_name(desc->binding->v1.prefix)) {
		ERROR("binding [%s] invalid prefix...", path);
		goto error2;
	}
	if (desc->binding->v1.info == NULL || *desc->binding->v1.info == 0) {
		ERROR("binding [%s] bad description...", path);
		goto error2;
	}
	if (desc->binding->v1.verbs == NULL) {
		ERROR("binding [%s] no APIs...", path);
		goto error2;
	}

	/* records the binding */
	afb_ditf_rename(&desc->ditf, desc->binding->v1.prefix);
	afb_api.closure = desc;
	afb_api.itf = &so_v1_api_itf;
	if (afb_apiset_add(apiset, desc->binding->v1.prefix, afb_api) < 0) {
		ERROR("binding [%s] can't be registered...", path);
		goto error2;
	}
	NOTICE("binding %s loaded with API prefix %s", path, desc->binding->v1.prefix);
	return 1;

error2:
	free(desc);
error:
	return -1;
}

