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

#include "afb-context.h"

struct json_object;
struct afb_evt_listener;
struct afb_xreq;
struct afb_cred;
struct afb_apiset;
struct afb_event;
struct afb_verb_desc_v1;
struct afb_verb_v2;
struct afb_req;
struct afb_req_itf;
struct afb_request;
struct afb_stored_req;

struct afb_xreq_query_itf {
	struct json_object *(*json)(struct afb_xreq *xreq);
	struct afb_arg (*get)(struct afb_xreq *xreq, const char *name);
	void (*success)(struct afb_xreq *xreq, struct json_object *obj, const char *info);
	void (*fail)(struct afb_xreq *xreq, const char *status, const char *info);
	void (*reply)(struct afb_xreq *xreq, int status, struct json_object *obj);
	void (*unref)(struct afb_xreq *xreq);
	int (*subscribe)(struct afb_xreq *xreq, struct afb_event event);
	int (*unsubscribe)(struct afb_xreq *xreq, struct afb_event event);
	void (*subcall)(
		struct afb_xreq *xreq,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*),
		void *cb_closure);
};

/**
 * Internal data for requests
 */
struct afb_xreq
{
	const struct afb_req_itf *itf;	/**< interface functions */
	struct afb_context context;	/**< context of the request */
	struct afb_apiset *apiset;	/**< apiset of the xreq */
	const char *api;		/**< the requested API */
	const char *verb;		/**< the requested VERB */
	struct json_object *json;	/**< the json object (or NULL) */
	const struct afb_xreq_query_itf *queryitf; /**< interface of xreq implmentation functions */
	int refcount;			/**< current ref count */
	int replied;			/**< is replied? */
	int hookflags;			/**< flags for hooking */
	int hookindex;			/**< hook index of the request if hooked */
	struct afb_evt_listener *listener; /**< event listener for the request */
	struct afb_cred *cred;		/**< client credential if revelant */
	struct afb_xreq *caller;	/**< caller request if any */
};

/**
 * Macro for retrieve the pointer of a structure of 'type' having a field named 'field'
 * of adress 'ptr'.
 * @param type the type that has the 'field' (ex: "struct mystruct")
 * @param field the name of the field within the structure 'type'
 * @param ptr the pointer to an element 'field'
 * @return the pointer to the structure that contains the 'field' at address 'ptr'
 */
#define CONTAINER_OF(type,field,ptr) ((type*)(((intptr_t)(ptr))-((intptr_t)&(((type*)NULL)->field))))

/**
 * Macro for retrieve the pointer of a structure of 'type' having a field named "xreq"
 * of adress 'x'.
 * @param type the type that has the field "xreq" (ex: "struct mystruct")
 * @param x the pointer to the field "xreq"
 * @return the pointer to the structure that contains the field "xreq" of address 'x'
 */
#define CONTAINER_OF_XREQ(type,x) CONTAINER_OF(type,xreq,x)

/* req wrappers for xreq */
extern struct afb_req afb_xreq_unstore(struct afb_stored_req *sreq);
extern void afb_xreq_addref(struct afb_xreq *xreq);
extern void afb_xreq_unref(struct afb_xreq *xreq);
extern void afb_xreq_unhooked_addref(struct afb_xreq *xreq);
extern void afb_xreq_unhooked_unref(struct afb_xreq *xreq);

extern struct json_object *afb_xreq_json(struct afb_xreq *xreq);

extern void afb_xreq_success(struct afb_xreq *xreq, struct json_object *obj, const char *info);
extern void afb_xreq_success_f(struct afb_xreq *xreq, struct json_object *obj, const char *info, ...);

extern void afb_xreq_fail(struct afb_xreq *xreq, const char *status, const char *info);
extern void afb_xreq_fail_f(struct afb_xreq *xreq, const char *status, const char *info, ...);
extern void afb_xreq_fail_unknown_api(struct afb_xreq *xreq);
extern void afb_xreq_fail_unknown_verb(struct afb_xreq *xreq);

extern const char *afb_xreq_raw(struct afb_xreq *xreq, size_t *size);

extern int afb_xreq_subscribe(struct afb_xreq *xreq, struct afb_event event);
extern int afb_xreq_unsubscribe(struct afb_xreq *xreq, struct afb_event event);

extern void afb_xreq_subcall(
		struct afb_xreq *xreq,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*),
		void *cb_closure);
extern void afb_xreq_unhooked_subcall(
		struct afb_xreq *xreq,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*),
		void *cb_closure);

extern int afb_xreq_unhooked_subcall_sync(
		struct afb_xreq *xreq,
		const char *api,
		const char *verb,
		struct json_object *args,
		struct json_object **result);
extern int afb_xreq_subcall_sync(
		struct afb_xreq *xreq,
		const char *api,
		const char *verb,
		struct json_object *args,
		struct json_object **result);

/* initialisation and processing of xreq */
extern void afb_xreq_init(struct afb_xreq *xreq, const struct afb_xreq_query_itf *queryitf);

extern void afb_xreq_process(struct afb_xreq *xreq, struct afb_apiset *apiset);

extern void afb_xreq_call_verb_v1(struct afb_xreq *xreq, const struct afb_verb_desc_v1 *verb);
extern void afb_xreq_call_verb_v2(struct afb_xreq *xreq, const struct afb_verb_v2 *verb);

