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
#include <errno.h>

#include <json-c/json.h>

#include <afb/afb-binding-v1.h>
#include <afb/afb-binding-v2.h>

#include "afb-ditf.h"
#include "afb-evt.h"
#include "afb-common.h"
#include "afb-xreq.h"
#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-hook.h"
#include "jobs.h"
#include "verbose.h"

extern struct afb_apiset *main_apiset;

/**********************************************
* normal flow
**********************************************/
static void vverbose_cb(void *closure, int level, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	char *p;
	struct afb_ditf *ditf = closure;

	if (vasprintf(&p, fmt, args) < 0)
		vverbose(level, file, line, function, fmt, args);
	else {
		verbose(level, file, line, function, "[API %s] %s", ditf->api, p);
		free(p);
	}
}

static void old_vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args)
{
	vverbose_cb(closure, level, file, line, "?", fmt, args);
}

static struct afb_event event_make_cb(void *closure, const char *name)
{
	size_t plen, nlen;
	char *event;
	struct afb_ditf *ditf = closure;

	/* makes the event name */
	plen = strlen(ditf->api);
	nlen = strlen(name);
	event = alloca(nlen + plen + 2);
	memcpy(event, ditf->api, plen);
	event[plen] = '/';
	memcpy(event + plen + 1, name, nlen + 1);

	/* create the event */
	return afb_evt_create_event(event);
}

static int event_broadcast_cb(void *closure, const char *name, struct json_object *object)
{
	size_t plen, nlen;
	char *event;
	struct afb_ditf *ditf = closure;

	/* makes the event name */
	plen = strlen(ditf->api);
	nlen = strlen(name);
	event = alloca(nlen + plen + 2);
	memcpy(event, ditf->api, plen);
	event[plen] = '/';
	memcpy(event + plen + 1, name, nlen + 1);

	/* broadcast the event */
	return afb_evt_broadcast(event, object);
}

static int rootdir_open_locale_cb(void *closure, const char *filename, int flags, const char *locale)
{
	return afb_common_rootdir_open_locale(filename, flags, locale);
}

static int queue_job_cb(void *closure, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout)
{
	return jobs_queue(group, timeout, callback, argument);
}

static struct afb_req unstore_req_cb(void *closure, struct afb_stored_req *sreq)
{
	return afb_xreq_unstore(sreq);
}

static int require_api_cb(void *closure, const char *name, int initialized)
{
	return -!(initialized ? afb_apiset_lookup_started : afb_apiset_lookup)(main_apiset, name, 1);
}

/**********************************************
* hooked flow
**********************************************/
static void hooked_vverbose_cb(void *closure, int level, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	struct afb_ditf *ditf = closure;
	va_list ap;
	va_copy(ap, args);
	vverbose_cb(closure, level, file, line, function, fmt, args);
	afb_hook_ditf_vverbose(ditf, level, file, line, function, fmt, ap);
	va_end(ap);
}

static void hooked_old_vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args)
{
	hooked_vverbose_cb(closure, level, file, line, "?", fmt, args);
}

static struct afb_event hooked_event_make_cb(void *closure, const char *name)
{
	struct afb_ditf *ditf = closure;
	struct afb_event r = event_make_cb(closure, name);
	return afb_hook_ditf_event_make(ditf, name, r);
}

static int hooked_event_broadcast_cb(void *closure, const char *name, struct json_object *object)
{
	int r;
	struct afb_ditf *ditf = closure;
	json_object_get(object);
	afb_hook_ditf_event_broadcast_before(ditf, name, json_object_get(object));
	r = event_broadcast_cb(closure, name, object);
	afb_hook_ditf_event_broadcast_after(ditf, name, object, r);
	json_object_put(object);
	return r;
}

static struct sd_event *hooked_get_event_loop(void *closure)
{
	struct afb_ditf *ditf = closure;
	struct sd_event *r = afb_common_get_event_loop();
	return afb_hook_ditf_get_event_loop(ditf, r);
}

static struct sd_bus *hooked_get_user_bus(void *closure)
{
	struct afb_ditf *ditf = closure;
	struct sd_bus *r = afb_common_get_user_bus();
	return afb_hook_ditf_get_user_bus(ditf, r);
}

static struct sd_bus *hooked_get_system_bus(void *closure)
{
	struct afb_ditf *ditf = closure;
	struct sd_bus *r = afb_common_get_system_bus();
	return afb_hook_ditf_get_system_bus(ditf, r);
}

static int hooked_rootdir_get_fd(void *closure)
{
	struct afb_ditf *ditf = closure;
	int r = afb_common_rootdir_get_fd();
	return afb_hook_ditf_rootdir_get_fd(ditf, r);
}

static int hooked_rootdir_open_locale_cb(void *closure, const char *filename, int flags, const char *locale)
{
	struct afb_ditf *ditf = closure;
	int r = rootdir_open_locale_cb(closure, filename, flags, locale);
	return afb_hook_ditf_rootdir_open_locale(ditf, filename, flags, locale, r);
}

static int hooked_queue_job_cb(void *closure, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout)
{
	struct afb_ditf *ditf = closure;
	int r = queue_job_cb(closure, callback, argument, group, timeout);
	return afb_hook_ditf_queue_job(ditf, callback, argument, group, timeout, r);
}

static struct afb_req hooked_unstore_req_cb(void *closure, struct afb_stored_req *sreq)
{
	struct afb_ditf *ditf = closure;
	afb_hook_ditf_unstore_req(ditf, sreq);
	return unstore_req_cb(closure, sreq);
}

static int hooked_require_api_cb(void *closure, const char *name, int initialized)
{
	int result;
	struct afb_ditf *ditf = closure;
	afb_hook_ditf_require_api(ditf, name, initialized);
	result = require_api_cb(closure, name, initialized);
	return afb_hook_ditf_require_api_result(ditf, name, initialized, result);
}

/**********************************************
* vectors
**********************************************/
static const struct afb_daemon_itf daemon_itf = {
	.vverbose_v1 = old_vverbose_cb,
	.vverbose_v2 = vverbose_cb,
	.event_make = event_make_cb,
	.event_broadcast = event_broadcast_cb,
	.get_event_loop = afb_common_get_event_loop,
	.get_user_bus = afb_common_get_user_bus,
	.get_system_bus = afb_common_get_system_bus,
	.rootdir_get_fd = afb_common_rootdir_get_fd,
	.rootdir_open_locale = rootdir_open_locale_cb,
	.queue_job = queue_job_cb,
	.unstore_req = unstore_req_cb,
	.require_api = require_api_cb
};

static const struct afb_daemon_itf hooked_daemon_itf = {
	.vverbose_v1 = hooked_old_vverbose_cb,
	.vverbose_v2 = hooked_vverbose_cb,
	.event_make = hooked_event_make_cb,
	.event_broadcast = hooked_event_broadcast_cb,
	.get_event_loop = hooked_get_event_loop,
	.get_user_bus = hooked_get_user_bus,
	.get_system_bus = hooked_get_system_bus,
	.rootdir_get_fd = hooked_rootdir_get_fd,
	.rootdir_open_locale = hooked_rootdir_open_locale_cb,
	.queue_job = hooked_queue_job_cb,
	.unstore_req = hooked_unstore_req_cb,
	.require_api = hooked_require_api_cb
};

void afb_ditf_init_v2(struct afb_ditf *ditf, const char *api, struct afb_binding_data_v2 *data)
{
	ditf->version = 2;
	ditf->v2 = data;
	data->daemon.closure = ditf;
	afb_ditf_rename(ditf, api);
}

void afb_ditf_init_v1(struct afb_ditf *ditf, const char *api, struct afb_binding_interface_v1 *itf)
{
	ditf->version = 1;
	ditf->v1 = itf;
	itf->verbosity = verbosity;
	itf->mode = AFB_MODE_LOCAL;
	itf->daemon.closure = ditf;
	afb_ditf_rename(ditf, api);
}

void afb_ditf_rename(struct afb_ditf *ditf, const char *api)
{
	ditf->api = api;
	afb_ditf_update_hook(ditf);
}

void afb_ditf_update_hook(struct afb_ditf *ditf)
{
	int hooked = !!afb_hook_flags_ditf(ditf->api);
	switch (ditf->version) {
	case 1:
		ditf->v1->daemon.itf = hooked ? &hooked_daemon_itf : &daemon_itf;
		break;
	default:
	case 2:
		ditf->v2->daemon.itf = hooked ? &hooked_daemon_itf : &daemon_itf;
		break;
	}
}

