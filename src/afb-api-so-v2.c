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
#define NO_BINDING_VERBOSE_MACRO

#include <string.h>
#include <dlfcn.h>
#include <assert.h>

#include <afb/afb-binding.h>

#include "afb-apis.h"
#include "afb-svc.h"
#include "afb-ditf.h"
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

/*
 * Description of a binding
 */
struct api_so_v2 {
	struct afb_binding_v2 *binding;	/* descriptor */
	void *handle;			/* context of dlopen */
	struct afb_svc *service;	/* handler for service started */
	struct afb_ditf ditf;		/* daemon interface */
};

static const struct afb_verb_v2 *search(struct api_so_v2 *desc, const char *verb)
{
	const struct afb_verb_v2 *result;

	result = desc->binding->verbs;
	while (result->verb && strcasecmp(result->verb, verb))
		result++;
	return result->verb ? result : NULL;
}

static void call_cb(void *closure, struct afb_xreq *xreq)
{
	struct api_so_v2 *desc = closure;
	const struct afb_verb_v2 *verb;

	verb = search(desc, xreq->verb);
	if (!verb)
		afb_xreq_fail_f(xreq, "unknown-verb", "verb %s unknown within api %s", xreq->verb, desc->binding->api);
	else
		afb_xreq_call(xreq, verb->session, verb->callback);
}

static int service_start_cb(void *closure, int share_session, int onneed)
{
	int (*start)(const struct afb_binding_interface *interface, struct afb_service service);
	void (*onevent)(const char *event, struct json_object *object);

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
	desc->service = afb_svc_create_v2(share_session, onevent, start, &desc->ditf.interface);
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
	return desc->ditf.interface.verbosity;
}

static void set_verbosity_cb(void *closure, int level)
{
	struct api_so_v2 *desc = closure;
	desc->ditf.interface.verbosity = level;
}

static struct afb_api_itf so_v2_api_itf = {
	.call = call_cb,
	.service_start = service_start_cb,
	.update_hooks = update_hooks_cb,
	.get_verbosity = get_verbosity_cb,
	.set_verbosity = set_verbosity_cb
};

int afb_api_so_v2_add(const char *path, void *handle)
{
	int rc;
	struct api_so_v2 *desc;
	struct afb_binding_v2 *binding;
	struct afb_api afb_api;

	/* retrieves the register function */
	binding = dlsym(handle, afb_api_so_v2_descriptor);
	if (!binding)
		return 0;

	INFO("binding [%s] looks like an AFB binding V2", path);

	/* basic checks */
	if (binding->api == NULL || *binding->api == 0) {
		ERROR("binding [%s] bad api name...", path);
		goto error;
	}
	if (!afb_apis_is_valid_api_name(binding->api)) {
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

	/* allocates the description */
	desc = calloc(1, sizeof *desc);
	if (desc == NULL) {
		ERROR("out of memory");
		goto error;
	}
	desc->binding = binding;
	desc->handle = handle;

	/* init the interface */
	afb_ditf_init(&desc->ditf, binding->api);

	/* for log purpose, a fake binding is needed here */

	/* init the binding */
	if (binding->init) {
		NOTICE("binding %s [%s] calling init function", binding->api, path);
		rc = binding->init(&desc->ditf.interface);
		if (rc < 0) {
			ERROR("binding %s [%s] initialisation function failed...", binding->api, path);
			goto error2;
		}
	}

	/* records the binding */
	afb_api.closure = desc;
	afb_api.itf = &so_v2_api_itf;
	if (afb_apis_add(binding->api, afb_api) < 0) {
		ERROR("binding [%s] can't be registered...", path);
		goto error2;
	}
	NOTICE("binding %s loaded with API prefix %s", path, binding->api);
	return 1;

error2:
	free(desc);
error:
	return -1;
}

