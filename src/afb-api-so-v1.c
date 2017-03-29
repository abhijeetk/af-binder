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
#include "afb-thread.h"
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
	struct api_so_v1 *desc = closure;

	/* makes the event name */
	assert(desc->binding != NULL);
	length = strlen(name);
	event = alloca(length + 2 + desc->apilength);
	memcpy(event, desc->binding->v1.prefix, desc->apilength);
	event[desc->apilength] = '/';
	memcpy(event + desc->apilength + 1, name, length + 1);

	/* crate the event */
	return afb_evt_create_event(event);
}

static int afb_api_so_event_broadcast_cb(void *closure, const char *name, struct json_object *object)
{
	size_t length;
	char *event;
	struct api_so_v1 *desc = closure;

	/* makes the event name */
	assert(desc->binding != NULL);
	length = strlen(name);
	event = alloca(length + 2 + desc->apilength);
	memcpy(event, desc->binding->v1.prefix, desc->apilength);
	event[desc->apilength] = '/';
	memcpy(event + desc->apilength + 1, name, length + 1);

	return afb_evt_broadcast(event, object);
}

static void afb_api_so_vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args)
{
	char *p;
	struct api_so_v1 *desc = closure;

	if (vasprintf(&p, fmt, args) < 0)
		vverbose(level, file, line, fmt, args);
	else {
		verbose(level, file, line, "%s {binding %s}", p, desc->binding->v1.prefix);
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

static int call_check(struct afb_req req, struct afb_context *context, const struct afb_verb_desc_v1 *verb)
{
	int stag = (int)verb->session;

	if ((stag & (AFB_SESSION_CREATE|AFB_SESSION_CLOSE|AFB_SESSION_RENEW|AFB_SESSION_CHECK|AFB_SESSION_LOA_EQ)) != 0) {
		if (!afb_context_check(context)) {
			afb_context_close(context);
			afb_req_fail(req, "failed", "invalid token's identity");
			return 0;
		}
	}

	if ((stag & AFB_SESSION_CREATE) != 0) {
		if (afb_context_check_loa(context, 1)) {
			afb_req_fail(req, "failed", "invalid creation state");
			return 0;
		}
		afb_context_change_loa(context, 1);
		afb_context_refresh(context);
	}

	if ((stag & (AFB_SESSION_CREATE | AFB_SESSION_RENEW)) != 0)
		afb_context_refresh(context);

	if ((stag & AFB_SESSION_CLOSE) != 0) {
		afb_context_change_loa(context, 0);
		afb_context_close(context);
	}

	if ((stag & AFB_SESSION_LOA_GE) != 0) {
		int loa = (stag >> AFB_SESSION_LOA_SHIFT) & AFB_SESSION_LOA_MASK;
		if (!afb_context_check_loa(context, loa)) {
			afb_req_fail(req, "failed", "invalid LOA");
			return 0;
		}
	}

	if ((stag & AFB_SESSION_LOA_LE) != 0) {
		int loa = (stag >> AFB_SESSION_LOA_SHIFT) & AFB_SESSION_LOA_MASK;
		if (afb_context_check_loa(context, loa + 1)) {
			afb_req_fail(req, "failed", "invalid LOA");
			return 0;
		}
	}
	return 1;
}

static void call_cb(void *closure, struct afb_req req, struct afb_context *context, const char *strverb)
{
	const struct afb_verb_desc_v1 *verb;
	struct api_so_v1 *desc = closure;

	verb = desc->binding->v1.verbs;
	while (verb->name && strcasecmp(verb->name, strverb))
		verb++;
	if (!verb->name)
		afb_req_fail_f(req, "unknown-verb", "verb %s unknown within api %s", strverb, desc->binding->v1.prefix);
	else if (call_check(req, context, verb)) {
		afb_thread_req_call(req, verb->callback, afb_api_so_timeout, desc);
	}
}

static int service_start_cb(void *closure, int share_session, int onneed)
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
	desc->service = afb_svc_create(share_session, init, onevent);
	if (desc->service == NULL) {
		/* starting error */
		ERROR("Starting service %s failed", desc->binding->v1.prefix);
		return -1;
	}

	return 0;
}

int afb_api_so_v1_add(const char *path, void *handle)
{
	struct api_so_v1 *desc;
	struct afb_binding *(*register_function) (const struct afb_binding_interface *interface);
	struct afb_verb_desc_v1 fake_verb;
	struct afb_binding fake_binding;

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
	desc->interface.verbosity = verbosity;
	desc->interface.mode = AFB_MODE_LOCAL;
	desc->interface.daemon.itf = &daemon_itf;
	desc->interface.daemon.closure = desc;

	/* for log purpose, a fake binding is needed here */
	desc->binding = &fake_binding;
	fake_binding.type = AFB_BINDING_VERSION_1;
	fake_binding.v1.info = path;
	fake_binding.v1.prefix = path;
	fake_binding.v1.verbs = &fake_verb;
	fake_verb.name = NULL;

	/* init the binding */
	NOTICE("binding [%s] calling registering function %s", path, afb_api_so_v1_register);
	desc->binding = register_function(&desc->interface);
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
	if (!afb_apis_is_valid_api_name(desc->binding->v1.prefix)) {
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
	desc->apilength = strlen(desc->binding->v1.prefix);
	if (afb_apis_add(desc->binding->v1.prefix, (struct afb_api){
			.closure = desc,
			.call = call_cb,
			.service_start = service_start_cb }) < 0) {
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

