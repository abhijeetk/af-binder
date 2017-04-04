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
#include "jobs.h"

struct subcall;

static void subcall_destroy(void *closure);
static void subcall_reply(void *closure, int iserror, struct json_object *obj);
static int subcall_subscribe(void *closure, struct afb_event event);
static int subcall_unsubscribe(void *closure, struct afb_event event);
static void subcall_sync_leave(struct subcall *subcall);

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
	struct jobloop *jobloop;
	struct json_object *result;
	int iserror;
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

	if (subcall->callback) {
		subcall->callback(subcall->closure, iserror, obj);
		json_object_put(obj);
	} else {
		subcall->result = obj;
		subcall->iserror = iserror;
		subcall_sync_leave(subcall);
	}
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
	afb_xreq_addref(caller);
	json_object_get(args);
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
		callback(closure, 1, afb_msg_json_internal_error());
		return;
	}

	afb_apis_call(&subcall->xreq);
	afb_xreq_unref(&subcall->xreq);
}

static void subcall_sync_leave(struct subcall *subcall)
{
	struct jobloop *jobloop = subcall->jobloop;
	subcall->jobloop = NULL;
	if (jobloop)
		jobs_leave(jobloop);
}

static void subcall_sync_cb(int signum, void *closure, struct jobloop *jobloop)
{
	struct subcall *subcall = closure;

	if (!signum) {
		subcall->jobloop = jobloop;
		afb_apis_call_direct(&subcall->xreq);
	} else {
		afb_xreq_fail_f(&subcall->xreq, "aborted", "signal %s(%d) caught", strsignal(signum), signum);
	}
	subcall_sync_leave(subcall);
}

int afb_subcall_sync(
		struct afb_xreq *caller,
		const char *api,
		const char *verb,
		struct json_object *args,
		struct json_object **result
)
{
	int rc;
	struct subcall *subcall;

	subcall = create_subcall(caller, api, verb, args, NULL, NULL);
	if (subcall == NULL) {
		*result = json_object_get(afb_msg_json_internal_error());
		return 0;
	}

	rc = jobs_enter(NULL, 0, subcall_sync_cb, subcall);
	if (rc < 0) {
		subcall->result = json_object_get(afb_msg_json_internal_error());
		subcall->iserror = 1;
	}
	rc = !subcall->iserror;
	*result = subcall->result;
	afb_xreq_unref(&subcall->xreq);
	return rc;
}


