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
#include "afb-apiset.h"
#include "afb-context.h"
#include "afb-xreq.h"
#include "afb-cred.h"
#include "verbose.h"

struct subcall
{
	struct afb_xreq xreq;
	struct afb_xreq *caller;
	void (*callback)(void*, int, struct json_object*);
	void *closure;
};

static void subcall_destroy(struct afb_xreq *xreq)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	json_object_put(subcall->xreq.json);
	afb_cred_unref(subcall->xreq.cred);
	afb_xreq_unref(subcall->caller);
	free(subcall);
}

static void subcall_reply(struct afb_xreq *xreq, int iserror, struct json_object *obj)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	subcall->callback(subcall->closure, iserror, obj);
	json_object_put(obj);
}

static int subcall_subscribe(struct afb_xreq *xreq, struct afb_event event)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	return afb_xreq_subscribe(subcall->caller, event);
}

static int subcall_unsubscribe(struct afb_xreq *xreq, struct afb_event event)
{
	struct subcall *subcall = CONTAINER_OF_XREQ(struct subcall, xreq);

	return afb_xreq_unsubscribe(subcall->caller, event);
}

const struct afb_xreq_query_itf afb_subcall_xreq_itf = {
	.reply = subcall_reply,
	.unref = subcall_destroy,
	.subscribe = subcall_subscribe,
	.unsubscribe = subcall_unsubscribe
};

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
	size_t lenapi, lenverb;
	char *copy;

	lenapi = 1 + strlen(api);
	lenverb = 1 + strlen(verb);
	subcall = malloc(lenapi + lenverb + sizeof *subcall);
	if (subcall == NULL) {
		json_object_put(args); /* keep args existing */
		callback(closure, 1, afb_msg_json_internal_error());
	} else {
		afb_xreq_init(&subcall->xreq, &afb_subcall_xreq_itf);
		afb_context_subinit(&subcall->xreq.context, &caller->context);
		subcall->xreq.cred = afb_cred_addref(caller->cred);
		subcall->xreq.json = args;
		copy = (char*)&subcall[1];
		memcpy(copy, api, lenapi);
		subcall->xreq.api = copy;
		copy = &copy[lenapi];
		memcpy(copy, verb, lenverb);
		subcall->xreq.verb = copy;
		subcall->caller = caller;
		subcall->callback = callback;
		subcall->closure = closure;
		afb_xreq_addref(caller);
		afb_xreq_process(&subcall->xreq, caller->apiset);
	}
}

