/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
 * Author: José Bollo <jose.bollo@iot.bzh>
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

#pragma once

#include <stdarg.h>

/* declaration of features of libsystemd */
struct sd_event;
struct sd_bus;
struct afb_stored_req;
struct afb_req;

/*
 * Definition of the facilities provided by the daemon.
 */
struct afb_daemon_itf
{
	int (*event_broadcast)(void *closure, const char *name, struct json_object *object); /* broadcasts evant 'name' with 'object' */
	struct sd_event *(*get_event_loop)(void *closure);      /* gets the common systemd's event loop */
	struct sd_bus *(*get_user_bus)(void *closure);          /* gets the common systemd's user d-bus */
	struct sd_bus *(*get_system_bus)(void *closure);        /* gets the common systemd's system d-bus */
	void (*vverbose_v1)(void*closure, int level, const char *file, int line, const char *fmt, va_list args);
	struct afb_event (*event_make)(void *closure, const char *name); /* creates an event of 'name' */
	int (*rootdir_get_fd)(void *closure);
	int (*rootdir_open_locale)(void *closure, const char *filename, int flags, const char *locale);
	int (*queue_job)(void *closure, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout);
	void (*vverbose_v2)(void*closure, int level, const char *file, int line, const char * func, const char *fmt, va_list args);
	struct afb_req (*unstore_req)(void*closure, struct afb_stored_req *sreq);
	int (*require_api)(void*closure, const char *name, int initialized);
};

/*
 * Structure for accessing daemon.
 * See also: afb_daemon_get_event_sender, afb_daemon_get_event_loop, afb_daemon_get_user_bus, afb_daemon_get_system_bus
 */
struct afb_daemon
{
	const struct afb_daemon_itf *itf;       /* the interfacing functions */
	void *closure;                          /* the closure when calling these functions */
};

