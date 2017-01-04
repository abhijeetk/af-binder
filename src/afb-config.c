/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
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
#include <getopt.h>
#include <limits.h>

#include "verbose.h"
#include "afb-config.h"
#include "afb-hook.h"

#include <afb/afb-binding.h>

#if !defined(BINDING_INSTALL_DIR)
#error "you should define BINDING_INSTALL_DIR"
#endif

#define AFB_VERSION    "0.5"

// default
#define DEFLT_CNTX_TIMEOUT  3600	// default Client Connection
					// Timeout
#define DEFLT_API_TIMEOUT   20		// default Plugin API Timeout [0=NoLimit
					// for Debug Only]
#define DEFLT_CACHE_TIMEOUT 100000	// default Static File Chache
					// [Client Side Cache
					// 100000~=1day]
#define DEFLT_AUTH_TOKEN    NULL	// expect for debug should == NULL
#define CTX_NBCLIENTS       10		// allow a default of 10 authenticated
					// clients


// Define command line option
#define SET_VERBOSE        1
#define SET_BACKGROUND     2
#define SET_FORGROUND      3

#define SET_TCP_PORT       5
#define SET_ROOT_DIR       6
#define SET_ROOT_BASE      7
#define SET_ROOT_API       8
#define SET_ALIAS          9

#define SET_CACHE_TIMEOUT  10
#define SET_SESSION_DIR    11

#define SET_AUTH_TOKEN     12
#define SET_LDPATH         13
#define SET_APITIMEOUT     14
#define SET_CNTXTIMEOUT    15

#define DISPLAY_VERSION    16
#define DISPLAY_HELP       17

#define SET_MODE           18
#define SET_READYFD        19

#define DBUS_CLIENT        20
#define DBUS_SERVICE       21
#define SO_BINDING         22

#define SET_SESSIONMAX     23

#define WS_CLIENT          24
#define WS_SERVICE         25

#define SET_ROOT_HTTP      26

#define SET_TRACEREQ       27

// Command line structure hold cli --command + help text
typedef struct {
	int val;		// command number within application
	int has_arg;		// command number within application
	char *name;		// command as used in --xxxx cli
	char *help;		// help text
} AFB_options;

// Supported option
static AFB_options cliOptions[] = {
/* *INDENT-OFF* */
	{SET_VERBOSE,       0, "verbose",     "Verbose Mode, repeat to increase verbosity"},

	{SET_FORGROUND,     0, "foreground",  "Get all in foreground mode"},
	{SET_BACKGROUND,    0, "daemon",      "Get all in background mode"},

	{SET_TCP_PORT,      1, "port",        "HTTP listening TCP port  [default 1234]"},
	{SET_ROOT_DIR,      1, "rootdir",     "Root Directory [default $HOME/.AFB]"},
	{SET_ROOT_HTTP,     1, "roothttp",    "HTTP Root Directory [default rootdir]"},
	{SET_ROOT_BASE,     1, "rootbase",    "Angular Base Root URL [default /opa]"},
	{SET_ROOT_API,      1, "rootapi",     "HTML Root API URL [default /api]"},
	{SET_ALIAS,         1, "alias",       "Muliple url map outside of rootdir [eg: --alias=/icons:/usr/share/icons]"},

	{SET_APITIMEOUT,    1, "apitimeout",  "Binding API timeout in seconds [default 10]"},
	{SET_CNTXTIMEOUT,   1, "cntxtimeout", "Client Session Context Timeout [default 900]"},
	{SET_CACHE_TIMEOUT, 1, "cache-eol",   "Client cache end of live [default 3600]"},

	{SET_SESSION_DIR,   1, "sessiondir",  "Sessions file path [default rootdir/sessions]"},

	{SET_LDPATH,        1, "ldpaths",     "Load bindingss from dir1:dir2:... [default = " BINDING_INSTALL_DIR "]"},
	{SET_AUTH_TOKEN,    1, "token",       "Initial Secret [default=no-session, --token= for session without authentication]"},

	{DISPLAY_VERSION,   0, "version",     "Display version and copyright"},
	{DISPLAY_HELP,      0, "help",        "Display this help"},

	{SET_MODE,          1, "mode",        "Set the mode: either local, remote or global"},
	{SET_READYFD,       1, "readyfd",     "Set the #fd to signal when ready"},

	{DBUS_CLIENT,       1, "dbus-client", "Bind to an afb service through dbus"},
	{DBUS_SERVICE,      1, "dbus-server", "Provides an afb service through dbus"},
	{WS_CLIENT,         1, "ws-client",   "Bind to an afb service through websocket"},
	{WS_SERVICE,        1, "ws-server",   "Provides an afb service through websockets"},
	{SO_BINDING,        1, "binding",     "Load the binding of path"},

	{SET_SESSIONMAX,    1, "session-max", "Max count of session simultaneously [default 10]"},

	{SET_TRACEREQ,      1, "tracereq",    "Log the requests: no, common, extra, all"},

	{0, 0, NULL, NULL}
/* *INDENT-ON* */
};


struct enumdesc
{
	const char *name;
	int value;
};

static struct enumdesc tracereq_desc[] = {
	{ "no",     0 },
	{ "common", afb_hook_flags_req_common },
	{ "extra",  afb_hook_flags_req_extra },
	{ "all",    afb_hook_flags_req_all },
	{ NULL, 0 }
};

static struct enumdesc mode_desc[] = {
	{ "local",  AFB_MODE_LOCAL },
	{ "remote", AFB_MODE_REMOTE },
	{ "global", AFB_MODE_GLOBAL },
	{ NULL, 0 }
};

/*----------------------------------------------------------
 | printversion
 |   print version and copyright
 +--------------------------------------------------------- */
static void printVersion(FILE * file)
{
	fprintf(file, "\n----------------------------------------- \n");
	fprintf(file, "  AFB [Application Framework Binder] version=%s |\n",
		AFB_VERSION);
	fprintf(file, " \n");
	fprintf(file,
		"  Copyright (C) 2015, 2016, 2017 \"IoT.bzh\" [fulup -at- iot.bzh]\n");
	fprintf(file, "  AFB comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(file, "  Licence Apache 2\n\n");
	exit(0);
}

/*----------------------------------------------------------
 | printHelp
 |   print information from long option array
 +--------------------------------------------------------- */

static void printHelp(FILE * file, const char *name)
{
	int ind;
	char command[50];

	fprintf(file, "%s:\nallowed options\n", name);
	for (ind = 0; cliOptions[ind].name != NULL; ind++) {
		strcpy(command, cliOptions[ind].name);
		if (cliOptions[ind].has_arg)
			strcat(command, "=xxxx");
		fprintf(file, "  --%-15s %s\n", command, cliOptions[ind].help);
	}
	fprintf(file,
		"Example:\n  %s\\\n  --verbose --port=1234 --token='azerty' --ldpaths=build/bindings:/usr/lib64/agl/bindings\n",
		name);
}

// load config from disk and merge with CLI option
static void config_set_default(struct afb_config *config)
{
	// default HTTP port
	if (config->httpdPort == 0)
		config->httpdPort = 1234;

	// default binding API timeout
	if (config->apiTimeout == 0)
		config->apiTimeout = DEFLT_API_TIMEOUT;

	// default AUTH_TOKEN
	if (config->token == NULL)
		config->token = DEFLT_AUTH_TOKEN;

	// cache timeout default one hour
	if (config->cacheTimeout == 0)
		config->cacheTimeout = DEFLT_CACHE_TIMEOUT;

	// cache timeout default one hour
	if (config->cntxTimeout == 0)
		config->cntxTimeout = DEFLT_CNTX_TIMEOUT;

	// max count of sessions
	if (config->nbSessionMax == 0)
		config->nbSessionMax = CTX_NBCLIENTS;

	if (config->rootdir == NULL) {
		config->rootdir = getenv("AFBDIR");
		if (config->rootdir == NULL) {
			config->rootdir = malloc(512);
			strncpy(config->rootdir, getenv("HOME"), 512);
			strncat(config->rootdir, "/.AFB", 512);
		}
	}
	// if no Angular/HTML5 rootbase let's try '/' as default
	if (config->rootbase == NULL)
		config->rootbase = "/opa";

	if (config->rootapi == NULL)
		config->rootapi = "/api";

	if (config->ldpaths == NULL)
		config->ldpaths = BINDING_INSTALL_DIR;

	// if no session dir create a default path from rootdir
	if (config->sessiondir == NULL) {
		config->sessiondir = malloc(512);
		strncpy(config->sessiondir, config->rootdir, 512);
		strncat(config->sessiondir, "/sessions", 512);
	}
	// if no config dir create a default path from sessiondir
	if (config->console == NULL) {
		config->console = malloc(512);
		strncpy(config->console, config->sessiondir, 512);
		strncat(config->console, "/AFB-console.out", 512);
	}
}

/*---------------------------------------------------------
 | main
 |   Parse option and launch action
 +--------------------------------------------------------- */

static void list_add(struct afb_config_list **head, char *value)
{
	struct afb_config_list *item;

	/*
	 * search tail 
	 */
	item = *head;
	while (item != NULL) {
		head = &item->next;
		item = item->next;
	}

	/*
	 * alloc the item 
	 */
	item = malloc(sizeof *item);
	if (item == NULL) {
		ERROR("out of memory");
		exit(1);
	}

	/*
	 * init the item 
	 */
	*head = item;
	item->value = value;
	item->next = NULL;
}

static char *argvalstr(int index)
{
	if (optarg == 0) {
		ERROR("option [--%s] needs a value i.e. --%s=xxx",
		      cliOptions[index].name, cliOptions[index].name);
		exit(1);
	}
	return optarg;
}

static int argvalenum(int index, struct enumdesc *desc)
{
	int i;
	size_t len;
	char *list, *name = argvalstr(index);

	i = 0;
	while(desc[i].name && strcmp(desc[i].name, name))
		i++;
	if (!desc[i].name) {
		len = 0;
		i = 0;
		while(desc[i].name)
			len += strlen(desc[i++].name);
		list = malloc(len + i + i);
		if (!i || !list)
			ERROR("option [--%s] bad value (found %s)",
				cliOptions[index].name, name);
		else {
			i = 0;
			strcpy(list, desc[i].name ? : "");
			while(desc[++i].name)
				strcat(strcat(list, ", "), desc[i].name);
			ERROR("option [--%s] bad value, only accepts values %s (found %s)",
				cliOptions[index].name, list, name);
		}
		free(list);
		exit(1);
	}
	return desc[i].value;
}

static int argvalint(int index, int mini, int maxi, int base)
{
	char *beg, *end;
	long int val;
	beg = argvalstr(index);
	val = strtol(beg, &end, base);
	if (*end || end == beg) {
		ERROR("option [--%s] requires a valid integer (found %s)",
			cliOptions[index].name, beg);
		exit(1);
	}
	if (val < (long int)mini || val > (long int)maxi) {
		ERROR("option [--%s] value out of bounds (not %d<=%ld<=%d)",
			cliOptions[index].name, mini, val, maxi);
		exit(1);
	}
	return (int)val;
}

static int argvalintdec(int index, int mini, int maxi)
{
	return argvalint(index, mini, maxi, 10);
}

static void noarg(int index)
{
	if (optarg != 0) {
		ERROR("option [--%s] need no value (found %s)", cliOptions[index].name, optarg);
		exit(1);
	}
}

static void parse_arguments(int argc, char **argv, struct afb_config *config)
{
	char *programName = argv[0];
	int optionIndex = 0;
	int optc, ind;
	int nbcmd;
	struct option *gnuOptions;

	// ------------------ Process Command Line -----------------------

	// if no argument print help and return
	if (argc < 2) {
		printHelp(stderr, programName);
		exit(1);
	}
	// build GNU getopt info from cliOptions
	nbcmd = sizeof(cliOptions) / sizeof(AFB_options);
	gnuOptions = malloc(sizeof(*gnuOptions) * (unsigned)nbcmd);
	for (ind = 0; ind < nbcmd; ind++) {
		gnuOptions[ind].name = cliOptions[ind].name;
		gnuOptions[ind].has_arg = cliOptions[ind].has_arg;
		gnuOptions[ind].flag = 0;
		gnuOptions[ind].val = cliOptions[ind].val;
	}

	// get all options from command line
	while ((optc =
		getopt_long(argc, argv, "vsp?", gnuOptions, &optionIndex))
	       != EOF) {
		switch (optc) {
		case SET_VERBOSE:
			verbosity++;
			break;

		case SET_TCP_PORT:
			config->httpdPort = argvalintdec(optionIndex, 1024, 32767);
			break;

		case SET_APITIMEOUT:
			config->apiTimeout = argvalintdec(optionIndex, 0, INT_MAX);
			break;

		case SET_CNTXTIMEOUT:
			config->cntxTimeout = argvalintdec(optionIndex, 0, INT_MAX);
			break;

		case SET_ROOT_DIR:
			config->rootdir = argvalstr(optionIndex);
			INFO("Forcing Rootdir=%s", config->rootdir);
			break;

		case SET_ROOT_HTTP:
			config->roothttp = argvalstr(optionIndex);
			INFO("Forcing Root HTTP=%s", config->roothttp);
			break;

		case SET_ROOT_BASE:
			config->rootbase = argvalstr(optionIndex);
			INFO("Forcing Rootbase=%s", config->rootbase);
			break;

		case SET_ROOT_API:
			config->rootapi = argvalstr(optionIndex);
			INFO("Forcing Rootapi=%s", config->rootapi);
			break;

		case SET_ALIAS:
			list_add(&config->aliases, argvalstr(optionIndex));
			break;

		case SET_AUTH_TOKEN:
			config->token = argvalstr(optionIndex);
			break;

		case SET_LDPATH:
			config->ldpaths = argvalstr(optionIndex);
			break;

		case SET_SESSION_DIR:
			config->sessiondir = argvalstr(optionIndex);
			break;

		case SET_CACHE_TIMEOUT:
			config->cacheTimeout = argvalintdec(optionIndex, 0, INT_MAX);
			break;

		case SET_SESSIONMAX:
			config->nbSessionMax = argvalintdec(optionIndex, 1, INT_MAX);
			break;

		case SET_FORGROUND:
			noarg(optionIndex);
			config->background = 0;
			break;

		case SET_BACKGROUND:
			noarg(optionIndex);
			config->background = 1;
			break;

		case SET_MODE:
			config->mode = argvalenum(optionIndex, mode_desc);
			break;

		case SET_READYFD:
			config->readyfd = argvalintdec(optionIndex, 0, INT_MAX);
			break;

		case DBUS_CLIENT:
			list_add(&config->dbus_clients, argvalstr(optionIndex));
			break;

		case DBUS_SERVICE:
			list_add(&config->dbus_servers, argvalstr(optionIndex));
			break;

		case WS_CLIENT:
			list_add(&config->ws_clients, argvalstr(optionIndex));
			break;

		case WS_SERVICE:
			list_add(&config->ws_servers, argvalstr(optionIndex));
			break;

		case SO_BINDING:
			list_add(&config->so_bindings, argvalstr(optionIndex));
			break;

		case SET_TRACEREQ:
			config->tracereq = argvalenum(optionIndex, tracereq_desc);
			break;

		case DISPLAY_VERSION:
			noarg(optionIndex);
			printVersion(stdout);
			break;

		case DISPLAY_HELP:
		default:
			printHelp(stdout, programName);
			exit(0);
		}
	}
	free(gnuOptions);

	config_set_default(config);
}

struct afb_config *afb_config_parse_arguments(int argc, char **argv)
{
	struct afb_config *result;

	result = calloc(1, sizeof *result);

	parse_arguments(argc, argv, result);
	return result;
}
