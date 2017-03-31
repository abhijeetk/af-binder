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

struct afb_xreq_query_itf {
	struct json_object *(*json)(void *closure);
	struct afb_arg (*get)(void *closure, const char *name);
	void (*success)(void *closure, struct json_object *obj, const char *info);
	void (*fail)(void *closure, const char *status, const char *info);
	void (*reply)(void *closure, int iserror, struct json_object *obj);
	void (*unref)(void *closure);
	void (*subcall)(void *closure, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *cb_closure);
};


/**
 * Internal data for requests
 */
struct afb_xreq
{
	struct afb_context context; /**< context of the request */
	const char *api;	/**< the requested API */
	const char *verb;	/**< the requested VERB */
	void *query;	/**< closure for the query */
	const struct afb_xreq_query_itf *queryitf;
	int refcount;	/**< current ref count */
	int replied;	/**< is replied? */
	int timeout;	/**< timeout */
	int sessionflags; /**< flags to check */
	void *group;
	void (*callback)(struct afb_req req);
	struct afb_evt_listener *listener;
};

extern void afb_xreq_addref(struct afb_xreq *xreq);
extern void afb_xreq_unref(struct afb_xreq *xreq);
extern void afb_xreq_fail_f(struct afb_xreq *xreq, const char *status, const char *info, ...);
extern void afb_xreq_success_f(struct afb_xreq *xreq, struct json_object *obj, const char *info, ...);
extern void afb_xreq_call(struct afb_xreq *xreq);

