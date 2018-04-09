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
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct event
{
        struct event *next;
        afb_event_t event;
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
        afb_event_unref(e->event);
        free(e);
        return 0;
}

/* creates the event of tag */
static int event_add(afb_api_t api, const char *tag, const char *name)
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
        e->event = afb_api_make_event(api, name);
        if (!e->event) { free(e); return -1; }

        /* link */
        e->next = events;
        events = e;
        return 0;
}

static int event_subscribe(afb_req_t request, const char *tag)
{
        struct event *e;
        e = event_get(tag);
        return e ? afb_req_subscribe(request, e->event) : -1;
}

static int event_unsubscribe(afb_req_t request, const char *tag)
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

static int event_broadcast(struct json_object *args, const char *tag)
{
        struct event *e;
        e = event_get(tag);
        return e ? afb_event_broadcast(e->event, json_object_get(args)) : -1;
}

// Sample Generic Ping Debug API
static void ping(afb_req_t request, json_object *jresp, const char *tag)
{
        static int pingcount = 0;
        json_object *query = afb_req_json(request);
        afb_req_success_f(request, jresp, "Ping Binder Daemon tag=%s count=%d query=%s", tag, ++pingcount, json_object_to_json_string(query));
}

static void pingSample (afb_req_t request)
{
        ping(request, json_object_new_string ("Some String"), "pingSample");
}

static void pingFail (afb_req_t request)
{
        afb_req_fail(request, "failed", "Ping Binder Daemon fails");
}

static void pingNull (afb_req_t request)
{
        ping(request, NULL, "pingNull");
}

static void pingBug (afb_req_t request)
{
        ping(NULL, NULL, "pingBug");
}

static void pingEvent(afb_req_t request)
{
        json_object *query = afb_req_json(request);
        afb_api_broadcast_event(request->api, "event", json_object_get(query));
        ping(request, json_object_get(query), "event");
}


// For samples https://linuxprograms.wordpress.com/2010/05/20/json-c-libjson-tutorial/
static void pingJson (afb_req_t request) {
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

static void subcallcb (void *closure, json_object *object, const char *error, const char *info, afb_req_t request)
{
        afb_req_reply(request, json_object_get(object), error, info);
}

static void subcall (afb_req_t request)
{
        const char *api = afb_req_value(request, "api");
        const char *verb = afb_req_value(request, "verb");
        const char *args = afb_req_value(request, "args");
        json_object *object = api && verb && args ? json_tokener_parse(args) : NULL;

        if (object == NULL)
                afb_req_fail(request, "failed", "bad arguments");
        else
                afb_req_subcall(request, api, verb, object, afb_req_subcall_pass_events, subcallcb, NULL);
}

static void subcallsync (afb_req_t request)
{
        int rc;
        const char *api = afb_req_value(request, "api");
        const char *verb = afb_req_value(request, "verb");
        const char *args = afb_req_value(request, "args");
	json_object *oargs = api && verb && args ? json_tokener_parse(args) : NULL;
        json_object *object;
        char *error, *info;

        if (oargs == NULL)
                afb_req_fail(request, "failed", "bad arguments");
        else {
                rc = afb_req_subcall_sync(request, api, verb, oargs, afb_req_subcall_pass_events, &object, &error, &info);
                afb_req_reply_f(request, object, error, "rc=%d, info=%s", rc, info?:"NULL");
		free(error);
		free(info);
        }
}

static void eventadd (afb_req_t request)
{
        const char *tag = afb_req_value(request, "tag");
        const char *name = afb_req_value(request, "name");

        pthread_mutex_lock(&mutex);
        if (tag == NULL || name == NULL)
                afb_req_fail(request, "failed", "bad arguments");
        else if (0 != event_add(request->api, tag, name))
                afb_req_fail(request, "failed", "creation error");
        else
                afb_req_success(request, NULL, NULL);
        pthread_mutex_unlock(&mutex);
}

static void eventdel (afb_req_t request)
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

static void eventsub (afb_req_t request)
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

static void eventunsub (afb_req_t request)
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

static void eventpush (afb_req_t request)
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

static void callcb (void *prequest, json_object *object, const char *error, const char *info, afb_api_t api)
{
        afb_req_t request = prequest;
	afb_req_reply(request, json_object_get(object), error, info);
        afb_req_unref(request);
}

static void call (afb_req_t request)
{
        const char *api = afb_req_value(request, "api");
        const char *verb = afb_req_value(request, "verb");
        const char *args = afb_req_value(request, "args");
        json_object *object = api && verb && args ? json_tokener_parse(args) : NULL;

        if (object == NULL)
                afb_req_fail(request, "failed", "bad arguments");
        else
                afb_api_call(request->api, api, verb, object, callcb, afb_req_addref(request));
}

static void callsync (afb_req_t request)
{
        int rc;
        const char *api = afb_req_value(request, "api");
        const char *verb = afb_req_value(request, "verb");
        const char *args = afb_req_value(request, "args");
	json_object *oargs = api && verb && args ? json_tokener_parse(args) : NULL;
        json_object *object;
        char *error, *info;

        if (oargs == NULL)
                afb_req_fail(request, "failed", "bad arguments");
        else {
                rc = afb_api_call_sync(request->api, api, verb, oargs, &object, &error, &info);
                afb_req_reply_f(request, object, error, "rc=%d, info=%s", rc, info);
		free(error);
		free(info);
        }
}

static void verbose (afb_req_t request)
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

static void exitnow (afb_req_t request)
{
        int code = 0;
        json_object *query = afb_req_json(request), *l;

        if (json_object_is_type(query,json_type_int))
                code = json_object_get_int(query);
        else if (json_object_object_get_ex(query, "code", &l) && json_object_is_type(l, json_type_int))
                code = json_object_get_int(l);

        if (!json_object_object_get_ex(query,"reason",&l))
                l = NULL;

        AFB_REQ_NOTICE(request, "in phase of exiting with code %d, reason: %s", code, l ? json_object_get_string(l) : "unknown");
        afb_req_success(request, NULL, NULL);
        exit(code);
}

static void broadcast(afb_req_t request)
{
        const char *tag = afb_req_value(request, "tag");
        const char *name = afb_req_value(request, "name");
        const char *data = afb_req_value(request, "data");
        json_object *object = data ? json_tokener_parse(data) : NULL;

        if (tag != NULL) {
                pthread_mutex_lock(&mutex);
                if (0 > event_broadcast(object, tag))
                        afb_req_fail(request, "failed", "broadcast error");
                else
                        afb_req_success(request, NULL, NULL);
                pthread_mutex_unlock(&mutex);
        } else if (name != NULL) {
                if (0 > afb_api_broadcast_event(request->api, name, object))
                        afb_req_fail(request, "failed", "broadcast error");
                else
                        afb_req_success(request, NULL, NULL);
        } else {
                afb_req_fail(request, "failed", "bad arguments");
        }
        json_object_put(object);
}

static void hasperm (afb_req_t request)
{
        const char *perm = afb_req_value(request, "perm");
        if (afb_req_has_permission(request, perm))
                afb_req_success_f(request, NULL, "permission %s granted", perm?:"(null)");
        else
                afb_req_fail_f(request, "not-granted", "permission %s NOT granted", perm?:"(null)");
}

static void appid (afb_req_t request)
{
        char *aid = afb_req_get_application_id(request);
        afb_req_success_f(request, aid ? json_object_new_string(aid) : NULL, "application is %s", aid?:"?");
        free(aid);
}

static int init(afb_api_t api)
{
        AFB_API_NOTICE(api, "dynamic binding AVE(%s) starting", (const char*)api->userdata);
        return 0;
}

static void onevent(afb_api_t api, const char *event, struct json_object *object)
{
        AFB_API_NOTICE(api, "received event %s(%s) by AVE(%s)",
                        event, json_object_to_json_string(object),
                        (const char*)afb_api_get_userdata(api));
}

// NOTE: this sample does not use session to keep test a basic as possible
//       in real application most APIs should be protected with AFB_SESSION_CHECK
static const struct {
        const char *verb;
        void (*callback)(afb_req_t); } verbs[] =
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

static void pingoo(struct afb_req_x1 req)
{
        json_object *args = afb_req_x1_json(req);
        afb_req_x1_reply_f(req, json_object_get(args), NULL, "You reached pingoo \\o/ nice args: %s", json_object_to_json_string(args));
}

static const struct afb_verb_v2 verbsv2[]= {
  { .verb="pingoo",    .callback=pingoo },
  { .verb="ping",      .callback=pingoo },
  { .verb=NULL}
};

static const char *apis[] = { "ave3", "hi3", "salut3", NULL };

static int build_api(void *closure, afb_api_t api)
{
        int i, rc;

        afb_api_set_userdata(api, closure);
        AFB_API_NOTICE(api, "dynamic binding AVE(%s) comes to live", (const char*)afb_api_get_userdata(api));
        afb_api_on_init(api, init);
        afb_api_on_event(api, onevent);

        rc = afb_api_set_verbs_v2(api, verbsv2);
        for (i = rc = 0; verbs[i].verb && rc >= 0 ; i++) {
                rc = afb_api_add_verb(api, verbs[i].verb, NULL, verbs[i].callback, (void*)(intptr_t)i, NULL, 0, 0);
        }
        afb_api_seal(api);
        return rc;
}

int afbBindingV3entry(afb_api_t api)
{
        int i;
	afb_api_t napi;

        for (i = 0; apis[i] ; i++) {
                napi = afb_api_new_api(api, apis[i], NULL, 0, build_api, (void*)apis[i]);
                if (!napi)
                        AFB_API_ERROR(api, "can't create API %s", apis[i]);
        }
        return 0;
}

