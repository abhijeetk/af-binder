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

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <assert.h>

#include <json-c/json.h>

#include <afb/afb-binding-v1.h>

#include "afb-api.h"
#include "afb-api-so-v1.h"
#include "afb-apiset.h"
#include "afb-export.h"
#include "afb-common.h"
#include "afb-context.h"
#include "afb-api-so.h"
#include "afb-xreq.h"
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
	struct afb_binding_v1 *binding;	/* descriptor */
	void *handle;			/* context of dlopen */
	struct afb_export *export;	/* export */
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

	xreq->request.dynapi = (void*)desc->export; /* hack: this avoids to export afb_export structure */
	verb = search(desc, xreq->request.verb);
	afb_xreq_call_verb_v1(xreq, verb);
}

static int service_start_cb(void *closure, int share_session, int onneed, struct afb_apiset *apiset)
{
	struct api_so_v1 *desc = closure;
	return afb_export_start(desc->export, share_session, onneed, apiset);
}

static void update_hooks_cb(void *closure)
{
	struct api_so_v1 *desc = closure;
	afb_export_update_hook(desc->export);
}

static int get_verbosity_cb(void *closure)
{
	struct api_so_v1 *desc = closure;
	return afb_export_verbosity_get(desc->export);
}

static void set_verbosity_cb(void *closure, int level)
{
	struct api_so_v1 *desc = closure;
	afb_export_verbosity_set(desc->export, level);
}

static struct json_object *addperm(struct json_object *o, struct json_object *x)
{
	struct json_object *a;

	if (!o)
		return x;

	if (!json_object_object_get_ex(o, "allOf", &a)) {
		a = json_object_new_array();
		json_object_array_add(a, o);
		o = json_object_new_object();
		json_object_object_add(o, "allOf", a);
	}
	json_object_array_add(a, x);
	return o;
}

static struct json_object *addperm_key_val(struct json_object *o, const char *key, struct json_object *val)
{
	struct json_object *x = json_object_new_object();
	json_object_object_add(x, key, val);
	return addperm(o, x);
}

static struct json_object *addperm_key_valstr(struct json_object *o, const char *key, const char *val)
{
	return addperm_key_val(o, key, json_object_new_string(val));
}

static struct json_object *addperm_key_valint(struct json_object *o, const char *key, int val)
{
	return addperm_key_val(o, key, json_object_new_int(val));
}

static struct json_object *make_description_openAPIv3(struct api_so_v1 *desc)
{
	char buffer[256];
	const struct afb_verb_desc_v1 *verb;
	struct json_object *r, *f, *a, *i, *p, *g;

	r = json_object_new_object();
	json_object_object_add(r, "openapi", json_object_new_string("3.0.0"));

	i = json_object_new_object();
	json_object_object_add(r, "info", i);
	json_object_object_add(i, "title", json_object_new_string(afb_export_apiname(desc->export)));
	json_object_object_add(i, "version", json_object_new_string("0.0.0"));
	json_object_object_add(i, "description", json_object_new_string(desc->binding->v1.info ?: afb_export_apiname(desc->export)));

	p = json_object_new_object();
	json_object_object_add(r, "paths", p);
	verb = desc->binding->v1.verbs;
	while (verb->name) {
		buffer[0] = '/';
		strncpy(buffer + 1, verb->name, sizeof buffer - 1);
		buffer[sizeof buffer - 1] = 0;
		f = json_object_new_object();
		json_object_object_add(p, buffer, f);
		g = json_object_new_object();
		json_object_object_add(f, "get", g);

		a = NULL;
		if (verb->session & AFB_SESSION_CLOSE_V1)
			a = addperm_key_valstr(a, "session", "close");
		if (verb->session & AFB_SESSION_CHECK_V1)
			a = addperm_key_valstr(a, "session", "check");
		if (verb->session & AFB_SESSION_RENEW_V1)
			a = addperm_key_valstr(a, "token", "refresh");
		if (verb->session & AFB_SESSION_LOA_MASK_V1)
			a = addperm_key_valint(a, "LOA", (verb->session >> AFB_SESSION_LOA_SHIFT_V1) & AFB_SESSION_LOA_MASK_V1);
		if (a)
			json_object_object_add(g, "x-permissions", a);

		a = json_object_new_object();
		json_object_object_add(g, "responses", a);
		f = json_object_new_object();
		json_object_object_add(a, "200", f);
		json_object_object_add(f, "description", json_object_new_string(verb->info));
		verb++;
	}
	return r;
}

static struct json_object *describe_cb(void *closure)
{
	struct api_so_v1 *desc = closure;

	return make_description_openAPIv3(desc);
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
	struct afb_binding_v1 *(*register_function) (const struct afb_binding_interface_v1 *interface);
	int (*init)(struct afb_service service);
	void (*onevent)(const char *event, struct json_object *object);
	struct afb_api afb_api;
	struct afb_export *export;

	/* retrieves the register function */
	register_function = dlsym(handle, afb_api_so_v1_register);
	if (!register_function)
		return 0;
	INFO("binding [%s] is a valid AFB binding V1", path);

	/* allocates the description */
	init = dlsym(handle, afb_api_so_v1_service_init);
	onevent = dlsym(handle, afb_api_so_v1_service_event);
	export = afb_export_create_v1(apiset, path, init, onevent);
	desc = calloc(1, sizeof *desc);
	if (desc == NULL || export == NULL) {
		ERROR("out of memory");
		goto error;
	}
	desc->export = export;
	desc->handle = handle;

	/* init the binding */
	INFO("binding [%s] calling registering function %s", path, afb_api_so_v1_register);
	desc->binding = afb_export_register_v1(desc->export, register_function);
	if (desc->binding == NULL) {
		ERROR("binding [%s] register function failed. continuing...", path);
		goto error;
	}

	/* check the returned structure */
	if (desc->binding->type != AFB_BINDING_VERSION_1) {
		ERROR("binding [%s] invalid type %d...", path, desc->binding->type);
		goto error;
	}
	if (desc->binding->v1.prefix == NULL || *desc->binding->v1.prefix == 0) {
		ERROR("binding [%s] bad prefix...", path);
		goto error;
	}
	if (!afb_api_is_valid_name(desc->binding->v1.prefix)) {
		ERROR("binding [%s] invalid prefix...", path);
		goto error;
	}
	if (desc->binding->v1.info == NULL || *desc->binding->v1.info == 0) {
		ERROR("binding [%s] bad description...", path);
		goto error;
	}
	if (desc->binding->v1.verbs == NULL) {
		ERROR("binding [%s] no APIs...", path);
		goto error;
	}

	/* records the binding */
	if (!strcmp(path, afb_export_apiname(desc->export)))
		afb_export_rename(desc->export, desc->binding->v1.prefix);
	afb_api.closure = desc;
	afb_api.itf = &so_v1_api_itf;
	afb_api.group = NULL;
	if (afb_apiset_add(apiset, afb_export_apiname(desc->export), afb_api) < 0) {
		ERROR("binding [%s] can't be registered...", path);
		goto error;
	}
	INFO("binding %s loaded with API prefix %s", path, afb_export_apiname(desc->export));
	return 1;

error:
	afb_export_destroy(export);
	free(desc);

	return -1;
}

