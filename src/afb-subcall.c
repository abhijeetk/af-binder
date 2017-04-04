/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
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

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>
#include <afb/afb-req-itf.h>

#include "afb-subcall.h"
#include "afb-msg-json.h"
#include "afb-apis.h"
#include "afb-context.h"
#include "afb-xreq.h"
#include "verbose.h"

struct subcall;

static void subcall_destroy(void *closure);
static void subcall_reply(void *closure, int iserror, struct json_object *obj);
static int subcall_subscribe(void *closure, struct afb_event event);
static int subcall_unsubscribe(void *closure, struct afb_event event);

const struct afb_xreq_query_itf afb_subcall_xreq_itf = {
	.reply = subcall_reply,
	.unref = subcall_destroy,
	.subscribe = subcall_subscribe,
	.unsubscribe = subcall_unsubscribe
};

struct subcall
{
	struct afb_xreq xreq;
	struct afb_xreq *caller;
	void (*callback)(void*, int, struct json_object*);
	void *closure;
};

static void subcall_destroy(void *closure)
{
	struct subcall *subcall = closure;

	json_object_put(subcall->xreq.json);
	afb_xreq_unref(subcall->caller);
	free(subcall);
}

static void subcall_reply(void *closure, int iserror, struct json_object *obj)
{
	struct subcall *subcall = closure;

	subcall->callback(subcall->closure, iserror, obj);
	json_object_put(obj);
}

static int subcall_subscribe(void *closure, struct afb_event event)
{
	struct subcall *subcall = closure;

	return afb_xreq_subscribe(subcall->caller, event);
}

static int subcall_unsubscribe(void *closure, struct afb_event event)
{
	struct subcall *subcall = closure;

	return afb_xreq_unsubscribe(subcall->caller, event);
}

void afb_subcall_internal_error(void (*callback)(void*, int, struct json_object*), void *closure)
{
	static struct json_object *obj;

	if (obj == NULL)
		obj = afb_msg_json_reply_error("failed", "internal error", NULL, NULL);

	callback(closure, 1, obj);
}

static struct subcall *create_subcall(struct afb_xreq *caller, const char *api, const char *verb, struct json_object *args, void (*callback)(void*, int, struct json_object*), void *closure)
{
	struct subcall *subcall;

	subcall = calloc(1, sizeof *subcall);
	if (subcall == NULL) {
		return NULL;
	}


	afb_context_subinit(&subcall->xreq.context, &caller->context);
	subcall->xreq.refcount = 1;
	subcall->xreq.json = args;
	subcall->xreq.api = api; /* TODO: alloc ? */
	subcall->xreq.verb = verb; /* TODO: alloc ? */
	subcall->xreq.query = subcall;
	subcall->xreq.queryitf = &afb_subcall_xreq_itf;
	subcall->caller = caller;
	subcall->callback = callback;
	subcall->closure = closure;
	return subcall;
}

void afb_subcall(
		struct afb_xreq *caller,
		const char *api,
		const char *verb,
		struct json_object *args,
		void (*callback)(void*, int, struct json_object*),
		void *closure
)
{
	struct subcall *subcall;

	subcall = create_subcall(caller, api, verb, args, callback, closure);
	if (subcall == NULL) {
		afb_subcall_internal_error(callback, closure);
		return;
	}

	afb_xreq_addref(caller);
	afb_apis_xcall(&subcall->xreq);
	afb_xreq_unref(&subcall->xreq);
}


