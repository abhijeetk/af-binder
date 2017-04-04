/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
 * Author "Fulup Ar Foll"
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

#include <assert.h>
#include <stdlib.h>

#include "afb-session.h"
#include "afb-context.h"

static void init_context(struct afb_context *context, struct afb_session *session, const char *token)
{
	assert(session != NULL);

	/* reset the context for the session */
	context->session = session;
	context->flags = 0;
	context->super = NULL;
	context->api_key = NULL;
	context->loa_in = afb_session_get_LOA(session) & 7;

	/* check the token */
	if (token != NULL) {
		if (afb_session_check_token(session, token))
			context->validated = 1;
		else
			context->invalidated = 1;
	}
}

void afb_context_init(struct afb_context *context, struct afb_session *session, const char *token)
{
	init_context(context, afb_session_addref(session), token);
}

void afb_context_subinit(struct afb_context *context, struct afb_context *super)
{
	*context = *super;
	context->super = super;
}

int afb_context_connect(struct afb_context *context, const char *uuid, const char *token)
{
	int created;
	struct afb_session *session;

	session = afb_session_get (uuid, &created);
	if (session == NULL)
		return -1;
	init_context(context, session, token);
	if (created) {
		context->created = 1;
		/* context->refreshing = 1; */
	}
	return 0;
}

void afb_context_disconnect(struct afb_context *context)
{
	if (context->session && !context->super) {
		if (context->refreshing && !context->refreshed) {
			afb_session_new_token (context->session);
			context->refreshed = 1;
		}
		if (context->loa_changing && !context->loa_changed) {
			afb_session_set_LOA (context->session, context->loa_out);
			context->loa_changed = 1;
		}
		if (context->closing && !context->closed) {
			afb_session_close(context->session);
			context->closed = 1;
		}
		afb_session_unref(context->session);
		context->session = NULL;
	}
}

const char *afb_context_sent_token(struct afb_context *context)
{
	if (context->session == NULL || context->closing || context->super)
		return NULL;
	if (!context->refreshing)
		return NULL;
	if (!context->refreshed) {
		afb_session_new_token (context->session);
		context->refreshed = 1;
	}
	return afb_session_token(context->session);
}

const char *afb_context_sent_uuid(struct afb_context *context)
{
	if (context->session == NULL || context->closing || context->super)
		return NULL;
	if (!context->created)
		return NULL;
	return afb_session_uuid(context->session);
}

void *afb_context_get(struct afb_context *context)
{
	assert(context->session != NULL);
	return afb_session_get_cookie(context->session, context->api_key);
}

void afb_context_set(struct afb_context *context, void *value, void (*free_value)(void*))
{
	int rc;
	assert(context->session != NULL);
	rc = afb_session_set_cookie(context->session, context->api_key, value, free_value);
	(void)rc; /* TODO */
}

void afb_context_close(struct afb_context *context)
{
	if (context->super)
		afb_context_close(context->super);
	else
		context->closing = 1;
}

void afb_context_refresh(struct afb_context *context)
{
	if (context->super)
		afb_context_refresh(context->super);
	else {
		assert(context->validated);
		context->refreshing = 1;
	}
}

int afb_context_check(struct afb_context *context)
{
	if (context->super)
		return afb_context_check(context);
	return context->validated;
}

int afb_context_check_loa(struct afb_context *context, unsigned loa)
{
	if (context->super)
		return afb_context_check_loa(context->super, loa);
	return context->loa_in >= loa;
}

int afb_context_change_loa(struct afb_context *context, unsigned loa)
{
	if (context->super)
		return afb_context_change_loa(context, loa);

	if (!context->validated || loa > 7)
		return 0;

	if (loa == context->loa_in && !context->loa_changed)
		context->loa_changing = 0;
	else {
		context->loa_out = loa & 7;
		context->loa_changing = 1;
		context->loa_changed = 0;
	}
	return 1;
}


