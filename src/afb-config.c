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
#define AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>

#include "verbose.h"
#include "afb-config.h"
#include "afb-hook.h"

#include <afb/afb-binding-v1.h>

#if !defined(BINDING_INSTALL_DIR)
#error "you should define BINDING_INSTALL_DIR"
#endif

#define AFB_VERSION    "0.6"

// default
#define DEFLT_CNTX_TIMEOUT  3600	// default Client Connection
					// Timeout
#define DEFLT_API_TIMEOUT   20		// default Plugin API Timeout [0=NoLimit
					// for Debug Only]
#define DEFLT_CACHE_TIMEOUT 100000	// default Static File Chache
					// [Client Side Cache
					// 100000~=1day]
#define CTX_NBCLIENTS       10		// allow a default of 10 authenticated
					// clients


// Define command line option
#define SET_BACKGROUND     2
#define SET_FORGROUND      3

#define SET_ROOT_DIR       6
#define SET_ROOT_BASE      7
#define SET_ROOT_API       8
#define SET_ALIAS          9

#define SET_CACHE_TIMEOUT  10
#define SET_SESSION_DIR    11

#define SET_LDPATH         13
#define SET_APITIMEOUT     14
#define SET_CNTXTIMEOUT    15
#define SET_WEAK_LDPATH    16
#define NO_LDPATH          17

#define SET_MODE           18

#define DBUS_CLIENT        20
#define DBUS_SERVICE       21
#define SO_BINDING         22

#define SET_SESSIONMAX     23

#define WS_CLIENT          24
#define WS_SERVICE         25

#define SET_ROOT_HTTP      26

#define SET_NO_HTTPD       28

#define ADD_CALL           'c'
#define SET_TRACEDITF      'D'
#define SET_TRACEEVT       'E'
#define SET_EXEC           'e'
#define DISPLAY_HELP       'h'
#if defined(WITH_MONITORING_OPTION)
#define SET_MONITORING     'M'
#endif
#define SET_NAME           'n'
#define SET_TCP_PORT       'p'
#define SET_QUIET          'q'
#define SET_RNDTOKEN       'r'
#define SET_TRACESVC       'S'
#define SET_TRACEREQ       'T'
#define SET_AUTH_TOKEN     't'
#define SET_UPLOAD_DIR     'u'
#define DISPLAY_VERSION    'V'
#define SET_VERBOSE        'v'
#define SET_WORK_DIR       'w'

const char shortopts[] =
	"c:D:E:ehn:p:qrT:t:u:Vvw:"
#if defined(WITH_MONITORING_OPTION)
	"M"
#endif
;

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
	{SET_QUIET,         0, "quiet",       "Quiet Mode, repeat to decrease verbosity"},

	{SET_FORGROUND,     0, "foreground",  "Get all in foreground mode"},
	{SET_BACKGROUND,    0, "daemon",      "Get all in background mode"},

	{SET_NAME,          1, "name",        "Set the visible name"},

	{SET_TCP_PORT,      1, "port",        "HTTP listening TCP port  [default 1234]"},
	{SET_ROOT_HTTP,     1, "roothttp",    "HTTP Root Directory [default no root http (files not served but apis still available)]"},
	{SET_ROOT_BASE,     1, "rootbase",    "Angular Base Root URL [default /opa]"},
	{SET_ROOT_API,      1, "rootapi",     "HTML Root API URL [default /api]"},
	{SET_ALIAS,         1, "alias",       "Multiple url map outside of rootdir [eg: --alias=/icons:/usr/share/icons]"},

	{SET_APITIMEOUT,    1, "apitimeout",  "Binding API timeout in seconds [default 10]"},
	{SET_CNTXTIMEOUT,   1, "cntxtimeout", "Client Session Context Timeout [default 900]"},
	{SET_CACHE_TIMEOUT, 1, "cache-eol",   "Client cache end of live [default 3600]"},

	{SET_WORK_DIR,      1, "workdir",     "Set the working directory [default: $PWD or current working directory]"},
	{SET_UPLOAD_DIR,    1, "uploaddir",   "Directory for uploading files [default: workdir]"},
	{SET_ROOT_DIR,      1, "rootdir",     "Root Directory of the application [default: workdir]"},
	{SET_SESSION_DIR,   1, "sessiondir",  "OBSOLETE (was: Sessions file path)"},

	{SET_LDPATH,        1, "ldpaths",     "Load bindings from dir1:dir2:... [default = " BINDING_INSTALL_DIR "]"},
	{SO_BINDING,        1, "binding",     "Load the binding of path"},
	{SET_WEAK_LDPATH,   1, "weak-ldpaths","Same as --ldpaths but ignore errors"},
	{NO_LDPATH,         0, "no-ldpaths",  "Discard default ldpaths loading"},

	{SET_AUTH_TOKEN,    1, "token",       "Initial Secret [default=no-session, --token= for session without authentication]"},
	{SET_RNDTOKEN,      0, "random-token","Enforce a random token"},

	{DISPLAY_VERSION,   0, "version",     "Display version and copyright"},
	{DISPLAY_HELP,      0, "help",        "Display this help"},

	{SET_MODE,          1, "mode",        "Set the mode: either local, remote or global"},

	{DBUS_CLIENT,       1, "dbus-client", "Bind to an afb service through dbus"},
	{DBUS_SERVICE,      1, "dbus-server", "Provides an afb service through dbus"},
	{WS_CLIENT,         1, "ws-client",   "Bind to an afb service through websocket"},
	{WS_SERVICE,        1, "ws-server",   "Provides an afb service through websockets"},

	{SET_SESSIONMAX,    1, "session-max", "Max count of session simultaneously [default 10]"},

	{SET_TRACEREQ,      1, "tracereq",    "Log the requests: no, common, extra, all"},
	{SET_TRACEDITF,     1, "traceditf",   "Log the requests: no, common, extra, all"},
	{SET_TRACESVC,      1, "tracesvc",    "Log the requests: no, all"},
	{SET_TRACEEVT,      1, "traceevt",    "Log the requests: no, common, extra, all"},

	{ADD_CALL,          1, "call",        "call at start format of val: API/VERB:json-args"},

	{SET_NO_HTTPD,      0, "no-httpd",    "Forbids HTTP service"},
	{SET_EXEC,          0, "exec",        "Execute the remaining arguments"},

#if defined(WITH_MONITORING_OPTION)
	{SET_MONITORING,    0, "monitoring",  "enable HTTP monitoring at <ROOT>/monitoring/"},
#endif
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

static struct enumdesc traceditf_desc[] = {
	{ "no",     0 },
	{ "common", afb_hook_flags_ditf_common },
	{ "extra",  afb_hook_flags_ditf_extra },
	{ "all",    afb_hook_flags_ditf_all },
	{ NULL, 0 }
};

static struct enumdesc tracesvc_desc[] = {
	{ "no",     0 },
	{ "all",    afb_hook_flags_svc_all },
	{ NULL, 0 }
};

static struct enumdesc traceevt_desc[] = {
	{ "no",     0 },
	{ "common", afb_hook_flags_evt_common },
	{ "extra",  afb_hook_flags_evt_extra },
	{ "all",    afb_hook_flags_evt_all },
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
		"Example:\n  %s  --verbose --port=1234 --token='azerty' --ldpaths=build/bindings:/usr/lib64/agl/bindings\n",
		name);
}


/*----------------------------------------------------------
 |   adds a string to the list
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

/*---------------------------------------------------------
 |   helpers for argument scanning
 +--------------------------------------------------------- */

static const char *name_of_option(int optc)
{
	AFB_options *o = cliOptions;
	while (o->name && o->val != optc)
		o++;
	return o->name ? : "<unknown-option-name>";
}

static const char *current_argument(int optc)
{
	if (optarg == 0) {
		ERROR("option [--%s] needs a value i.e. --%s=xxx",
		      name_of_option(optc), name_of_option(optc));
		exit(1);
	}
	return optarg;
}

static char *argvalstr(int optc)
{
	char *result = strdup(current_argument(optc));
	if (result == NULL) {
		ERROR("can't alloc memory");
		exit(1);
	}
	return result;
}

static int argvalenum(int optc, struct enumdesc *desc)
{
	int i;
	size_t len;
	char *list;
	const char *name = current_argument(optc);

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
				name_of_option(optc), name);
		else {
			i = 0;
			strcpy(list, desc[i].name ? : "");
			while(desc[++i].name)
				strcat(strcat(list, ", "), desc[i].name);
			ERROR("option [--%s] bad value, only accepts values %s (found %s)",
				name_of_option(optc), list, name);
		}
		free(list);
		exit(1);
	}
	return desc[i].value;
}

static int argvalint(int optc, int mini, int maxi, int base)
{
	const char *beg, *end;
	long int val;
	beg = current_argument(optc);
	val = strtol(beg, (char**)&end, base);
	if (*end || end == beg) {
		ERROR("option [--%s] requires a valid integer (found %s)",
			name_of_option(optc), beg);
		exit(1);
	}
	if (val < (long int)mini || val > (long int)maxi) {
		ERROR("option [--%s] value out of bounds (not %d<=%ld<=%d)",
			name_of_option(optc), mini, val, maxi);
		exit(1);
	}
	return (int)val;
}

static int argvalintdec(int optc, int mini, int maxi)
{
	return argvalint(optc, mini, maxi, 10);
}

static void noarg(int optc)
{
	if (optarg != 0) {
		ERROR("option [--%s] need no value (found %s)", name_of_option(optc), optarg);
		exit(1);
	}
}

/*---------------------------------------------------------
 |   Parse option and launch action
 +--------------------------------------------------------- */

static void parse_arguments(int argc, char **argv, struct afb_config *config)
{
	char *programName = argv[0];
	int optc, ind;
	int nbcmd;
	struct option *gnuOptions;

	// ------------------ Process Command Line -----------------------

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
	while ((optc = getopt_long(argc, argv, shortopts, gnuOptions, NULL)) != EOF) {
		switch (optc) {
		case SET_VERBOSE:
			verbosity++;
			break;

		case SET_QUIET:
			verbosity--;
			break;

		case SET_TCP_PORT:
			config->httpdPort = argvalintdec(optc, 1024, 32767);
			break;

		case SET_APITIMEOUT:
			config->apiTimeout = argvalintdec(optc, 0, INT_MAX);
			break;

		case SET_CNTXTIMEOUT:
			config->cntxTimeout = argvalintdec(optc, 0, INT_MAX);
			break;

		case SET_ROOT_DIR:
			config->rootdir = argvalstr(optc);
			INFO("Forcing Rootdir=%s", config->rootdir);
			break;

		case SET_ROOT_HTTP:
			config->roothttp = argvalstr(optc);
			INFO("Forcing Root HTTP=%s", config->roothttp);
			break;

		case SET_ROOT_BASE:
			config->rootbase = argvalstr(optc);
			INFO("Forcing Rootbase=%s", config->rootbase);
			break;

		case SET_ROOT_API:
			config->rootapi = argvalstr(optc);
			INFO("Forcing Rootapi=%s", config->rootapi);
			break;

		case SET_ALIAS:
			list_add(&config->aliases, argvalstr(optc));
			break;

		case SET_AUTH_TOKEN:
			config->token = argvalstr(optc);
			break;

		case SET_LDPATH:
			list_add(&config->ldpaths, argvalstr(optc));
			break;

		case SET_WEAK_LDPATH:
			list_add(&config->weak_ldpaths, argvalstr(optc));
			break;

		case NO_LDPATH:
			noarg(optc);
			config->no_ldpaths = 1;
			break;

		case ADD_CALL:
			list_add(&config->calls, argvalstr(optc));
			break;

		case SET_SESSION_DIR:
			/* config->sessiondir = argvalstr(optc); */
			WARNING("Obsolete option %s ignored", name_of_option(optc));
			break;

		case SET_UPLOAD_DIR:
			config->uploaddir = argvalstr(optc);
			break;

		case SET_WORK_DIR:
			config->workdir = argvalstr(optc);
			break;

		case SET_CACHE_TIMEOUT:
			config->cacheTimeout = argvalintdec(optc, 0, INT_MAX);
			break;

		case SET_SESSIONMAX:
			config->nbSessionMax = argvalintdec(optc, 1, INT_MAX);
			break;

		case SET_FORGROUND:
			noarg(optc);
			config->background = 0;
			break;

		case SET_BACKGROUND:
			noarg(optc);
			config->background = 1;
			break;

		case SET_NAME:
			config->name = argvalstr(optc);
			break;

		case SET_MODE:
			config->mode = argvalenum(optc, mode_desc);
			break;

		case DBUS_CLIENT:
			list_add(&config->dbus_clients, argvalstr(optc));
			break;

		case DBUS_SERVICE:
			list_add(&config->dbus_servers, argvalstr(optc));
			break;

		case WS_CLIENT:
			list_add(&config->ws_clients, argvalstr(optc));
			break;

		case WS_SERVICE:
			list_add(&config->ws_servers, argvalstr(optc));
			break;

		case SO_BINDING:
			list_add(&config->so_bindings, argvalstr(optc));
			break;

		case SET_TRACEREQ:
			config->tracereq = argvalenum(optc, tracereq_desc);
			break;

		case SET_TRACEDITF:
			config->traceditf = argvalenum(optc, traceditf_desc);
			break;

		case SET_TRACESVC:
			config->tracesvc = argvalenum(optc, tracesvc_desc);
			break;

		case SET_TRACEEVT:
			config->traceevt = argvalenum(optc, traceevt_desc);
			break;

		case SET_NO_HTTPD:
			noarg(optc);
			config->noHttpd = 1;
			break;

		case SET_EXEC:
			config->exec = &argv[optind];
			optind = argc;
			break;

		case SET_RNDTOKEN:
			config->random_token = 1;
			break;

#if defined(WITH_MONITORING_OPTION)
		case SET_MONITORING:
			config->monitoring = 1;
			break;
#endif

		case DISPLAY_VERSION:
			noarg(optc);
			printVersion(stdout);
			exit(0);

		case DISPLAY_HELP:
			printHelp(stdout, programName);
			exit(0);

		default:
			exit(1);
		}
	}
	free(gnuOptions);
}

static void fulfill_config(struct afb_config *config)
{
	// default HTTP port
	if (config->httpdPort == 0)
		config->httpdPort = 1234;

	// default binding API timeout
	if (config->apiTimeout == 0)
		config->apiTimeout = DEFLT_API_TIMEOUT;

	// default AUTH_TOKEN
	if (config->random_token)
		config->token = NULL;

	// cache timeout default one hour
	if (config->cacheTimeout == 0)
		config->cacheTimeout = DEFLT_CACHE_TIMEOUT;

	// cache timeout default one hour
	if (config->cntxTimeout == 0)
		config->cntxTimeout = DEFLT_CNTX_TIMEOUT;

	// max count of sessions
	if (config->nbSessionMax == 0)
		config->nbSessionMax = CTX_NBCLIENTS;

	/* set directories */
	if (config->workdir == NULL)
		config->workdir = ".";

	if (config->rootdir == NULL)
		config->rootdir = ".";

	if (config->uploaddir == NULL)
		config->uploaddir = ".";

	// if no Angular/HTML5 rootbase let's try '/' as default
	if (config->rootbase == NULL)
		config->rootbase = "/opa";

	if (config->rootapi == NULL)
		config->rootapi = "/api";

	if (config->ldpaths == NULL && config->weak_ldpaths == NULL && !config->no_ldpaths)
		list_add(&config->ldpaths, BINDING_INSTALL_DIR);

#if defined(WITH_MONITORING_OPTION)
	if (config->monitoring)
		list_add(&config->aliases, strdup("/monitoring:"BINDING_INSTALL_DIR"/monitoring"));
#endif

	// if no config dir create a default path from uploaddir
	if (config->console == NULL) {
		config->console = malloc(512);
		strncpy(config->console, config->uploaddir, 512);
		strncat(config->console, "/AFB-console.out", 512);
	}
}

void afb_config_dump(struct afb_config *config)
{
	struct afb_config_list *l;
	struct enumdesc *e;
	char **v;

#define NN(x)   (x)?:""
#define P(...)  fprintf(stderr, __VA_ARGS__)
#define PF(x)   P("-- %15s: ", #x)
#define PE      P("\n")
#define S(x)	PF(x);P("%s",NN(config->x));PE;
#define D(x)	PF(x);P("%d",config->x);PE;
#define H(x)	PF(x);P("%x",config->x);PE;
#define B(x)	PF(x);P("%s",config->x?"yes":"no");PE;
#define L(x)	PF(x);l=config->x;if(l){P("%s\n",NN(l->value));for(l=l->next;l;l=l->next)P("-- %15s  %s\n","",NN(l->value));}else PE;
#define E(x,d)	for(e=d;e->name&&e->value!=config->x;e++);if(e->name){PF(x);P("%s",e->name);PE;}else{D(x);}
#define V(x)	P("-- %15s:", #x);for(v=config->x;v&&*v;v++)P(" %s",*v); PE;

	P("---BEGIN-OF-CONFIG---\n");
	S(console)
	S(rootdir)
	S(roothttp)
	S(rootbase)
	S(rootapi)
	S(workdir)
	S(uploaddir)
	S(token)
	S(name)

	L(aliases)
	L(dbus_clients)
	L(dbus_servers)
	L(ws_clients)
	L(ws_servers)
	L(so_bindings)
	L(ldpaths)
	L(weak_ldpaths)
	L(calls)

	V(exec)

	D(httpdPort)
	D(cacheTimeout)
	D(apiTimeout)
	D(cntxTimeout)
	D(nbSessionMax)

	E(mode,mode_desc)
	E(tracereq,tracereq_desc)
	E(traceditf,traceditf_desc)
	E(tracesvc,tracesvc_desc)
	E(traceevt,traceevt_desc)

	B(no_ldpaths)
	B(noHttpd)
	B(background)
	B(monitoring)
	B(random_token)
	P("---END-OF-CONFIG---\n");

#undef V
#undef E
#undef L
#undef B
#undef H
#undef D
#undef S
#undef PE
#undef PF
#undef P
#undef NN
}

struct afb_config *afb_config_parse_arguments(int argc, char **argv)
{
	struct afb_config *result;

	result = calloc(1, sizeof *result);

	parse_arguments(argc, argv, result);
	fulfill_config(result);
	if (verbosity >= 3)
		afb_config_dump(result);
	return result;
}

