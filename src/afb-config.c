/*
 * Copyright (C) 2015-2018 "IoT.bzh"
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
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>

#include <json-c/json.h>

#include "verbose.h"
#include "afb-config.h"
#include "afb-hook.h"

#define _d2s_(x)  #x
#define d2s(x)    _d2s_(x)

#if !defined(BINDING_INSTALL_DIR)
#error "you should define BINDING_INSTALL_DIR"
#endif
#if !defined(AFB_VERSION)
#error "you should define AFB_VERSION"
#endif

/**
 * The default timeout of sessions in seconds
 */
#define DEFAULT_SESSION_TIMEOUT		32000000

/**
 * The default timeout of api calls in seconds
 */
#define DEFAULT_API_TIMEOUT		20

/**
 * The default timeout of cache in seconds
 */
#define DEFAULT_CACHE_TIMEOUT		100000

/**
 * The default maximum count of sessions
 */
#define DEFAULT_MAX_SESSION_COUNT       200

/**
 * The default HTTP port to serve
 */
#define DEFAULT_HTTP_PORT		1234

// Define command line option
#define SET_BACKGROUND     1
#define SET_FORGROUND      2
#define SET_ROOT_DIR       3
#define SET_ROOT_BASE      4
#define SET_ROOT_API       5
#define SET_ALIAS          6

#define SET_CACHE_TIMEOUT  7

#define AUTO_WS            8
#define AUTO_LINK          9

#define SET_LDPATH         10
#define SET_APITIMEOUT     11
#define SET_CNTXTIMEOUT    12
#define SET_WEAK_LDPATH    13
#define NO_LDPATH          14

#define SET_SESSIONMAX     15

#define WS_CLIENT          16
#define WS_SERVICE         17

#define SET_ROOT_HTTP      18

#define SET_NO_HTTPD       19

#define SET_TRACEEVT       20
#define SET_TRACESES       21
#define SET_TRACEREQ       22
#define SET_TRACEAPI       23
#if !defined(REMOVE_LEGACY_TRACE)
#define SET_TRACEDITF      24
#define SET_TRACESVC       25
#endif

#if defined(WITH_DBUS_TRANSPARENCY)
#   define DBUS_CLIENT        30
#   define DBUS_SERVICE       31
#endif


#define AUTO_API           'A'
#define SO_BINDING         'b'
#define ADD_CALL           'c'
#define SET_EXEC           'e'
#define DISPLAY_HELP       'h'
#define SET_LOG            'l'
#if defined(WITH_MONITORING_OPTION)
#define SET_MONITORING     'M'
#endif
#define SET_NAME           'n'
#define SET_TCP_PORT       'p'
#define SET_QUIET          'q'
#define SET_RNDTOKEN       'r'
#define SET_AUTH_TOKEN     't'
#define SET_UPLOAD_DIR     'u'
#define DISPLAY_VERSION    'V'
#define SET_VERBOSE        'v'
#define SET_WORK_DIR       'w'

const char shortopts[] =
	"A:"
	"b:"
	"c:"
	"e"
	"h"
	"l:"
#if defined(WITH_MONITORING_OPTION)
	"M"
#endif
	"n:"
	"p:"
	"q"
	"r"
	"t:"
	"u:"
	"V"
	"v"
	"w:"
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
	{SET_LOG,           1, "log",         "Tune log level"},

	{SET_FORGROUND,     0, "foreground",  "Get all in foreground mode"},
	{SET_BACKGROUND,    0, "daemon",      "Get all in background mode"},

	{SET_NAME,          1, "name",        "Set the visible name"},

	{SET_TCP_PORT,      1, "port",        "HTTP listening TCP port  [default " d2s(DEFAULT_HTTP_PORT) "]"},
	{SET_ROOT_HTTP,     1, "roothttp",    "HTTP Root Directory [default no root http (files not served but apis still available)]"},
	{SET_ROOT_BASE,     1, "rootbase",    "Angular Base Root URL [default /opa]"},
	{SET_ROOT_API,      1, "rootapi",     "HTML Root API URL [default /api]"},
	{SET_ALIAS,         1, "alias",       "Multiple url map outside of rootdir [eg: --alias=/icons:/usr/share/icons]"},

	{SET_APITIMEOUT,    1, "apitimeout",  "Binding API timeout in seconds [default " d2s(DEFAULT_API_TIMEOUT) "]"},
	{SET_CNTXTIMEOUT,   1, "cntxtimeout", "Client Session Context Timeout [default " d2s(DEFAULT_SESSION_TIMEOUT) "]"},
	{SET_CACHE_TIMEOUT, 1, "cache-eol",   "Client cache end of live [default " d2s(DEFAULT_CACHE_TIMEOUT) "]"},

	{SET_WORK_DIR,      1, "workdir",     "Set the working directory [default: $PWD or current working directory]"},
	{SET_UPLOAD_DIR,    1, "uploaddir",   "Directory for uploading files [default: workdir]"},
	{SET_ROOT_DIR,      1, "rootdir",     "Root Directory of the application [default: workdir]"},

	{SET_LDPATH,        1, "ldpaths",     "Load bindings from dir1:dir2:... [default = " BINDING_INSTALL_DIR "]"},
	{SO_BINDING,        1, "binding",     "Load the binding of path"},
	{SET_WEAK_LDPATH,   1, "weak-ldpaths","Same as --ldpaths but ignore errors"},
	{NO_LDPATH,         0, "no-ldpaths",  "Discard default ldpaths loading"},

	{SET_AUTH_TOKEN,    1, "token",       "Initial Secret [default=random, use --token="" to allow any token]"},
	{SET_RNDTOKEN,      0, "random-token","Enforce a random token"},

	{DISPLAY_VERSION,   0, "version",     "Display version and copyright"},
	{DISPLAY_HELP,      0, "help",        "Display this help"},

#if defined(WITH_DBUS_TRANSPARENCY)
	{DBUS_CLIENT,       1, "dbus-client", "Bind to an afb service through dbus"},
	{DBUS_SERVICE,      1, "dbus-server", "Provide an afb service through dbus"},
#endif
	{WS_CLIENT,         1, "ws-client",   "Bind to an afb service through websocket"},
	{WS_SERVICE,        1, "ws-server",   "Provide an afb service through websockets"},

	{AUTO_API,          1, "auto-api",    "Automatic load of api of the given directory"},

	{SET_SESSIONMAX,    1, "session-max", "Max count of session simultaneously [default " d2s(DEFAULT_MAX_SESSION_COUNT) "]"},

	{SET_TRACEREQ,      1, "tracereq",    "Log the requests: no, common, extra, all"},
#if !defined(REMOVE_LEGACY_TRACE)
	{SET_TRACEDITF,     1, "traceditf",   "Log the daemons: no, common, all"},
	{SET_TRACESVC,      1, "tracesvc",    "Log the services: no, all"},
#endif
	{SET_TRACEEVT,      1, "traceevt",    "Log the events: no, common, extra, all"},
	{SET_TRACESES,      1, "traceses",    "Log the sessions: no, all"},
	{SET_TRACEAPI,      1, "traceapi",    "Log the apis: no, common, api, event, all"},

	{ADD_CALL,          1, "call",        "call at start format of val: API/VERB:json-args"},

	{SET_NO_HTTPD,      0, "no-httpd",    "Forbid HTTP service"},
	{SET_EXEC,          0, "exec",        "Execute the remaining arguments"},

#if defined(WITH_MONITORING_OPTION)
	{SET_MONITORING,    0, "monitoring",  "Enable HTTP monitoring at <ROOT>/monitoring/"},
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

#if !defined(REMOVE_LEGACY_TRACE)
static struct enumdesc traceditf_desc[] = {
	{ "no",     0 },
	{ "common", afb_hook_flags_api_ditf_common },
	{ "all",    afb_hook_flags_api_ditf_all },
	{ NULL, 0 }
};

static struct enumdesc tracesvc_desc[] = {
	{ "no",     0 },
	{ "all",    afb_hook_flags_api_svc_all },
	{ NULL, 0 }
};
#endif

static struct enumdesc traceevt_desc[] = {
	{ "no",     0 },
	{ "common", afb_hook_flags_evt_common },
	{ "extra",  afb_hook_flags_evt_extra },
	{ "all",    afb_hook_flags_evt_all },
	{ NULL, 0 }
};

static struct enumdesc traceses_desc[] = {
	{ "no",     0 },
	{ "common", afb_hook_flags_session_common },
	{ "all",    afb_hook_flags_session_all },
	{ NULL, 0 }
};

static struct enumdesc traceapi_desc[] = {
	{ "no",		0 },
	{ "common",	afb_hook_flags_api_common },
	{ "api",	afb_hook_flags_api_api|afb_hook_flag_api_start },
	{ "event",	afb_hook_flags_api_event|afb_hook_flag_api_start },
	{ "all",	afb_hook_flags_api_all },
	{ NULL, 0 }
};

/*----------------------------------------------------------
 | printversion
 |   print version and copyright
 +--------------------------------------------------------- */
static void printVersion(FILE * file)
{
	fprintf(file,
		"\n"
		"  AGL Framework Binder [AFB %s] "
#if defined(WITH_DBUS_TRANSPARENCY)
		"+"
#else
		"-"
#endif
		"DBUS "
#if defined(WITH_MONITORING_OPTION)
		"+"
#else
		"-"
#endif
		"MONITOR "
#if defined(WITH_SUPERVISION)
		"+"
#else
		"-"
#endif
		"SUPERVISION [BINDINGS "
#if defined(WITH_LEGACY_BINDING_V1)
		"+"
#else
		"-"
#endif
		"V1 "
#if defined(WITH_LEGACY_BINDING_VDYN)
		"+"
#else
		"-"
#endif
		"VDYN +V2 +V3]\n"
		"\n",
		AFB_VERSION
	);
	fprintf(file,
		"  Copyright (C) 2015-2018 \"IoT.bzh\"\n"
		"  AFB comes with ABSOLUTELY NO WARRANTY.\n"
		"  Licence Apache 2\n"
		"\n");
}

/*----------------------------------------------------------
 | printHelp
 |   print information from long option array
 +--------------------------------------------------------- */

static void printHelp(FILE * file, const char *name)
{
	int ind;
	char command[50], sht[4];

	sht[3] = 0;
	fprintf(file, "%s:\nallowed options\n", name);
	for (ind = 0; cliOptions[ind].name != NULL; ind++) {
		if (((cliOptions[ind].val >= 'a' && cliOptions[ind].val <= 'z')
		 || (cliOptions[ind].val >= 'A' && cliOptions[ind].val <= 'Z')
		 || (cliOptions[ind].val >= '0' && cliOptions[ind].val <= '9'))
		 && strchr(shortopts, (char)cliOptions[ind].val)) {
			sht[0] = '-';
			sht[1] = (char)cliOptions[ind].val;
			sht[2] = ',';
		} else {
			sht[0] = sht[1] = sht[2] = ' ';
		}
		strcpy(command, cliOptions[ind].name);
		if (cliOptions[ind].has_arg)
			strcat(command, "=xxxx");
		fprintf(file, " %s --%-17s %s\n", sht, command, cliOptions[ind].help);
	}
	fprintf(file,
		"Example:\n  %s  --verbose --port=" d2s(DEFAULT_HTTP_PORT) " --token='azerty' --ldpaths=build/bindings:/usr/lib64/agl/bindings\n",
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

static char **make_exec(char **argv)
{
	char **result, *iter;
	size_t length;
	int i;

	length = 0;
	for (i = 0 ; argv[i] ; i++)
		length += strlen(argv[i]) + 1;

	result = malloc(length + ((unsigned)(i + 1)) * sizeof *result);
	if (result == NULL) {
		ERROR("can't alloc memory");
		exit(1);
	}

	iter = (char*)&result[i+1];
	for (i = 0 ; argv[i] ; i++) {
		result[i] = iter;
		iter = stpcpy(iter, argv[i]) + 1;
	}
	result[i] = NULL;
	return result;
}

/*---------------------------------------------------------
 |   set the log levels
 +--------------------------------------------------------- */

static void set_log(char *args)
{
	char o = 0, s, *p, *i = args;
	int lvl;

	for(;;) switch (*i) {
	case 0:
		return;
	case '+':
	case '-':
		o = *i;
		/*@fallthrough@*/
	case ' ':
	case ',':
		i++;
		break;
	default:
		p = i;
		while (isalpha(*p)) p++;
		s = *p;
		*p = 0;
		lvl = verbose_level_of_name(i);
		if (lvl < 0) {
			i = strdupa(i);
			*p = s;
			ERROR("Bad log name '%s' in %s", i, args);
			exit(1);
		}
		*p = s;
		i = p;
		if (o == '-')
			verbose_sub(lvl);
		else {
			if (!o) {
				verbose_clear();
				o = '+';
			}
			verbose_add(lvl);
		}
		break;
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
			verbose_inc();
			break;

		case SET_QUIET:
			verbose_dec();
			break;

		case SET_LOG:
			set_log(argvalstr(optc));
			break;

		case SET_TCP_PORT:
			config->http_port = argvalintdec(optc, 1024, 32767);
			break;

		case SET_APITIMEOUT:
			config->api_timeout = argvalintdec(optc, 0, INT_MAX);
			break;

		case SET_CNTXTIMEOUT:
			config->session_timeout = argvalintdec(optc, 0, INT_MAX);
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

		case SET_UPLOAD_DIR:
			config->uploaddir = argvalstr(optc);
			break;

		case SET_WORK_DIR:
			config->workdir = argvalstr(optc);
			break;

		case SET_CACHE_TIMEOUT:
			config->cache_timeout = argvalintdec(optc, 0, INT_MAX);
			break;

		case SET_SESSIONMAX:
			config->max_session_count = argvalintdec(optc, 1, INT_MAX);
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

#if defined(WITH_DBUS_TRANSPARENCY)
		case DBUS_CLIENT:
			list_add(&config->dbus_clients, argvalstr(optc));
			break;

		case DBUS_SERVICE:
			list_add(&config->dbus_servers, argvalstr(optc));
			break;
#endif

		case WS_CLIENT:
			list_add(&config->ws_clients, argvalstr(optc));
			break;

		case WS_SERVICE:
			list_add(&config->ws_servers, argvalstr(optc));
			break;

		case SO_BINDING:
			list_add(&config->so_bindings, argvalstr(optc));
			break;

		case AUTO_API:
			list_add(&config->auto_api, argvalstr(optc));
			break;

		case SET_TRACEREQ:
			config->tracereq = argvalenum(optc, tracereq_desc);
			break;

#if !defined(REMOVE_LEGACY_TRACE)
		case SET_TRACEDITF:
			config->traceditf = argvalenum(optc, traceditf_desc);
			break;

		case SET_TRACESVC:
			config->tracesvc = argvalenum(optc, tracesvc_desc);
			break;
#endif

		case SET_TRACEEVT:
			config->traceevt = argvalenum(optc, traceevt_desc);
			break;

		case SET_TRACESES:
			config->traceses = argvalenum(optc, traceses_desc);
			break;

		case SET_TRACEAPI:
			config->traceapi = argvalenum(optc, traceapi_desc);
			break;

		case SET_NO_HTTPD:
			noarg(optc);
			config->no_httpd = 1;
			break;

		case SET_EXEC:
			config->exec = make_exec(&argv[optind]);
			optind = argc; /* stop option scanning */
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
	if (config->http_port == 0)
		config->http_port = DEFAULT_HTTP_PORT;

	// default binding API timeout
	if (config->api_timeout == 0)
		config->api_timeout = DEFAULT_API_TIMEOUT;

	// default AUTH_TOKEN
	if (config->random_token)
		config->token = NULL;

	// cache timeout default one hour
	if (config->cache_timeout == 0)
		config->cache_timeout = DEFAULT_CACHE_TIMEOUT;

	// cache timeout default one hour
	if (config->session_timeout == 0)
		config->session_timeout = DEFAULT_SESSION_TIMEOUT;

	// max count of sessions
	if (config->max_session_count == 0)
		config->max_session_count = DEFAULT_MAX_SESSION_COUNT;

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

#if !defined(REMOVE_LEGACY_TRACE)
	config->traceapi |= config->traceditf | config->tracesvc;
#endif
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
#if defined(WITH_DBUS_TRANSPARENCY)
	L(dbus_clients)
	L(dbus_servers)
#endif
	L(ws_clients)
	L(ws_servers)
	L(so_bindings)
	L(auto_api)
	L(ldpaths)
	L(weak_ldpaths)
	L(calls)

	V(exec)

	D(http_port)
	D(cache_timeout)
	D(api_timeout)
	D(session_timeout)
	D(max_session_count)

	E(tracereq,tracereq_desc)
#if !defined(REMOVE_LEGACY_TRACE)
	E(traceditf,traceditf_desc)
	E(tracesvc,tracesvc_desc)
#endif
	E(traceevt,traceevt_desc)
	E(traceses,traceses_desc)
	E(traceapi,traceapi_desc)

	B(no_ldpaths)
	B(no_httpd)
	B(background)
#if defined(WITH_MONITORING_OPTION)
	B(monitoring)
#endif
	B(random_token)
	P("---END-OF-CONFIG---\n");

#undef V
#undef E
#undef L
#undef B
#undef D
#undef S
#undef PE
#undef PF
#undef P
#undef NN
}

static void on_environment_list(struct afb_config_list **to, const char *name)
{
	char *value = getenv(name);

	if (value)
		list_add(to, value);
}

static void on_environment_enum(int *to, const char *name, struct enumdesc *desc)
{
	char *value = getenv(name);

	if (value) {
		while (desc->name) {
			if (strcmp(desc->name, value))
				desc++;
			else {
				*to = desc->value;
				return;
			}
		}
		WARNING("Unknown value %s for environment variable %s, ignored", value, name);
	}
}

static void parse_environment(struct afb_config *config)
{
	on_environment_enum(&config->tracereq, "AFB_TRACEREQ", tracereq_desc);
#if !defined(REMOVE_LEGACY_TRACE)
	on_environment_enum(&config->traceditf, "AFB_TRACEDITF", traceditf_desc);
	on_environment_enum(&config->tracesvc, "AFB_TRACESVC", tracesvc_desc);
#endif
	on_environment_enum(&config->traceevt, "AFB_TRACEEVT", traceevt_desc);
	on_environment_enum(&config->traceses, "AFB_TRACESES", traceses_desc);
	on_environment_enum(&config->traceapi, "AFB_TRACEAPI", traceapi_desc);
	on_environment_list(&config->ldpaths, "AFB_LDPATHS");
}

struct afb_config *afb_config_parse_arguments(int argc, char **argv)
{
	struct afb_config *result;

	result = calloc(1, sizeof *result);

	parse_environment(result);
	parse_arguments(argc, argv, result);
	fulfill_config(result);
	if (verbose_wants(Log_Level_Info))
		afb_config_dump(result);
	return result;
}

struct json_object *afb_config_json(struct afb_config *config)
{
	struct json_object *r, *a;
	struct afb_config_list *l;
	struct enumdesc *e;
	char **v;

#define XA(t,o)		json_object_array_add(t,o);
#define XO(t,x,o)	json_object_object_add(t,x,o);
#define YS(s)		((s)?json_object_new_string(s):NULL)

#define AO(o)		XA(a,o)
#define AS(s)		AO(YS(s))
#define RO(x,o)		XO(r,x,o)
#define RS(x,s)		RO(x,YS(s))
#define RA(x)		RO(x,(a=json_object_new_array()))
#define RI(x,i)		RO(x,json_object_new_int(i))
#define RB(x,b)		RO(x,json_object_new_boolean(b))

#define S(x)		RS(#x,config->x)
#define V(x)		RA(#x);for(v=config->x;v&&*v;v++)AS(*v);
#define L(x)		RA(#x);for(l=config->x;l;l=l->next)AS(l->value);
#define D(x)		RI(#x,config->x)
#define B(x)		RB(#x,config->x)
#define E(x,d)		for(e=d;e->name&&e->value!=config->x;e++);if(e->name){RS(#x,e->name);}else{D(x);}

	r = json_object_new_object();
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
#if defined(WITH_DBUS_TRANSPARENCY)
	L(dbus_clients)
	L(dbus_servers)
#endif
	L(ws_clients)
	L(ws_servers)
	L(so_bindings)
	L(auto_api)
	L(ldpaths)
	L(weak_ldpaths)
	L(calls)

	V(exec)

	D(http_port)
	D(cache_timeout)
	D(api_timeout)
	D(session_timeout)
	D(max_session_count)

	E(tracereq,tracereq_desc)
#if !defined(REMOVE_LEGACY_TRACE)
	E(traceditf,traceditf_desc)
	E(tracesvc,tracesvc_desc)
#endif
	E(traceevt,traceevt_desc)
	E(traceses,traceses_desc)
	E(traceapi,traceapi_desc)

	B(no_ldpaths)
	B(no_httpd)
	B(background)
#if defined(WITH_MONITORING_OPTION)
	B(monitoring)
#endif
	B(random_token)

#undef E
#undef B
#undef D
#undef L
#undef V
#undef S

#undef RB
#undef RI
#undef RA
#undef RS
#undef RS
#undef AS
#undef AO

#undef YS
#undef XO
#undef XA

	return r;
}

