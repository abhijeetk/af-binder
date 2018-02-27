/*
 * Copyright (C) 2016, 2017, 2018 "IoT.bzh"
 * Author: José Bollo <jose.bollo@iot.bzh>
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

struct afb_event;
struct afb_eventid;
struct afb_evtid;
struct afb_session;
struct json_object;
struct afb_evt_listener;

struct afb_evt_itf
{
	void (*push)(void *closure, const char *event, int evtid, struct json_object *object);
	void (*broadcast)(void *closure, const char *event, int evtid, struct json_object *object);
	void (*add)(void *closure, const char *event, int evtid);
	void (*remove)(void *closure, const char *event, int evtid);
};

extern struct afb_evt_listener *afb_evt_listener_create(const struct afb_evt_itf *itf, void *closure);

extern int afb_evt_broadcast(const char *event, struct json_object *object);

extern struct afb_evt_listener *afb_evt_listener_addref(struct afb_evt_listener *listener);
extern void afb_evt_listener_unref(struct afb_evt_listener *listener);

extern struct afb_evtid *afb_evt_evtid_create(const char *fullname);
extern struct afb_evtid *afb_evt_evtid_create2(const char *prefix, const char *name);

extern struct afb_evtid *afb_evt_evtid_addref(struct afb_evtid *evtid);
extern struct afb_evtid *afb_evt_evtid_hooked_addref(struct afb_evtid *evtid);

extern void afb_evt_evtid_unref(struct afb_evtid *evtid);
extern void afb_evt_evtid_hooked_unref(struct afb_evtid *evtid);

extern const char *afb_evt_evtid_fullname(struct afb_evtid *evtid);
extern int afb_evt_evtid_id(struct afb_evtid *evtid);

extern const char *afb_evt_evtid_name(struct afb_evtid *evtid);
extern const char *afb_evt_evtid_hooked_name(struct afb_evtid *evtid);

extern int afb_evt_evtid_push(struct afb_evtid *evtid, struct json_object *obj);
extern int afb_evt_evtid_hooked_push(struct afb_evtid *evtid, struct json_object *obj);

extern int afb_evt_evtid_broadcast(struct afb_evtid *evtid, struct json_object *object);
extern int afb_evt_evtid_hooked_broadcast(struct afb_evtid *evtid, struct json_object *object);

extern int afb_evt_watch_add_evtid(struct afb_evt_listener *listener, struct afb_evtid *evtid);
extern int afb_evt_watch_sub_evtid(struct afb_evt_listener *listener, struct afb_evtid *evtid);

extern void afb_evt_update_hooks();


extern struct afb_eventid *afb_evt_eventid_create(const char *fullname);
extern struct afb_eventid *afb_evt_eventid_create2(const char *prefix, const char *name);
extern const char *afb_evt_eventid_fullname(struct afb_eventid *eventid);
extern int afb_evt_eventid_id(struct afb_eventid *eventid);
extern struct afb_eventid *afb_evt_eventid_addref(struct afb_eventid *eventid);
extern void afb_evt_eventid_unref(struct afb_eventid *eventid);

extern int afb_evt_eventid_push(struct afb_eventid *eventid, struct json_object *object);
extern int afb_evt_eventid_unhooked_push(struct afb_eventid *eventid, struct json_object *object);

extern int afb_evt_eventid_add_watch(struct afb_evt_listener *listener, struct afb_eventid *eventid);
extern int afb_evt_eventid_remove_watch(struct afb_evt_listener *listener, struct afb_eventid *eventid);

extern struct afb_evtid *afb_evt_eventid_to_evtid(struct afb_eventid *eventid);
extern struct afb_eventid *afb_evt_eventid_from_evtid(struct afb_evtid *evtid);
extern struct afb_event afb_evt_event_from_evtid(struct afb_evtid *evtid);

