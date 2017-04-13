/*
 * Copyright (C) 2017 "IoT.bzh"
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

#pragma once


#define NO_BINDING_VERBOSE_MACRO
#include <afb/afb-binding.h>
#include "afb-context.h"
#include "afb-evt.h"

struct json_object;
struct afb_evt_listener;
struct afb_xreq;
struct afb_cred;
struct afb_apiset;

struct afb_xreq_query_itf {
	struct json_object *(*json)(struct afb_xreq *xreq);
	struct afb_arg (*get)(struct afb_xreq *xreq, const char *name);
	void (*success)(struct afb_xreq *xreq, struct json_object *obj, const char *info);
	void (*fail)(struct afb_xreq *xreq, const char *status, const char *info);
	void (*reply)(struct afb_xreq *xreq, int iserror, struct json_object *obj);
	void (*unref)(struct afb_xreq *xreq);
	int (*subscribe)(struct afb_xreq *xreq, struct afb_event event);
	int (*unsubscribe)(struct afb_xreq *xreq, struct afb_event event);
	void (*subcall)(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure);
};

/**
 * Internal data for requests
 */
struct afb_xreq
{
	struct afb_context context; /**< context of the request */
	struct afb_apiset *apiset; /**< apiset of the xreq */
	const char *api;	/**< the requested API */
	const char *verb;	/**< the requested VERB */
	struct json_object *json; /**< the json object (or NULL) */
	const struct afb_xreq_query_itf *queryitf;
	int refcount;	/**< current ref count */
	int replied;	/**< is replied? */
	int hookflags;	/**< flags for hooking */
	int hookindex;	/**< index for hooking */
	struct afb_evt_listener *listener;
	struct afb_cred *cred;
};

#define CONTAINER_OF_XREQ(type,x) ((type*)(((intptr_t)(x))-((intptr_t)&(((type*)NULL)->xreq))))

extern void afb_xreq_addref(struct afb_xreq *xreq);
extern void afb_xreq_unref(struct afb_xreq *xreq);
extern void afb_xreq_success(struct afb_xreq *xreq, struct json_object *obj, const char *info);
extern void afb_xreq_fail(struct afb_xreq *xreq, const char *status, const char *info);
extern void afb_xreq_fail_f(struct afb_xreq *xreq, const char *status, const char *info, ...);
extern void afb_xreq_success_f(struct afb_xreq *xreq, struct json_object *obj, const char *info, ...);
extern const char *afb_xreq_raw(struct afb_xreq *xreq, size_t *size);
extern int afb_xreq_subscribe(struct afb_xreq *xreq, struct afb_event event);
extern int afb_xreq_unsubscribe(struct afb_xreq *xreq, struct afb_event event);
extern void afb_xreq_subcall(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure);
extern void afb_xreq_unhooked_subcall(struct afb_xreq *xreq, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure);

extern void afb_xreq_init(struct afb_xreq *xreq, const struct afb_xreq_query_itf *queryitf);
extern void afb_xreq_begin(struct afb_xreq *xreq);
extern void afb_xreq_so_call(struct afb_xreq *xreq, int sessionflags, void (*callback)(struct afb_req req));

extern void afb_xreq_process(struct afb_xreq *xreq, struct afb_apiset *apiset);
