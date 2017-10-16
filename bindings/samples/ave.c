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
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 0
#include <afb/afb-binding.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct event
{
	struct event *next;
	afb_eventid *eventid;
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
	afb_eventid_unref(e->eventid);
	free(e);
	return 0;
}

/* creates the event of tag */
static int event_add(afb_dynapi *dynapi, const char *tag, const char *name)
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
	e->eventid = afb_dynapi_make_eventid(dynapi, name);
	if (!e->eventid) { free(e); return -1; }

	/* link */
	e->next = events;
	events = e;
	return 0;
}

static int event_subscribe(afb_request *request, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_request_subscribe(request, e->eventid) : -1;
}

static int event_unsubscribe(afb_request *request, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_request_unsubscribe(request, e->eventid) : -1;
}

static int event_push(struct json_object *args, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_eventid_push(e->eventid, json_object_get(args)) : -1;
}

static int event_broadcast(struct json_object *args, const char *tag)
{
	struct event *e;
	e = event_get(tag);
	return e ? afb_eventid_broadcast(e->eventid, json_object_get(args)) : -1;
}

// Sample Generic Ping Debug API
static void ping(afb_request *request, json_object *jresp, const char *tag)
{
	static int pingcount = 0;
	json_object *query = afb_request_json(request);
	afb_request_success_f(request, jresp, "Ping Binder Daemon tag=%s count=%d query=%s", tag, ++pingcount, json_object_to_json_string(query));
}

static void pingSample (afb_request *request)
{
	ping(request, json_object_new_string ("Some String"), "pingSample");
}

static void pingFail (afb_request *request)
{
	afb_request_fail(request, "failed", "Ping Binder Daemon fails");
}

static void pingNull (afb_request *request)
{
	ping(request, NULL, "pingNull");
}

static void pingBug (afb_request *request)
{
	ping(NULL, NULL, "pingBug");
}

static void pingEvent(afb_request *request)
{
	json_object *query = afb_request_json(request);
	afb_dynapi_broadcast_event(request->dynapi, "event", json_object_get(query));
	ping(request, json_object_get(query), "event");
}


// For samples https://linuxprograms.wordpress.com/2010/05/20/json-c-libjson-tutorial/
static void pingJson (afb_request *request) {
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

static void subcallcb (void *closure, int status, json_object *object, afb_request *request)
{
	if (status < 0)
		afb_request_fail(request, "failed", json_object_to_json_string(object));
	else
		afb_request_success(request, json_object_get(object), NULL);
}

static void subcall (afb_request *request)
{
	const char *api = afb_request_value(request, "api");
	const char *verb = afb_request_value(request, "verb");
	const char *args = afb_request_value(request, "args");
	json_object *object = api && verb && args ? json_tokener_parse(args) : NULL;

	if (object == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else
		afb_request_subcall(request, api, verb, object, subcallcb, NULL);
}

static void subcallsync (afb_request *request)
{
	int rc;
	const char *api = afb_request_value(request, "api");
	const char *verb = afb_request_value(request, "verb");
	const char *args = afb_request_value(request, "args");
	json_object *result, *object = api && verb && args ? json_tokener_parse(args) : NULL;

	if (object == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else {
		rc = afb_request_subcall_sync(request, api, verb, object, &result);
		if (rc >= 0)
			afb_request_success(request, result, NULL);
		else {
			afb_request_fail(request, "failed", json_object_to_json_string(result));
			json_object_put(result);
		}
	}
}

static void eventadd (afb_request *request)
{
	const char *tag = afb_request_value(request, "tag");
	const char *name = afb_request_value(request, "name");

	pthread_mutex_lock(&mutex);
	if (tag == NULL || name == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else if (0 != event_add(request->dynapi, tag, name))
		afb_request_fail(request, "failed", "creation error");
	else
		afb_request_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
}

static void eventdel (afb_request *request)
{
	const char *tag = afb_request_value(request, "tag");

	pthread_mutex_lock(&mutex);
	if (tag == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else if (0 != event_del(tag))
		afb_request_fail(request, "failed", "deletion error");
	else
		afb_request_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
}

static void eventsub (afb_request *request)
{
	const char *tag = afb_request_value(request, "tag");

	pthread_mutex_lock(&mutex);
	if (tag == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else if (0 != event_subscribe(request, tag))
		afb_request_fail(request, "failed", "subscription error");
	else
		afb_request_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
}

static void eventunsub (afb_request *request)
{
	const char *tag = afb_request_value(request, "tag");

	pthread_mutex_lock(&mutex);
	if (tag == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else if (0 != event_unsubscribe(request, tag))
		afb_request_fail(request, "failed", "unsubscription error");
	else
		afb_request_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
}

static void eventpush (afb_request *request)
{
	const char *tag = afb_request_value(request, "tag");
	const char *data = afb_request_value(request, "data");
	json_object *object = data ? json_tokener_parse(data) : NULL;

	pthread_mutex_lock(&mutex);
	if (tag == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else if (0 > event_push(object, tag))
		afb_request_fail(request, "failed", "push error");
	else
		afb_request_success(request, NULL, NULL);
	pthread_mutex_unlock(&mutex);
	json_object_put(object);
}

static void callcb (void *prequest, int status, json_object *object, afb_dynapi *dynapi)
{
	afb_request *request = prequest;
	if (status < 0)
		afb_request_fail(request, "failed", json_object_to_json_string(object));
	else
		afb_request_success(request, json_object_get(object), NULL);
	afb_request_unref(request);
}

static void call (afb_request *request)
{
	const char *api = afb_request_value(request, "api");
	const char *verb = afb_request_value(request, "verb");
	const char *args = afb_request_value(request, "args");
	json_object *object = api && verb && args ? json_tokener_parse(args) : NULL;

	if (object == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else
		afb_dynapi_call(request->dynapi, api, verb, object, callcb, afb_request_addref(request));
}

static void callsync (afb_request *request)
{
	int rc;
	const char *api = afb_request_value(request, "api");
	const char *verb = afb_request_value(request, "verb");
	const char *args = afb_request_value(request, "args");
	json_object *result, *object = api && verb && args ? json_tokener_parse(args) : NULL;

	if (object == NULL)
		afb_request_fail(request, "failed", "bad arguments");
	else {
		rc = afb_dynapi_call_sync(request->dynapi, api, verb, object, &result);
		if (rc >= 0)
			afb_request_success(request, result, NULL);
		else {
			afb_request_fail(request, "failed", json_object_to_json_string(result));
			json_object_put(result);
		}
	}
}

static void verbose (afb_request *request)
{
	int level = 5;
	json_object *query = afb_request_json(request), *l;

	if (json_object_is_type(query,json_type_int))
		level = json_object_get_int(query);
	else if (json_object_object_get_ex(query, "level", &l) && json_object_is_type(l, json_type_int))
		level = json_object_get_int(l);

	if (!json_object_object_get_ex(query,"message",&l))
		l = query;

	AFB_REQUEST_VERBOSE(request, level, "verbose called for %s", json_object_get_string(l));
	afb_request_success(request, NULL, NULL);
}

static void exitnow (afb_request *request)
{
	int code = 0;
	json_object *query = afb_request_json(request), *l;

	if (json_object_is_type(query,json_type_int))
		code = json_object_get_int(query);
	else if (json_object_object_get_ex(query, "code", &l) && json_object_is_type(l, json_type_int))
		code = json_object_get_int(l);

	if (!json_object_object_get_ex(query,"reason",&l))
		l = NULL;

	AFB_REQUEST_NOTICE(request, "in phase of exiting with code %d, reason: %s", code, l ? json_object_get_string(l) : "unknown");
	afb_request_success(request, NULL, NULL);
	exit(code);
}

static void broadcast(afb_request *request)
{
	const char *tag = afb_request_value(request, "tag");
	const char *name = afb_request_value(request, "name");
	const char *data = afb_request_value(request, "data");
	json_object *object = data ? json_tokener_parse(data) : NULL;

	if (tag != NULL) {
		pthread_mutex_lock(&mutex);
		if (0 > event_broadcast(object, tag))
			afb_request_fail(request, "failed", "broadcast error");
		else
			afb_request_success(request, NULL, NULL);
		pthread_mutex_unlock(&mutex);
	} else if (name != NULL) {
		if (0 > afb_dynapi_broadcast_event(request->dynapi, name, object))
			afb_request_fail(request, "failed", "broadcast error");
		else
			afb_request_success(request, NULL, NULL);
	} else {
		afb_request_fail(request, "failed", "bad arguments");
	}
	json_object_put(object);
}

static void hasperm (afb_request *request)
{
	const char *perm = afb_request_value(request, "perm");
	if (afb_request_has_permission(request, perm))
		afb_request_success_f(request, NULL, "permission %s granted", perm?:"(null)");
	else
		afb_request_fail_f(request, "not-granted", "permission %s NOT granted", perm?:"(null)");
}

static void appid (afb_request *request)
{
	char *aid = afb_request_get_application_id(request);
	afb_request_success_f(request, aid ? json_object_new_string(aid) : NULL, "application is %s", aid?:"?");
	free(aid);
}

static int init(afb_dynapi *dynapi)
{
	AFB_DYNAPI_NOTICE(dynapi, "dynamic binding AVE(%s) starting", (const char*)dynapi->userdata);
	return 0;
}

static void onevent(afb_dynapi *dynapi, const char *event, struct json_object *object)
{
	AFB_DYNAPI_NOTICE(dynapi, "received event %s(%s) by AVE(%s)",
			event, json_object_to_json_string(object),
			(const char*)afb_dynapi_get_userdata(dynapi));
}

// NOTE: this sample does not use session to keep test a basic as possible
//       in real application most APIs should be protected with AFB_SESSION_CHECK
static const struct {
	const char *verb;
	void (*callback)(afb_request*); } verbs[] =
{
  { .verb="ping",        .callback=pingSample },
  { .verb="pingfail",    .callback=pingFail },
  { .verb="pingnull",    .callback=pingNull },
  { .verb="pingbug",     .callback=pingBug },
  { .verb="pingJson",    .callback=pingJson },
  { .verb="pingevent",   .callback=pingEvent },
  { .verb="subcall",     .callback=subcall },
  { .verb="subcallsync", .callback=subcallsync },
  { .verb="eventadd",    .callback=eventadd },
  { .verb="eventdel",    .callback=eventdel },
  { .verb="eventsub",    .callback=eventsub },
  { .verb="eventunsub",  .callback=eventunsub },
  { .verb="eventpush",   .callback=eventpush },
  { .verb="call",        .callback=call },
  { .verb="callsync",    .callback=callsync },
  { .verb="verbose",     .callback=verbose },
  { .verb="broadcast",   .callback=broadcast },
  { .verb="hasperm",     .callback=hasperm },
  { .verb="appid",       .callback=appid },
  { .verb="exit",        .callback=exitnow },
  { .verb=NULL}
};

static void pingoo(afb_req req)
{
	json_object *args = afb_req_json(req);
	afb_req_success_f(req, json_object_get(args), "You reached pingoo \\o/ nice args: %s", json_object_to_json_string(args));
}

static const afb_verb_v2 verbsv2[]= {
  { .verb="pingoo",      .callback=pingoo },
  { .verb="ping",      .callback=pingoo },
  { .verb=NULL}
};

static const char *apis[] = { "ave", "hi", "salut", NULL };

static int build_api(void *closure, afb_dynapi *dynapi)
{
	int i, rc;

	afb_dynapi_set_userdata(dynapi, closure);
	AFB_DYNAPI_NOTICE(dynapi, "dynamic binding AVE(%s) comes to live", (const char*)afb_dynapi_get_userdata(dynapi));
	afb_dynapi_on_init(dynapi, init);
	afb_dynapi_on_event(dynapi, onevent);

	rc = afb_dynapi_set_verbs_v2(dynapi, verbsv2);
	for (i = rc = 0; verbs[i].verb && rc >= 0 ; i++) {
		rc = afb_dynapi_add_verb(dynapi, verbs[i].verb, NULL, verbs[i].callback, (void*)(intptr_t)i, NULL, 0);
	}
	afb_dynapi_seal(dynapi);
	return rc;
}

int afbBindingVdyn(afb_dynapi *dynapi)
{
	int i, rc;

	for (i = 0; apis[i] ; i++) {
		rc = afb_dynapi_new_api(dynapi, apis[i], NULL, build_api, (void*)apis[i]);
		if (rc < 0)
			AFB_DYNAPI_ERROR(dynapi, "can't create API %s", apis[i]);
	}
	return 0;
}

