/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
 * Author "Fulup Ar Foll"
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
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

const struct afb_binding_interface *interface;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct event
{
	struct event *next;
	struct afb_event event;
	char tag[1];
};

static struct event *events = 0;

/* searchs the event of tag */
static struct event *event_get(const char *tag)
{
	struct event *e = events;
	while(e && strcmp(e->tag, tag))
		e = e->next;
	return e;
}

/* deletes the event of tag */
static int event_del(const char *tag)
{
	struct event *e, **p;

	/* check exists */
	e = event_get(tag);
	if (!e) return -1;

	/* unlink */
	p = &events;
	while(*p != e) p = &(*p)->next;
	*p = e->next;

	/* destroys */
	afb_event_drop(e->event);
	free(e);
	return 0;
}

/* creates the event of tag */
static int event_add(const char *tag, const char *name)
{
	struct event *e;

	/* check valid tag */
	e = event_get(tag);
	if (e) return -1;

	/* creation */
	e = malloc(strlen(tag) + sizeof *e);
	if (!e) return -1;
	strcpy(e->tag, tag);

	/* make the event */
	e->event = afb_daemon_make_event(name);
	if (!e->event.closure) { free(e); return -1; }

	/* link */
	e->next = events;
	events = e;
	return 0;
}

static int event_subscribe(struct afb_req request, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_req_subscribe(request, e->event) : -1;
}

static int event_unsubscribe(struct afb_req request, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_req_unsubscribe(request, e->event) : -1;
}

static int event_push(struct json_object *args, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_event_push(e->event, json_object_get(args)) : -1;
}

// Sample Generic Ping Debug API
static void ping(struct afb_req request, json_object *jresp, const char *tag)
{
	static int pingcount = 0;
	json_object *query = afb_req_json(request);
	afb_req_success_f(request, jresp, "Ping Binder Daemon tag=%s count=%d query=%s", tag, ++pingcount, json_object_to_json_string(query));
}

static void pingSample (struct afb_req request)
{
	ping(request, json_object_new_string ("Some String"), "pingSample");
}

static void pingFail (struct afb_req request)
{
	afb_req_fail(request, "failed", "Ping Binder Daemon fails");
}

static void pingNull (struct afb_req request)
{
	ping(request, NULL, "pingNull");
}

static void pingBug (struct afb_req request)
{
	ping((struct afb_req){NULL,NULL}, NULL, "pingBug");
}

static void pingEvent(struct afb_req request)
{
	json_object *query = afb_req_json(request);
	afb_daemon_broadcast_event("event", json_object_get(query));
	ping(request, json_object_get(query), "event");
}


// For samples https://linuxprograms.wordpress.com/2010/05/20/json-c-libjson-tutorial/
static void pingJson (struct afb_req request) {
    json_object *jresp, *embed;

    jresp = json_object_new_object();
    json_object_object_add(jresp, "myString", json_object_new_string ("Some String"));
    json_object_object_add(jresp, "myInt", json_object_new_int (1234));

    embed  = json_object_new_object();
    json_object_object_add(embed, "subObjString", json_object_new_string ("Some String"));
    json_object_object_add(embed, "subObjInt", json_object_new_int (5678));

    json_object_object_add(jresp,"eobj", embed);

    ping(request, jresp, "pingJson");
}

static void subcallcb (void *prequest, int iserror, json_object *object)
{
	struct afb_req request = afb_req_unstore(prequest);
	if (iserror)
		afb_req_fail(request, "failed", json_object_to_json_string(object));
	else
		afb_req_success(request, json_object_get(object), NULL);
	afb_req_unref(request);
}

static void subcall (struct afb_req request)
{
	const char *api = afb_req_value(request, "api");
	const char *verb = afb_req_value(request, "verb");
	const char *args = afb_req_value(request, "args");
	json_object *object = api && verb && args ? json_tokener_parse(args) : NULL;

	if (object == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else
		afb_req_subcall(request, api, verb, object, subcallcb, afb_req_store(request));
}

static void subcallsync (struct afb_req request)
{
	int rc;
	const char *api = afb_req_value(request, "api");
	const char *verb = afb_req_value(request, "verb");
	const char *args = afb_req_value(request, "args");
	json_object *result, *object = api && verb && args ? json_tokener_parse(args) : NULL;

	if (object == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else {
		rc = afb_req_subcall_sync(request, api, verb, object, &result);
		if (rc)
			afb_req_success(request, result, NULL);
		else {
			afb_req_fail(request, "failed", json_object_to_json_string(result));
			json_object_put(result);
		}
	}
}

static void eventadd (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");
	const char *name = afb_req_value(request, "name");

	pthread_mutex_lock(&mutex);
	if (tag == NULL || name == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 != event_add(tag, name))
		afb_req_fail(request, "failed", "creation error");
	else
		afb_req_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
}

static void eventdel (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");

	pthread_mutex_lock(&mutex);
	if (tag == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 != event_del(tag))
		afb_req_fail(request, "failed", "deletion error");
	else
		afb_req_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
}

static void eventsub (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");

	pthread_mutex_lock(&mutex);
	if (tag == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 != event_subscribe(request, tag))
		afb_req_fail(request, "failed", "subscription error");
	else
		afb_req_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
}

static void eventunsub (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");

	pthread_mutex_lock(&mutex);
	if (tag == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 != event_unsubscribe(request, tag))
		afb_req_fail(request, "failed", "unsubscription error");
	else
		afb_req_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
}

static void eventpush (struct afb_req request)
{
	const char *tag = afb_req_value(request, "tag");
	const char *data = afb_req_value(request, "data");
	json_object *object = data ? json_tokener_parse(data) : NULL;

	pthread_mutex_lock(&mutex);
	if (tag == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else if (0 > event_push(object, tag))
		afb_req_fail(request, "failed", "push error");
	else
		afb_req_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
	json_object_put(object);
}

static void callcb (void *prequest, int iserror, json_object *object)
{
	struct afb_req request = afb_req_unstore(prequest);
	if (iserror)
		afb_req_fail(request, "failed", json_object_to_json_string(object));
	else
		afb_req_success(request, json_object_get(object), NULL);
	afb_req_unref(request);
}

static void call (struct afb_req request)
{
	const char *api = afb_req_value(request, "api");
	const char *verb = afb_req_value(request, "verb");
	const char *args = afb_req_value(request, "args");
	json_object *object = api && verb && args ? json_tokener_parse(args) : NULL;

	if (object == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else
		afb_service_call(api, verb, object, callcb, afb_req_store(request));
}

static void callsync (struct afb_req request)
{
	int rc;
	const char *api = afb_req_value(request, "api");
	const char *verb = afb_req_value(request, "verb");
	const char *args = afb_req_value(request, "args");
	json_object *result, *object = api && verb && args ? json_tokener_parse(args) : NULL;

	if (object == NULL)
		afb_req_fail(request, "failed", "bad arguments");
	else {
		rc = afb_service_call_sync(api, verb, object, &result);
		if (rc)
			afb_req_success(request, result, NULL);
		else {
			afb_req_fail(request, "failed", json_object_to_json_string(result));
			json_object_put(result);
		}
	}
}

static void verbose (struct afb_req request)
{
	int level = 5;
	json_object *query = afb_req_json(request), *l;

	if (json_object_is_type(query,json_type_int))
		level = json_object_get_int(query);
	else if (json_object_object_get_ex(query, "level", &l) && json_object_is_type(l, json_type_int))
		level = json_object_get_int(l);

	if (!json_object_object_get_ex(query,"message",&l))
		l = query;

	AFB_REQ_VERBOSE(request, level, "verbose called for %s", json_object_get_string(l));
	afb_req_success(request, NULL, NULL);
}

static void exitnow (struct afb_req request)
{
	int code = 0;
	json_object *query = afb_req_json(request), *l;

	if (json_object_is_type(query,json_type_int))
		code = json_object_get_int(query);
	else if (json_object_object_get_ex(query, "code", &l) && json_object_is_type(l, json_type_int))
		code = json_object_get_int(l);

	if (!json_object_object_get_ex(query,"reason",&l))
		l = NULL;

	REQ_NOTICE(request, "in phase of exiting with code %d, reason: %s", code, l ? json_object_get_string(l) : "unknown");
	afb_req_success(request, NULL, NULL);
	exit(code);
}

static int preinit()
{
	NOTICE("hello binding comes to live");
	return 0;
}

static int init()
{
	NOTICE("hello binding starting");
	return 0;
}

static void onevent(const char *event, struct json_object *object)
{
	NOTICE("received event %s(%s)", event, json_object_to_json_string(object));
}

// NOTE: this sample does not use session to keep test a basic as possible
//       in real application most APIs should be protected with AFB_SESSION_CHECK
static const struct afb_verb_v2 verbs[]= {
  { "ping"     ,    pingSample , NULL, AFB_SESSION_NONE },
  { "pingfail" ,    pingFail   , NULL, AFB_SESSION_NONE },
  { "pingnull" ,    pingNull   , NULL, AFB_SESSION_NONE },
  { "pingbug"  ,    pingBug    , NULL, AFB_SESSION_NONE },
  { "pingJson" ,    pingJson   , NULL, AFB_SESSION_NONE },
  { "pingevent",    pingEvent  , NULL, AFB_SESSION_NONE },
  { "subcall",      subcall    , NULL, AFB_SESSION_NONE },
  { "subcallsync",  subcallsync, NULL, AFB_SESSION_NONE },
  { "eventadd",     eventadd   , NULL, AFB_SESSION_NONE },
  { "eventdel",     eventdel   , NULL, AFB_SESSION_NONE },
  { "eventsub",     eventsub   , NULL, AFB_SESSION_NONE },
  { "eventunsub",   eventunsub , NULL, AFB_SESSION_NONE },
  { "eventpush",    eventpush  , NULL, AFB_SESSION_NONE },
  { "call",         call       , NULL, AFB_SESSION_NONE },
  { "callsync",     callsync   , NULL, AFB_SESSION_NONE },
  { "verbose",      verbose    , NULL, AFB_SESSION_NONE },
  { "exit",         exitnow    , NULL, AFB_SESSION_NONE },
  { NULL}
};

const struct afb_binding_v2 afbBindingV2 = {
	.api = "hello",
	.specification = NULL,
	.verbs = verbs,
	.preinit = preinit,
	.init = init,
	.onevent = onevent
};

