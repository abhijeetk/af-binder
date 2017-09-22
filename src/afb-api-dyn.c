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
#include <assert.h>
#include <errno.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 0
#include <afb/afb-binding.h>

#include "afb-api.h"
#include "afb-api-dyn.h"
#include "afb-apiset.h"
#include "afb-auth.h"
#include "afb-export.h"
#include "afb-xreq.h"
#include "verbose.h"

/*
 * Description of a binding
 */
struct afb_api_dyn {
	int count;
	struct afb_api_dyn_verb **verbs;
	const struct afb_verb_v2 *verbsv2;
	struct afb_export *export;
	char info[1];
};

void afb_api_dyn_set_verbs_v2(
		struct afb_api_dyn *dynapi,
		const struct afb_verb_v2 *verbs)
{
	dynapi->verbsv2 = verbs;
}

int afb_api_dyn_add_verb(
		struct afb_api_dyn *dynapi,
		const char *verb,
		const char *info,
		void (*callback)(struct afb_request *request),
		void *vcbdata,
		const struct afb_auth *auth,
		uint32_t session)
{
	struct afb_api_dyn_verb *v, **vv;

	afb_api_dyn_sub_verb(dynapi, verb);

	vv = realloc(dynapi->verbs, (1 + dynapi->count) * sizeof *vv);
	if (!vv)
		goto oom;
	dynapi->verbs = vv;

	v = malloc(sizeof *v + strlen(verb) + (info ? 1 + strlen(info) : 0));
	if (!v)
		goto oom;

	v->callback = callback;
	v->vcbdata = vcbdata;
	v->auth = auth;
	v->session = session;

	v->info = 1 + stpcpy(v->verb, verb);
	if (info)
		strcpy((char*)v->info, info);
	else
		v->info = NULL;

	dynapi->verbs[dynapi->count++] = v;
	return 0;
oom:
	errno = ENOMEM;
	return -1;
}

int afb_api_dyn_sub_verb(
		struct afb_api_dyn *dynapi,
		const char *verb)
{
	struct afb_api_dyn_verb *v;
	int i;

	/* look first in dyna mic verbs */
	for (i = 0 ; i < dynapi->count ; i++) {
		v = dynapi->verbs[i];
		if (!strcasecmp(v->verb, verb)) {
			if (i != --dynapi->count)
				dynapi->verbs[i] = dynapi->verbs[dynapi->count];
			free(v);
			return 0;
		}
	}

	errno = ENOENT;
	return -1;
}

static void call_cb(void *closure, struct afb_xreq *xreq)
{
	struct afb_api_dyn *dynapi = closure;
	struct afb_api_dyn_verb **verbs, *v;
	const struct afb_verb_v2 *verbsv2;
	int i;
	const char *name;

	name = xreq->request.verb;
	xreq->request.dynapi = (void*)dynapi->export; /* hack: this avoids to export afb_export structure */

	/* look first in dyna mic verbs */
	verbs = dynapi->verbs;
	i = dynapi->count;
	while (i) {
		v = verbs[--i];
		if (!strcasecmp(v->verb, name)) {
			xreq->request.vcbdata = v->vcbdata;
			afb_xreq_call_verb_vdyn(xreq, verbs[i]);
			return;
		}
	}

	verbsv2 = dynapi->verbsv2;
	if (verbsv2) {
		while (verbsv2->verb) {
			if (strcasecmp(verbsv2->verb, name))
				verbsv2++;
			else {
				afb_xreq_call_verb_v2(xreq, verbsv2);
				return;
			}
		}
	}

	afb_xreq_fail_unknown_verb(xreq);
}

static int service_start_cb(void *closure, int share_session, int onneed, struct afb_apiset *apiset)
{
	struct afb_api_dyn *dynapi = closure;
	return afb_export_start(dynapi->export, share_session, onneed, apiset);
}

static void update_hooks_cb(void *closure)
{
	struct afb_api_dyn *dynapi = closure;
	afb_export_update_hook(dynapi->export);
}

static int get_verbosity_cb(void *closure)
{
	struct afb_api_dyn *dynapi = closure;
	return afb_export_verbosity_get(dynapi->export);
}

static void set_verbosity_cb(void *closure, int level)
{
	struct afb_api_dyn *dynapi = closure;
	afb_export_verbosity_set(dynapi->export, level);
}

static struct json_object *make_description_openAPIv3(struct afb_api_dyn *dynapi)
{
	char buffer[256];
	struct afb_api_dyn_verb **iter, **end, *verb;
	struct json_object *r, *f, *a, *i, *p, *g;

	r = json_object_new_object();
	json_object_object_add(r, "openapi", json_object_new_string("3.0.0"));

	i = json_object_new_object();
	json_object_object_add(r, "info", i);
	json_object_object_add(i, "title", json_object_new_string(afb_export_apiname(dynapi->export)));
	json_object_object_add(i, "version", json_object_new_string("0.0.0"));
	json_object_object_add(i, "description", json_object_new_string(dynapi->info));

	p = json_object_new_object();
	json_object_object_add(r, "paths", p);
	iter = dynapi->verbs;
	end = iter + dynapi->count;
	while (iter != end) {
		verb = *iter++;
		buffer[0] = '/';
		strncpy(buffer + 1, verb->verb, sizeof buffer - 1);
		buffer[sizeof buffer - 1] = 0;
		f = json_object_new_object();
		json_object_object_add(p, buffer, f);
		g = json_object_new_object();
		json_object_object_add(f, "get", g);

		a = afb_auth_json_v2(verb->auth, verb->session);
		if (a)
			json_object_object_add(g, "x-permissions", a);

		a = json_object_new_object();
		json_object_object_add(g, "responses", a);
		f = json_object_new_object();
		json_object_object_add(a, "200", f);
		json_object_object_add(f, "description", json_object_new_string(verb->info?:verb->verb));
	}
	return r;
}

static struct json_object *describe_cb(void *closure)
{
	struct afb_api_dyn *dynapi = closure;
	struct json_object *r = make_description_openAPIv3(dynapi);
	return r;
}

static struct afb_api_itf dyn_api_itf = {
	.call = call_cb,
	.service_start = service_start_cb,
	.update_hooks = update_hooks_cb,
	.get_verbosity = get_verbosity_cb,
	.set_verbosity = set_verbosity_cb,
	.describe = describe_cb
};

int afb_api_dyn_add(struct afb_apiset *apiset, const char *name, const char *info, int (*preinit)(void*, struct afb_dynapi*), void *closure)
{
	int rc;
	struct afb_api_dyn *dynapi;
	struct afb_api afb_api;
	struct afb_export *export;

	INFO("Starting creation of dynamic API %s", name);

	/* allocates the description */
	info = info ?: "";
	dynapi = calloc(1, sizeof *dynapi + strlen(info));
	export = afb_export_create_vdyn(apiset, name, dynapi);
	if (!dynapi || !export) {
		ERROR("out of memory");
		goto error;
	}
	strcpy(dynapi->info, info);
	dynapi->export = export;

	/* preinit the api */
	rc = afb_export_preinit_vdyn(export, preinit, closure);
	if (rc < 0) {
		ERROR("dynamic api %s preinit function failed, ABORTING it!",
				afb_export_apiname(dynapi->export));
		goto error;
	}

	/* records the binding */
	afb_api.closure = dynapi;
	afb_api.itf = &dyn_api_itf;
	afb_api.group = NULL;
	if (afb_apiset_add(apiset, afb_export_apiname(dynapi->export), afb_api) < 0) {
		ERROR("dynamic api %s can't be registered to set %s, ABORTING it!",
				afb_export_apiname(dynapi->export),
				afb_apiset_name(apiset));
		goto error;
	}
	INFO("binding %s added to set %s", afb_export_apiname(dynapi->export), afb_apiset_name(apiset));
	return 1;

error:
	afb_export_destroy(export);
	free(dynapi);

	return -1;
}

