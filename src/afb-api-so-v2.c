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
#include <json-c/json.h>

#include "afb-api.h"
#include "afb-api-so-v2.h"
#include "afb-apiset.h"
#include "afb-svc.h"
#include "afb-ditf.h"
#include "afb-evt.h"
#include "afb-common.h"
#include "afb-context.h"
#include "afb-api-so.h"
#include "afb-xreq.h"
#include "jobs.h"
#include "verbose.h"

/*
 * names of symbols
 */
static const char afb_api_so_v2_descriptor[] = "afbBindingV2";
static const char afb_api_so_v2_verbosity[] = "afbBindingV2verbosity";

/*
 * Description of a binding
 */
struct api_so_v2 {
	const struct afb_binding_v2 *binding;	/* descriptor */
	int *verbosity;				/* verbosity */
	void *handle;			/* context of dlopen */
	struct afb_svc *service;	/* handler for service started */
	struct afb_ditf ditf;		/* daemon interface */
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

struct call_sync
{
	struct api_so_v2 *desc;
	struct afb_xreq *xreq;
};

static void call_sync_cb_cb(int signum, void *closure)
{
	struct call_sync *cs = closure;
	if (!signum)
		call_cb(cs->desc, cs->xreq);
	else  {
		if (!cs->xreq->replied)
			afb_xreq_fail(cs->xreq, "aborted", "internal error");
	}
}

static void call_sync_cb(void *closure, struct afb_xreq *xreq)
{
	struct call_sync cs = { .desc = closure, .xreq = xreq };

	if (jobs_call(closure, 0, call_sync_cb_cb, &cs))
		call_cb(closure, xreq);
}

static int service_start_cb(void *closure, int share_session, int onneed, struct afb_apiset *apiset)
{
	int (*start)(struct afb_service service);
	void (*onevent)(struct afb_service service, const char *event, struct json_object *object);

	struct api_so_v2 *desc = closure;

	/* check state */
	if (desc->service != NULL) {
		/* not an error when onneed */
		if (onneed != 0)
			return 0;

		/* already started: it is an error */
		ERROR("Service %s already started", desc->binding->api);
		return -1;
	}

	/* get the initialisation */
	start = desc->binding->start;
	if (start == NULL) {
		/* not an error when onneed */
		if (onneed != 0)
			return 0;

		/* no initialisation method */
		ERROR("Binding %s is not a service", desc->binding->api);
		return -1;
	}

	/* get the event handler if any */
	onevent = desc->binding->onevent;
	desc->service = afb_svc_create_v2(apiset, share_session, start, onevent, &desc->ditf);
	if (desc->service == NULL) {
		/* starting error */
		ERROR("Starting service %s failed", desc->binding->api);
		return -1;
	}

	return 0;
}

static void update_hooks_cb(void *closure)
{
	struct api_so_v2 *desc = closure;
	afb_ditf_update_hook(&desc->ditf);
}

static int get_verbosity_cb(void *closure)
{
	struct api_so_v2 *desc = closure;
	return *desc->verbosity;
}

static void set_verbosity_cb(void *closure, int level)
{
	struct api_so_v2 *desc = closure;
	*desc->verbosity = level;
}

static struct json_object *describe_cb(void *closure)
{
	struct api_so_v2 *desc = closure;
	return desc->binding->specification ? json_tokener_parse(desc->binding->specification) : NULL;
}

static struct afb_api_itf so_v2_api_itf = {
	.call = call_cb,
	.service_start = service_start_cb,
	.update_hooks = update_hooks_cb,
	.get_verbosity = get_verbosity_cb,
	.set_verbosity = set_verbosity_cb,
	.describe = describe_cb
};

static struct afb_api_itf so_v2_sync_api_itf = {
	.call = call_sync_cb,
	.service_start = service_start_cb,
	.update_hooks = update_hooks_cb,
	.get_verbosity = get_verbosity_cb,
	.set_verbosity = set_verbosity_cb,
	.describe = describe_cb
};

int afb_api_so_v2_add_binding(const struct afb_binding_v2 *binding, void *handle, struct afb_apiset *apiset, int *pver)
{
	int rc;
	struct api_so_v2 *desc;
	struct afb_api afb_api;

	/* basic checks */
	assert(binding->api);
	assert(binding->specification);
	assert(binding->verbs);

	/* allocates the description */
	desc = malloc(sizeof *desc);
	if (desc == NULL) {
		ERROR("out of memory");
		goto error;
	}
	desc->binding = binding;
	desc->verbosity = pver;
	desc->handle = handle;
	desc->service = NULL;
	memset(&desc->ditf, 0, sizeof desc->ditf);

	/* init the interface */
	afb_ditf_init_v2(&desc->ditf, binding->api);

	/* init the binding */
	if (binding->init) {
		INFO("binding %s calling init function", binding->api);
		rc = binding->init(desc->ditf.daemon);
		if (rc < 0) {
			ERROR("binding %s initialisation function failed...", binding->api);
			goto error2;
		}
	}

	/* records the binding */
	afb_api.closure = desc;
	afb_api.itf = binding->concurrent ? &so_v2_api_itf : &so_v2_sync_api_itf;
	if (afb_apiset_add(apiset, binding->api, afb_api) < 0) {
		ERROR("binding %s can't be registered to set %s...", binding->api, afb_apiset_name(apiset));
		goto error2;
	}
	INFO("binding %s added to set %s", binding->api, afb_apiset_name(apiset));
	return 1;

error2:
	free(desc);
error:
	return -1;
}

int afb_api_so_v2_add(const char *path, void *handle, struct afb_apiset *apiset)
{
	const struct afb_binding_v2 *binding;
	int *pver;

	/* retrieves the register function */
	binding = dlsym(handle, afb_api_so_v2_descriptor);
	pver = dlsym(handle, afb_api_so_v2_verbosity);
	if (!binding && !pver)
		return 0;

	INFO("binding [%s] looks like an AFB binding V2", path);

	/* basic checks */
	if (!binding || !pver) {
		ERROR("binding [%s] incomplete symbols...", path);
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
	if (binding->specification == NULL || *binding->specification == 0) {
		ERROR("binding [%s] bad specification...", path);
		goto error;
	}
	if (binding->verbs == NULL) {
		ERROR("binding [%s] no verbs...", path);
		goto error;
	}

	return afb_api_so_v2_add_binding(binding, handle, apiset, pver);

 error:
	return -1;
}

