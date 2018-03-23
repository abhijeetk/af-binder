/*
 * Copyright (C) 2015-2018 "IoT.bzh"
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#if !defined(NO_CALL_PERSONALITY)
#include <sys/personality.h>
#endif

#include <json-c/json.h>

#include <systemd/sd-daemon.h>

#include "afb-config.h"
#include "afb-hswitch.h"
#include "afb-apiset.h"
#include "afb-api-so.h"
#if defined(WITH_DBUS_TRANSPARENCY)
#   include "afb-api-dbus.h"
#endif
#include "afb-api-ws.h"
#include "afb-hsrv.h"
#include "afb-hreq.h"
#include "afb-xreq.h"
#include "jobs.h"
#include "afb-session.h"
#include "verbose.h"
#include "afb-common.h"
#include "afb-monitor.h"
#include "afb-hook.h"
#include "afb-debug.h"
#include "process-name.h"
#include "afb-supervision.h"

/*
   if SELF_PGROUP == 0 the launched command is the group leader
   if SELF_PGROUP != 0 afb-daemon is the group leader
*/
#define SELF_PGROUP 1

struct afb_apiset *main_apiset;
struct afb_config *main_config;

static pid_t childpid;

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
	int (*starter) (const char *value, struct afb_apiset *apiset) = closure;
	return starter(value, main_apiset) >= 0;
}

static void apiset_start_list(struct afb_config_list *list,
		       int (*starter) (const char *value, struct afb_apiset *apiset), const char *message)
{
	list = run_for_list(list, run_start, starter);
	if (list) {
		ERROR("can't start %s %s", message, list->value);
		exit(1);
	}
}

/*----------------------------------------------------------
 | exit_handler
 |   Handles on exit specific actions
 +--------------------------------------------------------- */
static void exit_handler()
{
	struct sigaction siga;

	memset(&siga, 0, sizeof siga);
	siga.sa_handler = SIG_IGN;
	sigaction(SIGTERM, &siga, NULL);

	if (SELF_PGROUP)
		killpg(0, SIGTERM);
	else if (childpid > 0)
		killpg(childpid, SIGTERM);
}

static void on_sigterm(int signum, siginfo_t *info, void *uctx)
{
	NOTICE("Received SIGTERM");
	exit(0);
}

static void on_sighup(int signum, siginfo_t *info, void *uctx)
{
	NOTICE("Received SIGHUP");
	/* TODO */
}

static void setup_daemon()
{
	struct sigaction siga;

	/* install signal handlers */
	memset(&siga, 0, sizeof siga);
	siga.sa_flags = SA_SIGINFO;

	siga.sa_sigaction = on_sigterm;
	sigaction(SIGTERM, &siga, NULL);

	siga.sa_sigaction = on_sighup;
	sigaction(SIGHUP, &siga, NULL);

	/* handle groups */
	atexit(exit_handler);

	/* ignore any SIGPIPE */
	signal(SIGPIPE, SIG_IGN);
}

/*----------------------------------------------------------
 | daemonize
 |   set the process in background
 +--------------------------------------------------------- */
static void daemonize()
{
	int consoleFD;
	int pid;

	// open /dev/console to redirect output messAFBes
	consoleFD = open(main_config->console, O_WRONLY | O_APPEND | O_CREAT, 0640);
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
	       main_config->console);

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

static int init_http_server(struct afb_hsrv *hsrv)
{
	if (!afb_hsrv_add_handler
	    (hsrv, main_config->rootapi, afb_hswitch_websocket_switch, main_apiset, 20))
		return 0;

	if (!afb_hsrv_add_handler
	    (hsrv, main_config->rootapi, afb_hswitch_apis, main_apiset, 10))
		return 0;

	if (run_for_list(main_config->aliases, init_alias, hsrv))
		return 0;

	if (main_config->roothttp != NULL) {
		if (!afb_hsrv_add_alias
		    (hsrv, "", afb_common_rootdir_get_fd(), main_config->roothttp,
		     -10, 1))
			return 0;
	}

	if (!afb_hsrv_add_handler
	    (hsrv, main_config->rootbase, afb_hswitch_one_page_api_redirect, NULL,
	     -20))
		return 0;

	return 1;
}

static struct afb_hsrv *start_http_server()
{
	int rc;
	struct afb_hsrv *hsrv;

	if (afb_hreq_init_download_path(main_config->uploaddir)) {
		ERROR("unable to set the upload directory %s", main_config->uploaddir);
		return NULL;
	}

	hsrv = afb_hsrv_create();
	if (hsrv == NULL) {
		ERROR("memory allocation failure");
		return NULL;
	}

	if (!afb_hsrv_set_cache_timeout(hsrv, main_config->cacheTimeout)
	    || !init_http_server(hsrv)) {
		ERROR("initialisation of httpd failed");
		afb_hsrv_put(hsrv);
		return NULL;
	}

	NOTICE("Waiting port=%d rootdir=%s", main_config->httpdPort, main_config->rootdir);
	NOTICE("Browser URL= http://localhost:%d", main_config->httpdPort);

	rc = afb_hsrv_start(hsrv, (uint16_t) main_config->httpdPort, 15);
	if (!rc) {
		ERROR("starting of httpd failed");
		afb_hsrv_put(hsrv);
		return NULL;
	}

	return hsrv;
}

/*---------------------------------------------------------
 | execute_command
 +--------------------------------------------------------- */

static void on_sigchld(int signum, siginfo_t *info, void *uctx)
{
	if (info->si_pid == childpid) {
		switch (info->si_code) {
		case CLD_EXITED:
		case CLD_KILLED:
		case CLD_DUMPED:
			childpid = 0;
			if (!SELF_PGROUP)
				killpg(info->si_pid, SIGKILL);
			waitpid(info->si_pid, NULL, 0);
			exit(0);
		}
	}
}

/*
# @@ @
# @p port
# @t token
*/

#define SUBST_CHAR  '@'
#define SUBST_STR   "@"

static char *instanciate_string(char *arg, const char *port, const char *token)
{
	char *resu, *it, *wr;
	int chg, dif;

	/* get the changes */
	chg = 0;
	dif = 0;
	it = strchrnul(arg, SUBST_CHAR);
	while (*it) {
		switch(*++it) {
		case 'p': chg++; dif += (int)strlen(port) - 2; break;
		case 't': chg++; dif += (int)strlen(token) - 2; break;
		case SUBST_CHAR: it++; chg++; dif--; break;
		default: break;
		}
		it = strchrnul(it, SUBST_CHAR);
	}

	/* return arg when no change */
	if (!chg)
		return arg;

	/* allocates the result */
	resu = malloc((it - arg) + dif + 1);
	if (!resu) {
		ERROR("out of memory");
		return NULL;
	}

	/* instanciate the arguments */
	wr = resu;
	for (;;) {
		it = strchrnul(arg, SUBST_CHAR);
		wr = mempcpy(wr, arg, it - arg);
		if (!*it)
			break;
		switch(*++it) {
		case 'p': wr = stpcpy(wr, port); break;
		case 't': wr = stpcpy(wr, token); break;
		default: *wr++ = SUBST_CHAR; /*@fallthrough@*/
		case SUBST_CHAR: *wr++ = *it;
		}
		arg = ++it;
	}

	*wr = 0;
	return resu;
}

static int instanciate_environ(const char *port, const char *token)
{
	extern char **environ;
	char *repl;
	int i;

	/* instanciate the environment */
	for (i = 0 ; environ[i] ; i++) {
		repl = instanciate_string(environ[i], port, token);
		if (!repl)
			return -1;
		environ[i] = repl;
	}
	return 0;
}

static int instanciate_command_args(const char *port, const char *token)
{
	char *repl;
	int i;

	/* instanciate the arguments */
	for (i = 0 ; main_config->exec[i] ; i++) {
		repl = instanciate_string(main_config->exec[i], port, token);
		if (!repl)
			return -1;
		main_config->exec[i] = repl;
	}
	return 0;
}

static int execute_command()
{
	struct sigaction siga;
	char port[20];
	const char *token;
	int rc;

	/* check whether a command is to execute or not */
	if (!main_config->exec || !main_config->exec[0])
		return 0;

	if (SELF_PGROUP)
		setpgid(0, 0);

	/* install signal handler */
	memset(&siga, 0, sizeof siga);
	siga.sa_sigaction = on_sigchld;
	siga.sa_flags = SA_SIGINFO;
	sigaction(SIGCHLD, &siga, NULL);

	/* fork now */
	childpid = fork();
	if (childpid)
		return 0;

	/* compute the string for port */
	if (main_config->httpdPort)
		rc = snprintf(port, sizeof port, "%d", main_config->httpdPort);
	else
		rc = snprintf(port, sizeof port, "%cp", SUBST_CHAR);
	if (rc < 0 || rc >= (int)(sizeof port)) {
		ERROR("port->txt failed");
	}
	else {
		/* instanciate arguments and environment */
		token = afb_session_initial_token();
		if (instanciate_command_args(port, token) >= 0
		 && instanciate_environ(port, token) >= 0) {
			/* run */
			if (!SELF_PGROUP)
				setpgid(0, 0);
			execv(main_config->exec[0], main_config->exec);
			ERROR("can't launch %s: %m", main_config->exec[0]);
		}
	}
	exit(1);
	return -1;
}

/*---------------------------------------------------------
 | startup calls
 +--------------------------------------------------------- */

struct startup_req
{
	struct afb_xreq xreq;
	char *api;
	char *verb;
	struct afb_config_list *current;
	struct afb_session *session;
};

static void startup_call_reply(struct afb_xreq *xreq, int status, struct json_object *obj)
{
	struct startup_req *sreq = CONTAINER_OF_XREQ(struct startup_req, xreq);

	if (status >= 0)
		NOTICE("startup call %s returned %s", sreq->current->value, json_object_get_string(obj));
	else {
		ERROR("startup call %s ERROR! %s", sreq->current->value, json_object_get_string(obj));
		exit(1);
	}
}

static void startup_call_current(struct startup_req *sreq);

static void startup_call_unref(struct afb_xreq *xreq)
{
	struct startup_req *sreq = CONTAINER_OF_XREQ(struct startup_req, xreq);

	free(sreq->api);
	free(sreq->verb);
	json_object_put(sreq->xreq.json);
	sreq->current = sreq->current->next;
	if (sreq->current)
		startup_call_current(sreq);
	else {
		afb_session_close(sreq->session);
		afb_session_unref(sreq->session);
		free(sreq);
	}
}

static struct afb_xreq_query_itf startup_xreq_itf =
{
	.reply = startup_call_reply,
	.unref = startup_call_unref
};

static void startup_call_current(struct startup_req *sreq)
{
	char *api, *verb, *json;

	api = sreq->current->value;
	verb = strchr(api, '/');
	if (verb) {
		json = strchr(verb, ':');
		if (json) {
			afb_xreq_init(&sreq->xreq, &startup_xreq_itf);
			afb_context_init(&sreq->xreq.context, sreq->session, NULL);
			sreq->xreq.context.validated = 1;
			sreq->api = strndup(api, verb - api);
			sreq->verb = strndup(verb + 1, json - verb - 1);
			sreq->xreq.request.api = sreq->api;
			sreq->xreq.request.verb = sreq->verb;
			sreq->xreq.json = json_tokener_parse(json + 1);
			if (sreq->api && sreq->verb && sreq->xreq.json) {
				afb_xreq_process(&sreq->xreq, main_apiset);
				return;
			}
		}
	}
	ERROR("Bad call specification %s", sreq->current->value);
	exit(1);
}

static void run_startup_calls()
{
	struct afb_config_list *list;
	struct startup_req *sreq;

	list = main_config->calls;
	if (list) {
		sreq = calloc(1, sizeof *sreq);
		sreq->session = afb_session_create(3600);
		sreq->current = list;
		startup_call_current(sreq);
	}
}

/*---------------------------------------------------------
 | job for starting the daemon
 +--------------------------------------------------------- */

static void start(int signum, void *arg)
{
	struct afb_hsrv *hsrv;

	afb_debug("start-entry");

	if (signum) {
		ERROR("start aborted: received signal %s", strsignal(signum));
		exit(1);
	}

	/* set the directories */
	mkdir(main_config->workdir, S_IRWXU | S_IRGRP | S_IXGRP);
	if (chdir(main_config->workdir) < 0) {
		ERROR("Can't enter working dir %s", main_config->workdir);
		goto error;
	}
	if (afb_common_rootdir_set(main_config->rootdir) < 0) {
		ERROR("failed to set common root directory");
		goto error;
	}

	/* configure the daemon */
	if (afb_session_init(main_config->nbSessionMax, main_config->cntxTimeout, main_config->token)) {
		ERROR("initialisation of session manager failed");
		goto error;
	}
	main_apiset = afb_apiset_create("main", main_config->apiTimeout);
	if (!main_apiset) {
		ERROR("can't create main api set");
		goto error;
	}
	if (afb_monitor_init() < 0) {
		ERROR("failed to setup monitor");
		goto error;
	}
	if (afb_supervision_init() < 0) {
		ERROR("failed to setup supervision");
		goto error;
	}

	/* install hooks */
	if (main_config->tracereq)
		afb_hook_create_xreq(NULL, NULL, NULL, main_config->tracereq, NULL, NULL);
	if (main_config->traceditf)
		afb_hook_create_ditf(NULL, main_config->traceditf, NULL, NULL);
	if (main_config->tracesvc)
		afb_hook_create_svc(NULL, main_config->tracesvc, NULL, NULL);
	if (main_config->traceevt)
		afb_hook_create_evt(NULL, main_config->traceevt, NULL, NULL);
	if (main_config->traceses)
		afb_hook_create_session(NULL, main_config->traceses, NULL, NULL);

	/* load bindings */
	afb_debug("start-load");
	apiset_start_list(main_config->so_bindings, afb_api_so_add_binding, "the binding");
#if defined(WITH_DBUS_TRANSPARENCY)
	apiset_start_list(main_config->dbus_clients, afb_api_dbus_add_client, "the afb-dbus client");
#endif
	apiset_start_list(main_config->ws_clients, afb_api_ws_add_client_weak, "the afb-websocket client");
	apiset_start_list(main_config->ldpaths, afb_api_so_add_pathset_fails, "the binding path set");
	apiset_start_list(main_config->weak_ldpaths, afb_api_so_add_pathset_nofails, "the weak binding path set");

#if defined(WITH_DBUS_TRANSPARENCY)
	apiset_start_list(main_config->dbus_servers, afb_api_dbus_add_server, "the afb-dbus service");
#endif
	apiset_start_list(main_config->ws_servers, afb_api_ws_add_server, "the afb-websocket service");

	DEBUG("Init config done");

	/* start the services */
	afb_debug("start-start");
#if !defined(NO_CALL_PERSONALITY)
	personality((unsigned long)-1L);
#endif
	if (afb_apiset_start_all_services(main_apiset, 1) < 0)
		goto error;

	/* start the HTTP server */
	afb_debug("start-http");
	if (!main_config->noHttpd) {
		if (main_config->httpdPort <= 0) {
			ERROR("no port is defined");
			goto error;
		}

		if (!afb_hreq_init_cookie(main_config->httpdPort, main_config->rootapi, main_config->cntxTimeout)) {
			ERROR("initialisation of HTTP cookies failed");
			goto error;
		}

		hsrv = start_http_server();
		if (hsrv == NULL)
			goto error;
	}

	/* run the startup calls */
	afb_debug("start-call");
	run_startup_calls();

	/* run the command */
	afb_debug("start-exec");
	if (execute_command() < 0)
		goto error;

	/* ready */
	sd_notify(1, "READY=1");
	return;
error:
	exit(1);
}

/*---------------------------------------------------------
 | main
 |   Parse option and launch action
 +--------------------------------------------------------- */

int main(int argc, char *argv[])
{
	afb_debug("main-entry");

	// let's run this program with a low priority
	nice(20);

	// ------------- Build session handler & init config -------
	main_config = afb_config_parse_arguments(argc, argv);
	if (main_config->name) {
		verbose_set_name(main_config->name, 0);
		process_name_set_name(main_config->name);
		process_name_replace_cmdline(argv, main_config->name);
	}
	afb_debug("main-args");

	// --------- run -----------
	if (main_config->background) {
		// --------- in background mode -----------
		INFO("entering background mode");
		daemonize();
	} else {
		// ---- in foreground mode --------------------
		INFO("entering foreground mode");
	}
	INFO("running with pid %d", getpid());

	/* set the daemon environment */
	setup_daemon();

	afb_debug("main-start");

	/* enter job processing */
	jobs_start(3, 0, 50, start, NULL);
	WARNING("hoops returned from jobs_enter! [report bug]");
	return 1;
}

