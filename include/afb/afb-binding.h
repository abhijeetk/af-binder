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

/*****************************************************************************
 * This files is the main file to include for writing bindings dedicated to
 *
 *                      AFB-DAEMON
 *
 * Functions of bindings of afb-daemon are accessible by authorized clients
 * through the apis module of afb-daemon.
 *
 * A binding is a shared library. This shared library must have at least one
 * exported symbol for being registered in afb-daemon.
 *
 */

/*
 * Some function of the library are exported to afb-daemon.
 */

#include "afb-event-itf.h"
#include "afb-req-itf.h"
#include "afb-service-itf.h"
#include "afb-binding-v1.h"
#include "afb-binding-v2.h"

/*
 * config mode
 */
enum afb_mode {
	AFB_MODE_LOCAL = 0,     /* run locally */
	AFB_MODE_REMOTE,        /* run remotely */
	AFB_MODE_GLOBAL         /* run either remotely or locally (DONT USE! reserved for future) */
};

/* declaration of features of libsystemd */
struct sd_event;
struct sd_bus;

/*
 * Definition of the facilities provided by the daemon.
 */
struct afb_daemon_itf {
	int (*event_broadcast)(void *closure, const char *name, struct json_object *object); /* broadcasts evant 'name' with 'object' */
	struct sd_event *(*get_event_loop)(void *closure);      /* gets the common systemd's event loop */
	struct sd_bus *(*get_user_bus)(void *closure);          /* gets the common systemd's user d-bus */
	struct sd_bus *(*get_system_bus)(void *closure);        /* gets the common systemd's system d-bus */
	void (*vverbose)(void*closure, int level, const char *file, int line, const char *fmt, va_list args);
	struct afb_event (*event_make)(void *closure, const char *name); /* creates an event of 'name' */
	int (*rootdir_get_fd)(void *closure);
	int (*rootdir_open_locale)(void *closure, const char *filename, int flags, const char *locale);
	int (*queue_job)(void *closure, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout);
};

/*
 * Structure for accessing daemon.
 * See also: afb_daemon_get_event_sender, afb_daemon_get_event_loop, afb_daemon_get_user_bus, afb_daemon_get_system_bus
 */
struct afb_daemon {
	const struct afb_daemon_itf *itf;       /* the interfacing functions */
	void *closure;                          /* the closure when calling these functions */
};

/*
 * Interface between the daemon and the binding.
 */
struct afb_binding_interface
{
	struct afb_daemon daemon;       /* access to the daemon facilies */
	int verbosity;                  /* level of verbosity */
	enum afb_mode mode;             /* run mode (local or remote) */
};

/*
 * Retrieves the common systemd's event loop of AFB
 * 'daemon' MUST be the daemon given in interface when activating the binding.
 */
static inline struct sd_event *afb_daemon_get_event_loop(struct afb_daemon daemon)
{
	return daemon.itf->get_event_loop(daemon.closure);
}

/*
 * Retrieves the common systemd's user/session d-bus of AFB
 * 'daemon' MUST be the daemon given in interface when activating the binding.
 */
static inline struct sd_bus *afb_daemon_get_user_bus(struct afb_daemon daemon)
{
	return daemon.itf->get_user_bus(daemon.closure);
}

/*
 * Retrieves the common systemd's system d-bus of AFB
 * 'daemon' MUST be the daemon given in interface when activating the binding.
 */
static inline struct sd_bus *afb_daemon_get_system_bus(struct afb_daemon daemon)
{
	return daemon.itf->get_system_bus(daemon.closure);
}

/*
 * Broadcasts widely the event of 'name' with the data 'object'.
 * 'object' can be NULL.
 * 'daemon' MUST be the daemon given in interface when activating the binding.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
static inline int afb_daemon_broadcast_event(struct afb_daemon daemon, const char *name, struct json_object *object)
{
	return daemon.itf->event_broadcast(daemon.closure, name, object);
}

/*
 * Creates an event of 'name' and returns it.
 * 'daemon' MUST be the daemon given in interface when activating the binding.
 */
static inline struct afb_event afb_daemon_make_event(struct afb_daemon daemon, const char *name)
{
	return daemon.itf->event_make(daemon.closure, name);
}

/*
 * Send a message described by 'fmt' and following parameters
 * to the journal for the verbosity 'level'.
 * 'file' and 'line' are indicators of position of the code in source files.
 * 'daemon' MUST be the daemon given in interface when activating the binding.
 */
static inline void afb_daemon_verbose(struct afb_daemon daemon, int level, const char *file, int line, const char *fmt, ...) __attribute__((format(printf, 5, 6)));
static inline void afb_daemon_verbose(struct afb_daemon daemon, int level, const char *file, int line, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	daemon.itf->vverbose(daemon.closure, level, file, line, fmt, args);
	va_end(args);
}

/*
 * Macros for logging messages
 */
#if !defined(NO_BINDING_VERBOSE_MACRO)
# if !defined(NO_BINDING_FILE_LINE_INDICATION)
#  define ERROR(itf,...)   do{if(itf->verbosity>=0)afb_daemon_verbose(itf->daemon,3,__FILE__,__LINE__,__VA_ARGS__);}while(0)
#  define WARNING(itf,...) do{if(itf->verbosity>=1)afb_daemon_verbose(itf->daemon,4,__FILE__,__LINE__,__VA_ARGS__);}while(0)
#  define NOTICE(itf,...)  do{if(itf->verbosity>=1)afb_daemon_verbose(itf->daemon,5,__FILE__,__LINE__,__VA_ARGS__);}while(0)
#  define INFO(itf,...)    do{if(itf->verbosity>=2)afb_daemon_verbose(itf->daemon,6,__FILE__,__LINE__,__VA_ARGS__);}while(0)
#  define DEBUG(itf,...)   do{if(itf->verbosity>=3)afb_daemon_verbose(itf->daemon,7,__FILE__,__LINE__,__VA_ARGS__);}while(0)
# else
#  define ERROR(itf,...)   do{if(itf->verbosity>=0)afb_daemon_verbose(itf->daemon,3,NULL,0,__VA_ARGS__);}while(0)
#  define WARNING(itf,...) do{if(itf->verbosity>=1)afb_daemon_verbose(itf->daemon,4,NULL,0,__VA_ARGS__);}while(0)
#  define NOTICE(itf,...)  do{if(itf->verbosity>=1)afb_daemon_verbose(itf->daemon,5,NULL,0,__VA_ARGS__);}while(0)
#  define INFO(itf,...)    do{if(itf->verbosity>=2)afb_daemon_verbose(itf->daemon,6,NULL,0,__VA_ARGS__);}while(0)
#  define DEBUG(itf,...)   do{if(itf->verbosity>=3)afb_daemon_verbose(itf->daemon,7,NULL,0,__VA_ARGS__);}while(0)
# endif
#endif

/*
 * Get the root directory file descriptor. This file descriptor can
 * be used with functions 'openat', 'fstatat', ...
 */
static inline int afb_daemon_rootdir_get_fd(struct afb_daemon daemon)
{
	return daemon.itf->rootdir_get_fd(daemon.closure);
}

/*
 * Opens 'filename' within the root directory with 'flags' (see function openat)
 * using the 'locale' definition (example: "jp,en-US") that can be NULL.
 * Returns the file descriptor or -1 in case of error.
 */
static inline int afb_daemon_rootdir_open_locale(struct afb_daemon daemon, const char *filename, int flags, const char *locale)
{
	return daemon.itf->rootdir_open_locale(daemon.closure, filename, flags, locale);
}

/*
 * Queue the job defined by 'callback' and 'argument' for being executed asynchronously
 * in this thread (later) or in an other thread.
 * If 'group' is not NUL, the jobs queued with a same value (as the pointer value 'group')
 * are executed in sequence in the order of there submission.
 * If 'timeout' is not 0, it represent the maximum execution time for the job in seconds.
 * At first, the job is called with 0 as signum and the given argument.
 * The job is executed with the monitoring of its time and some signals like SIGSEGV and
 * SIGFPE. When a such signal is catched, the job is terminated and reexecuted but with
 * signum being the signal number (SIGALRM when timeout expired).
 *
 * Returns 0 in case of success or -1 in case of error.
 */
static inline int afb_daemon_queue_job(struct afb_daemon daemon, void (*callback)(int signum, void *arg), void *argument, void *group, int timeout)
{
	return daemon.itf->queue_job(daemon.closure, callback, argument, group, timeout);
}
