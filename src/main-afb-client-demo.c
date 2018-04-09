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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <systemd/sd-event.h>
#include <json-c/json.h>

#include "afb-wsj1.h"
#include "afb-ws-client.h"
#include "afb-proto-ws.h"

/* declaration of functions */
static void on_wsj1_hangup(void *closure, struct afb_wsj1 *wsj1);
static void on_wsj1_call(void *closure, const char *api, const char *verb, struct afb_wsj1_msg *msg);
static void on_wsj1_event(void *closure, const char *event, struct afb_wsj1_msg *msg);

static void on_pws_hangup(void *closure);
static void on_pws_reply(void *closure, void *request, struct json_object *result, const char *error, const char *info);
static void on_pws_event_create(void *closure, const char *event_name, int event_id);
static void on_pws_event_remove(void *closure, const char *event_name, int event_id);
static void on_pws_event_subscribe(void *closure, void *request, const char *event_name, int event_id);
static void on_pws_event_unsubscribe(void *closure, void *request, const char *event_name, int event_id);
static void on_pws_event_push(void *closure, const char *event_name, int event_id, struct json_object *data);
static void on_pws_event_broadcast(void *closure, const char *event_name, struct json_object *data);

static int io_event_callback(sd_event_source *src, int fd, uint32_t revents, void *closure);

static void wsj1_emit(const char *api, const char *verb, const char *object);
static void pws_call(const char *verb, const char *object);

/* the callback interface for wsj1 */
static struct afb_wsj1_itf wsj1_itf = {
	.on_hangup = on_wsj1_hangup,
	.on_call = on_wsj1_call,
	.on_event = on_wsj1_event
};

/* the callback interface for pws */
static struct afb_proto_ws_client_itf pws_itf = {
	.on_reply = on_pws_reply,
	.on_event_create = on_pws_event_create,
	.on_event_remove = on_pws_event_remove,
	.on_event_subscribe = on_pws_event_subscribe,
	.on_event_unsubscribe = on_pws_event_unsubscribe,
	.on_event_push = on_pws_event_push,
	.on_event_broadcast = on_pws_event_broadcast,
};

/* global variables */
static struct afb_wsj1 *wsj1;
static struct afb_proto_ws *pws;
static int breakcon;
static int exonrep;
static int callcount;
static int human;
static int raw;
static int direct;
static int echo;
static sd_event_source *evsrc;
static char *sessionid = "afb-client-demo";

/* print usage of the program */
static void usage(int status, char *arg0)
{
	char *name = strrchr(arg0, '/');
	name = name ? name + 1 : arg0;
	fprintf(status ? stderr : stdout, "usage: %s [-H [-r]] [-b] [-e] uri [api verb [data]]\n", name);
	fprintf(status ? stderr : stdout, "       %s -d [-H [-r]] [-b] [-e] uri [verb [data]]\n", name);
	fprintf(status ? stderr : stdout, "\n"
		"allowed options\n"
		"  --break, -b         Break connection just after event/call has been emitted.\n"
		"  --direct, -d        Direct api\n"
		"  --echo, -e          Echo inputs\n"
		"  --help, -h          Display this help\n"
		"  --human, -H         Display human readable JSON\n"
		"  --raw, -r           Raw output (default)\n"
		"Example:\n"
		" %s --human 'localhost:1234/api?token=HELLO&uuid=magic' hello ping\n"
		"\n", name
	);

	exit(status);
}

/* entry function */
int main(int ac, char **av, char **env)
{
	int rc;
	char *a0;
	sd_event *loop;

	/* get the program name */
	a0 = av[0];

	/* check options */
	while (ac > 1 && av[1][0] == '-') {
		if (av[1][1] == '-') {
			/* long option */

			if (!strcmp(av[1], "--human")) /* request for human output */
				human = 1;

			else if (!strcmp(av[1], "--raw")) /* request for raw output */
				raw = 1;

			else if (!strcmp(av[1], "--direct")) /* request for direct api */
				direct = 1;

			else if (!strcmp(av[1], "--break")) /* request to break connection */
				breakcon = 1;

			else if (!strcmp(av[1], "--echo")) /* request to echo inputs */
				echo = 1;

			/* emit usage and exit */
			else
				usage(!!strcmp(av[1], "--help"), a0);
		} else {
			/* short option(s) */
			for (rc = 1 ; av[1][rc] ; rc++)
				switch (av[1][rc]) {
				case 'H': human = 1; break;
				case 'r': raw = 1; break;
				case 'd': direct = 1; break;
				case 'b': breakcon = 1; break;
				case 'e': echo = 1; break;
				default: usage(av[1][rc] != 'h', a0);
				}
		}
		av++;
		ac--;
	}

	/* check the argument count */
	if (ac != 2 && ac != 4 && ac != 5)
		usage(1, a0);

	/* set raw by default */
	if (!human)
		raw = 1;

	/* get the default event loop */
	rc = sd_event_default(&loop);
	if (rc < 0) {
		fprintf(stderr, "connection to default event loop failed: %s\n", strerror(-rc));
		return 1;
	}

	/* connect the websocket wsj1 to the uri given by the first argument */
	if (direct) {
		pws = afb_ws_client_connect_api(loop, av[1], &pws_itf, NULL);
		if (pws == NULL) {
			fprintf(stderr, "connection to %s failed: %m\n", av[1]);
			return 1;
		}
		afb_proto_ws_on_hangup(pws, on_pws_hangup);
	} else {
		wsj1 = afb_ws_client_connect_wsj1(loop, av[1], &wsj1_itf, NULL);
		if (wsj1 == NULL) {
			fprintf(stderr, "connection to %s failed: %m\n", av[1]);
			return 1;
		}
	}

	/* test the behaviour */
	if (ac == 2) {
		/* get requests from stdin */
		fcntl(0, F_SETFL, O_NONBLOCK);
		sd_event_add_io(loop, &evsrc, 0, EPOLLIN, io_event_callback, NULL);
	} else {
		/* the request is defined by the arguments */
		exonrep = 1;
		if (direct)
			pws_call(av[2], av[3]);
		else
			wsj1_emit(av[2], av[3], av[4]);
	}

	/* loop until end */
	for(;;)
		sd_event_run(loop, 30000000);
	return 0;
}

/* decrement the count of calls */
static void dec_callcount()
{
	callcount--;
	if (exonrep && !callcount)
		exit(0);
}

/* called when wsj1 hangsup */
static void on_wsj1_hangup(void *closure, struct afb_wsj1 *wsj1)
{
	printf("ON-HANGUP\n");
	fflush(stdout);
	exit(0);
}

/* called when wsj1 receives a method invocation */
static void on_wsj1_call(void *closure, const char *api, const char *verb, struct afb_wsj1_msg *msg)
{
	int rc;
	if (raw)
		printf("%s\n", afb_wsj1_msg_object_s(msg));
	if (human)
		printf("ON-CALL %s/%s:\n%s\n", api, verb,
				json_object_to_json_string_ext(afb_wsj1_msg_object_j(msg),
							JSON_C_TO_STRING_PRETTY));
	fflush(stdout);
	rc = afb_wsj1_reply_error_s(msg, "\"unimplemented\"", NULL);
	if (rc < 0)
		fprintf(stderr, "replying failed: %m\n");
}

/* called when wsj1 receives an event */
static void on_wsj1_event(void *closure, const char *event, struct afb_wsj1_msg *msg)
{
	if (raw)
		printf("%s\n", afb_wsj1_msg_object_s(msg));
	if (human)
		printf("ON-EVENT %s:\n%s\n", event,
				json_object_to_json_string_ext(afb_wsj1_msg_object_j(msg),
							JSON_C_TO_STRING_PRETTY));
	fflush(stdout);
}

/* called when wsj1 receives a reply */
static void on_wsj1_reply(void *closure, struct afb_wsj1_msg *msg)
{
	if (raw)
		printf("%s\n", afb_wsj1_msg_object_s(msg));
	if (human)
		printf("ON-REPLY %s: %s\n%s\n", (char*)closure,
				afb_wsj1_msg_is_reply_ok(msg) ? "OK" : "ERROR",
				json_object_to_json_string_ext(afb_wsj1_msg_object_j(msg),
							JSON_C_TO_STRING_PRETTY));
	fflush(stdout);
	free(closure);
	dec_callcount();
}

/* makes a call */
static void wsj1_call(const char *api, const char *verb, const char *object)
{
	static int num = 0;
	char *key;
	int rc;

	/* allocates an id for the request */
	rc = asprintf(&key, "%d:%s/%s", ++num, api, verb);

	/* echo the command if asked */
	if (echo)
		printf("SEND-CALL %s/%s %s\n", api, verb, object?:"null");

	/* send the request */
	callcount++;
	rc = afb_wsj1_call_s(wsj1, api, verb, object, on_wsj1_reply, key);
	if (rc < 0) {
		fprintf(stderr, "calling %s/%s(%s) failed: %m\n", api, verb, object);
		dec_callcount();
	}
}

/* sends an event */
static void wsj1_event(const char *event, const char *object)
{
	int rc;

	/* echo the command if asked */
	if (echo)
		printf("SEND-EVENT: %s %s\n", event, object?:"null");

	rc = afb_wsj1_send_event_s(wsj1, event, object);
	if (rc < 0)
		fprintf(stderr, "sending !%s(%s) failed: %m\n", event, object);
}

/* emits either a call (when api!='!') or an event */
static void wsj1_emit(const char *api, const char *verb, const char *object)
{
	if (object == NULL || object[0] == 0)
		object = "null";

	if (api[0] == '!' && api[1] == 0)
		wsj1_event(verb, object);
	else
		wsj1_call(api, verb, object);
	if (breakcon)
		exit(0);
}

/* called when something happens on stdin */
static int io_event_callback(sd_event_source *src, int fd, uint32_t revents, void *closure)
{
	static size_t count = 0;
	static char line[16384];
	static char sep[] = " \t";
	static char sepnl[] = " \t\n";

	ssize_t rc;
	size_t pos;

	/* read the buffer */
	do { rc = read(0, line + count, sizeof line - count); } while (rc < 0 && errno == EINTR);
	if (rc < 0) {
		fprintf(stderr, "read error: %m\n");
		exit(1);
	}
	if (rc == 0) {
		if (!callcount)
			exit(0);
		exonrep = 1;
		sd_event_source_unref(evsrc);
	}
	count += (size_t)rc;

	/* normalise the buffer content */
	/* TODO: handle backspace \x7f ? */

	/* process the lines */
	pos = 0;
	for(;;) {
		size_t i, api[2], verb[2], rest[2];
		i = pos;
		while(i < count && strchr(sep, line[i])) i++;
		api[0] = i; while(i < count && !strchr(sepnl, line[i])) i++; api[1] = i;
		while(i < count && strchr(sep, line[i])) i++;
		if (direct) {
			verb[0] = api[0];
			verb[1] = api[1];
		} else {
			verb[0] = i; while(i < count && !strchr(sepnl, line[i])) i++; verb[1] = i;
			while(i < count && strchr(sep, line[i])) i++;
		}
		rest[0] = i; while(i < count && line[i] != '\n') i++; rest[1] = i;
		if (i == count) break;
		line[i++] = 0;
		if (api[0] == api[1]) {
			/* empty line */
		} else if (line[api[0]] == '#') {
			/* comment */
		} else if (verb[0] == verb[1]) {
			fprintf(stderr, "verb missing, bad line: %s\n", line+pos);
		} else {
			line[api[1]] = line[verb[1]] = 0;
			if (direct)
				pws_call(line + verb[0], line + rest[0]);
			else
				wsj1_emit(line + api[0], line + verb[0], line + rest[0]);
		}
		pos = i;
	}
	count -= pos;
	if (count == sizeof line) {
		fprintf(stderr, "overflow\n");
		exit(1);
	}
	if (count)
		memmove(line, line + pos, count);
	return 1;
}

static void on_pws_reply(void *closure, void *request, struct json_object *result, const char *error, const char *info)
{
	error = error ?: "success";
	if (raw) {
		/* TODO: transitionnal: fake the structured response */
		struct json_object *x = json_object_new_object(), *y = json_object_new_object();
		json_object_object_add(x, "jtype", json_object_new_string("afb-reply"));
		json_object_object_add(x, "request", y);
		json_object_object_add(y, "status", json_object_new_string(error));
		if (info)
			json_object_object_add(y, "info", json_object_new_string(info));
		if (result)
			json_object_object_add(x, "response", json_object_get(result));

		printf("%s\n", json_object_to_json_string(x));
		json_object_put(x);
	}
	if (human)
		printf("ON-REPLY %s: %s %s\n%s\n", (char*)request, error, info ?: "", json_object_to_json_string_ext(result, JSON_C_TO_STRING_PRETTY));
	fflush(stdout);
	free(request);
	dec_callcount();
}

static void on_pws_event_create(void *closure, const char *event_name, int event_id)
{
	printf("ON-EVENT-CREATE: [%d:%s]\n", event_id, event_name);
	fflush(stdout);
}

static void on_pws_event_remove(void *closure, const char *event_name, int event_id)
{
	printf("ON-EVENT-REMOVE: [%d:%s]\n", event_id, event_name);
	fflush(stdout);
}

static void on_pws_event_subscribe(void *closure, void *request, const char *event_name, int event_id)
{
	printf("ON-EVENT-SUBSCRIBE %s: [%d:%s]\n", (char*)request, event_id, event_name);
	fflush(stdout);
}

static void on_pws_event_unsubscribe(void *closure, void *request, const char *event_name, int event_id)
{
	printf("ON-EVENT-UNSUBSCRIBE %s: [%d:%s]\n", (char*)request, event_id, event_name);
	fflush(stdout);
}

static void on_pws_event_push(void *closure, const char *event_name, int event_id, struct json_object *data)
{
	if (raw)
		printf("ON-EVENT-PUSH: [%d:%s]\n%s\n", event_id, event_name, json_object_to_json_string_ext(data, 0));
	if (human)
		printf("ON-EVENT-PUSH: [%d:%s]\n%s\n", event_id, event_name, json_object_to_json_string_ext(data, JSON_C_TO_STRING_PRETTY));
	fflush(stdout);
}

static void on_pws_event_broadcast(void *closure, const char *event_name, struct json_object *data)
{
	if (raw)
		printf("ON-EVENT-BROADCAST: [%s]\n%s\n", event_name, json_object_to_json_string_ext(data, 0));
	if (human)
		printf("ON-EVENT-BROADCAST: [%s]\n%s\n", event_name, json_object_to_json_string_ext(data, JSON_C_TO_STRING_PRETTY));
	fflush(stdout);
}

/* makes a call */
static void pws_call(const char *verb, const char *object)
{
	static int num = 0;
	char *key;
	int rc;
	struct json_object *o;

	/* allocates an id for the request */
	rc = asprintf(&key, "%d:%s", ++num, verb);

	/* echo the command if asked */
	if (echo)
		printf("SEND-CALL: %s %s\n", verb, object?:"null");

	/* send the request */
	callcount++;
	if (object == NULL || object[0] == 0 || !strcmp(object, "null"))
		o = NULL;
	else {
		o = json_tokener_parse(object);
		if (!o)
			o = json_object_new_string(object);
	}
	rc = afb_proto_ws_client_call(pws, verb, o, sessionid, key, NULL);
	json_object_put(o);
	if (rc < 0) {
		fprintf(stderr, "calling %s(%s) failed: %m\n", verb, object?:"");
		dec_callcount();
	}
	if (breakcon)
		exit(0);
}

/* called when pws hangsup */
static void on_pws_hangup(void *closure)
{
	printf("ON-HANGUP\n");
	fflush(stdout);
	exit(0);
}


