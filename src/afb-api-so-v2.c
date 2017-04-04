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
	size_t apilength;		/* length of the API name */
	void *handle;			/* context of dlopen */
	struct afb_svc *service;	/* handler for service started */
	struct afb_binding_interface interface;	/* interface for the binding */
};

static struct afb_event afb_api_so_event_make_cb(void *closure, const char *name);
static int afb_api_so_event_broadcast_cb(void *closure, const char *name, struct json_object *object);
static void afb_api_so_vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args);
static int afb_api_so_rootdir_get_fd(void *closure);
static int afb_api_so_rootdir_open_locale(void *closure, const char *filename, int flags, const char *locale);

static const struct afb_daemon_itf daemon_itf = {
	.event_broadcast = afb_api_so_event_broadcast_cb,
	.get_event_loop = afb_common_get_event_loop,
	.get_user_bus = afb_common_get_user_bus,
	.get_system_bus = afb_common_get_system_bus,
	.vverbose = afb_api_so_vverbose_cb,
	.event_make = afb_api_so_event_make_cb,
	.rootdir_get_fd = afb_api_so_rootdir_get_fd,
	.rootdir_open_locale = afb_api_so_rootdir_open_locale
};

static struct afb_event afb_api_so_event_make_cb(void *closure, const char *name)
{
	size_t length;
	char *event;
	struct api_so_v2 *desc = closure;

	/* makes the event name */
	assert(desc->binding != NULL);
	length = strlen(name);
	event = alloca(length + 2 + desc->apilength);
	memcpy(event, desc->binding->api, desc->apilength);
	event[desc->apilength] = '/';
	memcpy(event + desc->apilength + 1, name, length + 1);

	/* crate the event */
	return afb_evt_create_event(event);
}

static int afb_api_so_event_broadcast_cb(void *closure, const char *name, struct json_object *object)
{
	size_t length;
	char *event;
	struct api_so_v2 *desc = closure;

	/* makes the event name */
	assert(desc->binding != NULL);
	length = strlen(name);
	event = alloca(length + 2 + desc->apilength);
	memcpy(event, desc->binding->api, desc->apilength);
	event[desc->apilength] = '/';
	memcpy(event + desc->apilength + 1, name, length + 1);

	return afb_evt_broadcast(event, object);
}

static void afb_api_so_vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args)
{
	char *p;
	struct api_so_v2 *desc = closure;

	if (vasprintf(&p, fmt, args) < 0)
		vverbose(level, file, line, fmt, args);
	else {
		verbose(level, file, line, "%s {binding %s}", p, desc->binding->api);
		free(p);
	}
}

static int afb_api_so_rootdir_get_fd(void *closure)
{
	return afb_common_rootdir_get_fd();
}

static int afb_api_so_rootdir_open_locale(void *closure, const char *filename, int flags, const char *locale)
{
	return afb_common_rootdir_open_locale(filename, flags, locale);
}

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
	else {
		xreq->sessionflags = (int)verb->session;
		xreq->group = desc;
		xreq->callback = verb->callback;
		afb_xreq_call(xreq);
	}
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
	desc->service = afb_svc_create_v2(share_session, onevent, start, &desc->interface);
	if (desc->service == NULL) {
		/* starting error */
		ERROR("Starting service %s failed", desc->binding->api);
		return -1;
	}

	return 0;
}

int afb_api_so_v2_add(const char *path, void *handle)
{
	int rc;
	struct api_so_v2 *desc;
	struct afb_binding_v2 *binding;

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
	desc->interface.verbosity = verbosity;
	desc->interface.mode = AFB_MODE_LOCAL;
	desc->interface.daemon.itf = &daemon_itf;
	desc->interface.daemon.closure = desc;

	/* for log purpose, a fake binding is needed here */

	/* init the binding */
	if (binding->init) {
		NOTICE("binding %s [%s] calling init function", binding->api, path);
		rc = binding->init(&desc->interface);
		if (rc < 0) {
			ERROR("binding %s [%s] initialisation function failed...", binding->api, path);
			goto error2;
		}
	}

	/* records the binding */
	desc->apilength = strlen(binding->api);
	if (afb_apis_add(binding->api, (struct afb_api){
			.closure = desc,
			.call = call_cb,
			.service_start = service_start_cb }) < 0) {
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

