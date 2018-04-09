/*
 * Copyright (C) 2015-2018 "IoT.bzh"
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

struct json_object;

/*
 * other definitions ---------------------------------------------------
 */

/**
 * list of configuration values
 */
struct afb_config_list {
	struct afb_config_list *next;
	char *value;
};

/**
 * main config structure
 */
struct afb_config {
	char *console;		/*< console device name (can be a file or a tty) */
	char *rootdir;		/*< base dir for files */
	char *roothttp;		/*< directory for http files */
	char *rootbase;		/*< Angular HTML5 base URL */
	char *rootapi;		/*< Base URL for REST APIs */
	char *workdir;		/*< where to run the program */
	char *uploaddir;	/*< where to store transient files */
	char *token;		/*< initial authentication token [default NULL no session] */
	char *name;		/*< name to set to the daemon */

	struct afb_config_list *aliases;
#if defined(WITH_DBUS_TRANSPARENCY)
	struct afb_config_list *dbus_clients;
	struct afb_config_list *dbus_servers;
#endif
	struct afb_config_list *ws_clients;
	struct afb_config_list *ws_servers;
	struct afb_config_list *so_bindings;
	struct afb_config_list *ldpaths;
	struct afb_config_list *weak_ldpaths;
	struct afb_config_list *calls;
	struct afb_config_list *auto_ws;
	struct afb_config_list *auto_link;

	char **exec;

	/* integers */
	int http_port;
	int cache_timeout;
	int api_timeout;
	int session_timeout;	/*< session timeout */
	int max_session_count;	/*< max count of sessions */

	/* enums */
#if defined(KEEP_LEGACY_MODE)
	int mode;		// mode of listening
#endif
	int tracereq;
#if !defined(REMOVE_LEGACY_TRACE)
	int traceditf;
	int tracesvc;
#endif
	int traceevt;
	int traceses;
	int traceapi;

	/* booleans */
	unsigned no_ldpaths: 1;		/**< disable default ldpaths */
	unsigned no_httpd: 1;
	unsigned background: 1;		/**< run in backround mode */
	unsigned random_token: 1;	/**< expects a random token */
#if defined(WITH_MONITORING_OPTION)
	unsigned monitoring: 1;		/**< activates monitoring */
#endif
};

extern struct afb_config *afb_config_parse_arguments(int argc, char **argv);
extern void afb_config_dump(struct afb_config *config);
extern struct json_object *afb_config_json(struct afb_config *config);

