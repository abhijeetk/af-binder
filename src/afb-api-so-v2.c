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
#include <string.h>
#include <dlfcn.h>
#include <assert.h>

#include <afb/afb-binding-v2.h>
#include <json-c/json.h>

#include "afb-api.h"
#include "afb-api-so-v2.h"
#include "afb-apiset.h"
#include "afb-export.h"
#include "afb-evt.h"
#include "afb-common.h"
#include "afb-context.h"
#include "afb-api-so.h"
#include "afb-xreq.h"
#include "verbose.h"

/*
 * names of symbols
 */
static const char afb_api_so_v2_descriptor[] = "afbBindingV2";
static const char afb_api_so_v2_data[] = "afbBindingV2data";

/*
 * Description of a binding
 */
struct api_so_v2 {
	const struct afb_binding_v2 *binding;	/* descriptor */
	void *handle;				/* context of dlopen */
	struct afb_export *export;		/* exportations */
};

static const struct afb_verb_v2 *search(struct api_so_v2 *desc, const char *name)
{
	const struct afb_verb_v2 *verb;

	verb = desc->binding->verbs;
	while (verb->verb && strcasecmp(verb->verb, name))
		verb++;
	return verb->verb ? verb : NULL;
	return NULL;
}

static void call_cb(void *closure, struct afb_xreq *xreq)
{
	struct api_so_v2 *desc = closure;
	const struct afb_verb_v2 *verb;

	verb = search(desc, xreq->verb);
	afb_xreq_call_verb_v2(xreq, verb);
}

static int service_start_cb(void *closure, int share_session, int onneed, struct afb_apiset *apiset)
{
	struct api_so_v2 *desc = closure;
	return afb_export_start(desc->export, share_session, onneed, apiset);
}

static void update_hooks_cb(void *closure)
{
	struct api_so_v2 *desc = closure;
	afb_export_update_hook(desc->export);
}

static int get_verbosity_cb(void *closure)
{
	struct api_so_v2 *desc = closure;
	return afb_export_verbosity_get(desc->export);
}

static void set_verbosity_cb(void *closure, int level)
{
	struct api_so_v2 *desc = closure;
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

static struct json_object *addauth_or_array(struct json_object *o, const struct afb_auth *auth);

static struct json_object *addauth(struct json_object *o, const struct afb_auth *auth)
{
	switch(auth->type) {
	case afb_auth_No: return addperm(o, json_object_new_boolean(0));
	case afb_auth_Token: return addperm_key_valstr(o, "session", "check");
	case afb_auth_LOA: return addperm_key_valint(o, "LOA", auth->loa);
	case afb_auth_Permission: return addperm_key_valstr(o, "permission", auth->text);
	case afb_auth_Or: return addperm_key_val(o, "anyOf", addauth_or_array(json_object_new_array(), auth));
	case afb_auth_And: return addauth(addauth(o, auth->first), auth->next);
	case afb_auth_Not: return addperm_key_val(o, "not", addauth(NULL, auth->first));
	case afb_auth_Yes: return addperm(o, json_object_new_boolean(1));
	}
	return o;
}

static struct json_object *addauth_or_array(struct json_object *o, const struct afb_auth *auth)
{
	if (auth->type != afb_auth_Or)
		json_object_array_add(o, addauth(NULL, auth));
	else {
		addauth_or_array(o, auth->first);
		addauth_or_array(o, auth->next);
	}

	return o;
}

static struct json_object *make_description_openAPIv3(struct api_so_v2 *desc)
{
	char buffer[256];
	const struct afb_verb_v2 *verb;
	struct json_object *r, *f, *a, *i, *p, *g;

	r = json_object_new_object();
	json_object_object_add(r, "openapi", json_object_new_string("3.0.0"));

	i = json_object_new_object();
	json_object_object_add(r, "info", i);
	json_object_object_add(i, "title", json_object_new_string(afb_export_apiname(desc->export)));
	json_object_object_add(i, "version", json_object_new_string("0.0.0"));
	json_object_object_add(i, "description", json_object_new_string(desc->binding->info ?: afb_export_apiname(desc->export)));

	p = json_object_new_object();
	json_object_object_add(r, "paths", p);
	verb = desc->binding->verbs;
	while (verb->verb) {
		buffer[0] = '/';
		strncpy(buffer + 1, verb->verb, sizeof buffer - 1);
		buffer[sizeof buffer - 1] = 0;
		f = json_object_new_object();
		json_object_object_add(p, buffer, f);
		g = json_object_new_object();
		json_object_object_add(f, "get", g);

		a = NULL;
		if (verb->session & AFB_SESSION_CLOSE_V2)
			a = addperm_key_valstr(a, "session", "close");
		if (verb->session & AFB_SESSION_CHECK_V2)
			a = addperm_key_valstr(a, "session", "check");
		if (verb->session & AFB_SESSION_REFRESH_V2)
			a = addperm_key_valstr(a, "token", "refresh");
		if (verb->session & AFB_SESSION_LOA_MASK_V2)
			a = addperm_key_valint(a, "LOA", verb->session & AFB_SESSION_LOA_MASK_V2);
		if (verb->auth)
			a = addauth(a, verb->auth);
		if (a)
			json_object_object_add(g, "x-permissions", a);

		a = json_object_new_object();
		json_object_object_add(g, "responses", a);
		f = json_object_new_object();
		json_object_object_add(a, "200", f);
		json_object_object_add(f, "description", json_object_new_string(verb->info?:verb->verb));
		verb++;
	}
	return r;
}

static struct json_object *describe_cb(void *closure)
{
	struct api_so_v2 *desc = closure;
	struct json_object *r = desc->binding->specification ? json_tokener_parse(desc->binding->specification) : NULL;
	if (!r)
		r = make_description_openAPIv3(desc);
	return r;
}

static struct afb_api_itf so_v2_api_itf = {
	.call = call_cb,
	.service_start = service_start_cb,
	.update_hooks = update_hooks_cb,
	.get_verbosity = get_verbosity_cb,
	.set_verbosity = set_verbosity_cb,
	.describe = describe_cb
};

int afb_api_so_v2_add_binding(const struct afb_binding_v2 *binding, void *handle, struct afb_apiset *apiset, struct afb_binding_data_v2 *data)
{
	int rc;
	struct api_so_v2 *desc;
	struct afb_api afb_api;
	struct afb_export *export;

	/* basic checks */
	assert(binding);
	assert(binding->api);
	assert(binding->verbs);
	assert(data);

	/* allocates the description */
	export = afb_export_create_v2(binding->api, data, binding->init, binding->onevent);
	desc = calloc(1, sizeof *desc);
	if (desc == NULL) {
		ERROR("out of memory");
		goto error;
	}
	desc->binding = binding;
	desc->handle = handle;
	desc->export = export;

	/* init the binding */
	if (binding->preinit) {
		INFO("binding %s calling preinit function", binding->api);
		rc = binding->preinit();
		if (rc < 0) {
			ERROR("binding %s preinit function failed...", afb_export_apiname(desc->export));
			goto error;
		}
	}

	/* records the binding */
	afb_api.closure = desc;
	afb_api.itf = &so_v2_api_itf;
	afb_api.noconcurrency = binding->noconcurrency;
	if (afb_apiset_add(apiset, afb_export_apiname(desc->export), afb_api) < 0) {
		ERROR("binding %s can't be registered to set %s...", afb_export_apiname(desc->export), afb_apiset_name(apiset));
		goto error;
	}
	INFO("binding %s added to set %s", afb_export_apiname(desc->export), afb_apiset_name(apiset));
	return 1;

error:
	afb_export_destroy(export);
	free(desc);

	return -1;
}

int afb_api_so_v2_add(const char *path, void *handle, struct afb_apiset *apiset)
{
	const struct afb_binding_v2 *binding;
	struct afb_binding_data_v2 *data;

	/* retrieves the register function */
	binding = dlsym(handle, afb_api_so_v2_descriptor);
	data = dlsym(handle, afb_api_so_v2_data);
	if (!binding && !data)
		return 0;

	INFO("binding [%s] looks like an AFB binding V2", path);

	/* basic checks */
	if (!binding || !data) {
		ERROR("binding [%s] incomplete symbol set: %s is missing",
			path, binding ? afb_api_so_v2_data : afb_api_so_v2_descriptor);
		goto error;
	}
	if (binding->api == NULL || *binding->api == 0) {
		ERROR("binding [%s] bad api name...", path);
		goto error;
	}
	if (!afb_api_is_valid_name(binding->api)) {
		ERROR("binding [%s] invalid api name...", path);
		goto error;
	}
#if 0
	if (binding->specification == NULL || *binding->specification == 0) {
		ERROR("binding [%s] bad specification...", path);
		goto error;
	}
#endif
	if (binding->verbs == NULL) {
		ERROR("binding [%s] no verbs...", path);
		goto error;
	}

	return afb_api_so_v2_add_binding(binding, handle, apiset, data);

 error:
	return -1;
}

