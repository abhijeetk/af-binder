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
#include <errno.h>

#include <afb/afb-binding.h>

#include "afb-ditf.h"
#include "afb-evt.h"
#include "afb-common.h"
#include "verbose.h"


static struct afb_event event_make_cb(void *closure, const char *name);
static int event_broadcast_cb(void *closure, const char *name, struct json_object *object);
static void vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args);
static int rootdir_open_locale_cb(void *closure, const char *filename, int flags, const char *locale);

static const struct afb_daemon_itf daemon_itf = {
	.vverbose = vverbose_cb,
	.event_make = event_make_cb,
	.event_broadcast = event_broadcast_cb,
	.get_event_loop = afb_common_get_event_loop,
	.get_user_bus = afb_common_get_user_bus,
	.get_system_bus = afb_common_get_system_bus,
	.rootdir_get_fd = afb_common_rootdir_get_fd,
	.rootdir_open_locale = rootdir_open_locale_cb
};

static void vverbose_cb(void *closure, int level, const char *file, int line, const char *fmt, va_list args)
{
	char *p;
	struct afb_ditf *ditf = closure;

	if (vasprintf(&p, fmt, args) < 0)
		vverbose(level, file, line, fmt, args);
	else {
		verbose(level, file, line, "%s {binding %s}", p, ditf->prefix);
		free(p);
	}
}

static struct afb_event event_make_cb(void *closure, const char *name)
{
	size_t plen, nlen;
	char *event;
	struct afb_ditf *ditf = closure;

	/* makes the event name */
	plen = strlen(ditf->prefix);
	nlen = strlen(name);
	event = alloca(nlen + plen + 2);
	memcpy(event, ditf->prefix, plen);
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
	plen = strlen(ditf->prefix);
	nlen = strlen(name);
	event = alloca(nlen + plen + 2);
	memcpy(event, ditf->prefix, plen);
	event[plen] = '/';
	memcpy(event + plen + 1, name, nlen + 1);

	/* broadcast the event */
	return afb_evt_broadcast(event, object);
}

static int rootdir_open_locale_cb(void *closure, const char *filename, int flags, const char *locale)
{
	return afb_common_rootdir_open_locale(filename, flags, locale);
}

void afb_ditf_init(struct afb_ditf *ditf, const char *prefix)
{
	ditf->interface.verbosity = verbosity;
	ditf->interface.mode = AFB_MODE_LOCAL;
	ditf->interface.daemon.itf = &daemon_itf;
	ditf->interface.daemon.closure = ditf;
	afb_ditf_rename(ditf, prefix);
}

void afb_ditf_rename(struct afb_ditf *ditf, const char *prefix)
{
	ditf->prefix = prefix;
}

