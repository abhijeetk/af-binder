/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
 * Author "Fulup Ar Foll"
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <systemd/sd-event.h>

#include "afb-config.h"
#include "afb-hswitch.h"
#include "afb-apis.h"
#include "afb-api-so.h"
#include "afb-api-dbus.h"
#include "afb-api-ws.h"
#include "afb-hsrv.h"
#include "afb-context.h"
#include "afb-hreq.h"
#include "afb-sig-handler.h"
#include "afb-thread.h"
#include "afb-session.h"
#include "verbose.h"
#include "afb-common.h"
#include "afb-hook.h"

#include <afb/afb-binding.h>

/*----------------------------------------------------------
 |   helpers for handling list of arguments
 +--------------------------------------------------------- */

/*
 * Calls the callback 'run' for each value of the 'list'
 * until the callback returns 0 or the end of the list is reached.
 * Returns either NULL if the end of the list is reached or a pointer
 * to the item whose value made 'run' return 0.
 * 'closure' is used for passing user data.
 */
static struct afb_config_list *run_for_list(struct afb_config_list *list,
					    int (*run) (void *closure, char *value),
					    void *closure)
{
	while (list && run(closure, list->value))
		list = list->next;
	return list;
}

static int run_start(void *closure, char *value)
{
	int (*starter) (const char *value) = closure;
	return starter(value) >= 0;
}

static void start_list(struct afb_config_list *list,
		       int (*starter) (const char *value), const char *message)
{
	list = run_for_list(list, run_start, starter);
	if (list) {
		ERROR("can't start %s %s", message, list->value);
		exit(1);
	}
}

/*----------------------------------------------------------
 | closeSession
 |   try to close everything before leaving
 +--------------------------------------------------------- */
static void closeSession(int status, void *data)
{
	/* struct afb_config *config = data; */
}

/*----------------------------------------------------------
 | daemonize
 |   set the process in background
 +--------------------------------------------------------- */
static void daemonize(struct afb_config *config)
{
	int consoleFD;
	int pid;

	// open /dev/console to redirect output messAFBes
	consoleFD = open(config->console, O_WRONLY | O_APPEND | O_CREAT, 0640);
	if (consoleFD < 0) {
		ERROR("AFB-daemon cannot open /dev/console (use --foreground)");
		exit(1);
	}
	// fork process when running background mode
	pid = fork();

	// if fail nothing much to do
	if (pid == -1) {
		ERROR("AFB-daemon Failed to fork son process");
		exit(1);
	}
	// if in father process, just leave
	if (pid != 0)
		_exit(0);

	// son process get all data in standalone mode
	NOTICE("background mode [pid:%d console:%s]", getpid(),
	       config->console);

	// redirect default I/O on console
	close(2);
	dup(consoleFD);		// redirect stderr
	close(1);
	dup(consoleFD);		// redirect stdout
	close(0);		// no need for stdin
	close(consoleFD);

#if 0
	setsid();		// allow father process to fully exit
	sleep(2);		// allow main to leave and release port
#endif
}

/*---------------------------------------------------------
 | http server
 |   Handles the HTTP server
 +--------------------------------------------------------- */
static int init_alias(void *closure, char *spec)
{
	struct afb_hsrv *hsrv = closure;
	char *path = strchr(spec, ':');

	if (path == NULL) {
		ERROR("Missing ':' in alias %s. Alias ignored", spec);
		return 1;
	}
	*path++ = 0;
	INFO("Alias for url=%s to path=%s", spec, path);
	return afb_hsrv_add_alias(hsrv, spec, afb_common_rootdir_get_fd(), path,
				  0, 0);
}

static int init_http_server(struct afb_hsrv *hsrv, struct afb_config *config)
{
	if (!afb_hsrv_add_handler
	    (hsrv, config->rootapi, afb_hswitch_websocket_switch, NULL, 20))
		return 0;

	if (!afb_hsrv_add_handler
	    (hsrv, config->rootapi, afb_hswitch_apis, NULL, 10))
		return 0;

	if (run_for_list(config->aliases, init_alias, hsrv))
		return 0;

	if (config->roothttp != NULL) {
		if (!afb_hsrv_add_alias
		    (hsrv, "", afb_common_rootdir_get_fd(), config->roothttp,
		     -10, 1))
			return 0;
	}

	if (!afb_hsrv_add_handler
	    (hsrv, config->rootbase, afb_hswitch_one_page_api_redirect, NULL,
	     -20))
		return 0;

	return 1;
}

static struct afb_hsrv *start_http_server(struct afb_config *config)
{
	int rc;
	struct afb_hsrv *hsrv;

	if (afb_hreq_init_download_path("/tmp")) {	/* TODO: sessiondir? */
		ERROR("unable to set the tmp directory");
		return NULL;
	}

	hsrv = afb_hsrv_create();
	if (hsrv == NULL) {
		ERROR("memory allocation failure");
		return NULL;
	}

	if (!afb_hsrv_set_cache_timeout(hsrv, config->cacheTimeout)
	    || !init_http_server(hsrv, config)) {
		ERROR("initialisation of httpd failed");
		afb_hsrv_put(hsrv);
		return NULL;
	}

	NOTICE("Waiting port=%d rootdir=%s", config->httpdPort,
	       config->rootdir);
	NOTICE("Browser URL= http:/*localhost:%d", config->httpdPort);

	rc = afb_hsrv_start(hsrv, (uint16_t) config->httpdPort, 15);
	if (!rc) {
		ERROR("starting of httpd failed");
		afb_hsrv_put(hsrv);
		return NULL;
	}

	return hsrv;
}

/*---------------------------------------------------------
 | main
 |   Parse option and launch action
 +--------------------------------------------------------- */

int main(int argc, char *argv[])
{
	struct afb_hsrv *hsrv;
	struct afb_config *config;
	struct sd_event *eventloop;

	LOGAUTH("afb-daemon");

	// ------------- Build session handler & init config -------
	config = afb_config_parse_arguments(argc, argv);
	on_exit(closeSession, config);

	// ------------------ sanity check ----------------------------------------
	if (config->httpdPort <= 0) {
		ERROR("no port is defined");
		exit(1);
	}

	afb_session_init(config->nbSessionMax, config->cntxTimeout,
			 config->token, afb_apis_count());

	afb_api_so_set_timeout(config->apiTimeout);
	if (config->ldpaths) {
		if (afb_api_so_add_pathset(config->ldpaths) < 0) {
			ERROR("initialisation of bindings within %s failed",
			      config->ldpaths);
			exit(1);
		}
	}

	start_list(config->dbus_clients, afb_api_dbus_add_client,
		   "the afb-dbus client");
	start_list(config->ws_clients, afb_api_ws_add_client,
		   "the afb-websocket client");
	start_list(config->so_bindings, afb_api_so_add_binding, "the binding");
	start_list(config->dbus_servers, afb_api_dbus_add_server,
		   "the afb-dbus service");
	start_list(config->ws_servers, afb_api_ws_add_server,
		   "the afb-websocket service");

	if (!afb_hreq_init_cookie
	    (config->httpdPort, config->rootapi, config->cntxTimeout)) {
		ERROR("initialisation of cookies failed");
		exit(1);
	}

	if (afb_sig_handler_init() < 0) {
		ERROR("failed to initialise signal handlers");
		return 1;
	}
	// if directory does not exist createit
	mkdir(config->rootdir, O_RDWR | S_IRWXU | S_IRGRP);
	if (afb_common_rootdir_set(config->rootdir) < 0) {
		ERROR("failed to set common root directory");
		return 1;
	}

	if (afb_thread_init(3, 1, 20) < 0) {
		ERROR("failed to initialise threading");
		return 1;
	}
	// let's run this program with a low priority
	nice(20);

	// ------------------ Finaly Process Commands -----------------------------
	// let's not take the risk to run as ROOT
	//if (getuid() == 0)  goto errorNoRoot;

	DEBUG("Init config done");

	// --------- run -----------
	if (config->background) {
		// --------- in background mode -----------
		INFO("entering background mode");
		daemonize(config);
	} else {
		// ---- in foreground mode --------------------
		INFO("entering foreground mode");
	}

	/* ignore any SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	/* install trace of requests */
	if (config->tracereq)
		afb_hook_req_create(NULL, NULL, NULL, config->tracereq, NULL, NULL);

	/* start the HTTP server */
	hsrv = start_http_server(config);
	if (hsrv == NULL)
		exit(1);

	/* start the services */
	if (afb_apis_start_all_services(1) < 0)
		exit(1);

	if (config->readyfd != 0) {
		static const char readystr[] = "READY=1";
		write(config->readyfd, readystr, sizeof(readystr) - 1);
		close(config->readyfd);
	}
	// infinite loop
	eventloop = afb_common_get_event_loop();
	for (;;)
		sd_event_run(eventloop, 30000000);

	WARNING("hoops returned from infinite loop [report bug]");

	return 0;
}
