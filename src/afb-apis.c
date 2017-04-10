/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "afb-session.h"
#include "verbose.h"
#include "afb-apis.h"
#include "afb-context.h"
#include "afb-xreq.h"
#include "jobs.h"

#include <afb/afb-req-itf.h>

/**
 * Internal description of an api
 */
struct api_desc {
	const char *name;	/**< name of the api */
	struct afb_api api;	/**< handler of the api */
};

static struct api_desc *apis_array = NULL;
static int apis_count = 0;
static int apis_timeout = 15;

/**
 * Set the API timeout
 * @param to the timeout in seconds
 */
void afb_apis_set_timeout(int to)
{
	apis_timeout = to;
}

/**
 * Checks wether 'name' is a valid API name.
 * @return 1 if valid, 0 otherwise
 */
int afb_apis_is_valid_api_name(const char *name)
{
	unsigned char c;

	c = (unsigned char)*name;
	if (c == 0)
		/* empty names aren't valid */
		return 0;

	do {
		if (c < (unsigned char)'\x80') {
			switch(c) {
			default:
				if (c > ' ')
					break;
			case '"':
			case '#':
			case '%':
			case '&':
			case '\'':
			case '/':
			case '?':
			case '`':
			case '\\':
			case '\x7f':
				return 0;
			}
		}
		c = (unsigned char)*++name;
	} while(c != 0);
	return 1;
}

/**
 * Adds the api of 'name' described by 'api'.
 * @param name the name of the api to add (have to survive, not copied!)
 * @param api the api
 * @returns 0 in case of success or -1 in case
 * of error with errno set:
 *   - EINVAL if name isn't valid
 *   - EEXIST if name already registered
 *   - ENOMEM when out of memory
 */
int afb_apis_add(const char *name, struct afb_api api)
{
	struct api_desc *apis;
	int i, c;

	/* Checks the api name */
	if (!afb_apis_is_valid_api_name(name)) {
		ERROR("invalid api name forbidden (name is '%s')", name);
		errno = EINVAL;
		goto error;
	}

	/* check previously existing plugin */
	for (i = 0 ; i < apis_count ; i++) {
		c = strcasecmp(apis_array[i].name, name);
		if (c == 0) {
			ERROR("api of name %s already exists", name);
			errno = EEXIST;
			goto error;
		}
		if (c > 0)
			break;
	}

	/* allocates enough memory */
	apis = realloc(apis_array, ((unsigned)apis_count + 1) * sizeof * apis);
	if (apis == NULL) {
		ERROR("out of memory");
		errno = ENOMEM;
		goto error;
	}
	apis_array = apis;

	/* copy higher part of the array */
	c = apis_count;
	while (c > i) {
		apis_array[c] = apis_array[c - 1];
		c--;
	}

	/* record the plugin */
	apis = &apis_array[i];
	apis->api = api;
	apis->name = name;
	apis_count++;

	NOTICE("API %s added", name);

	return 0;

error:
	return -1;
}

/**
 * Search the 'api'.
 * @param api the api of the verb
 * @return the descriptor if found or NULL otherwise
 */
static const struct api_desc *search(const char *api)
{
	int i, c, up, lo;
	const struct api_desc *a;

	/* dichotomic search of the api */
	/* initial slice */
	lo = 0;
	up = apis_count;
	for (;;) {
		/* check remaining slice */
		if (lo >= up) {
			/* not found */
			return NULL;
		}
		/* check the mid of the slice */
		i = (lo + up) >> 1;
		a = &apis_array[i];
		c = strcasecmp(a->name, api);
		if (c == 0) {
			/* found */
			return a;
		}
		/* update the slice */
		if (c < 0)
			lo = i + 1;
		else
			up = i;
	}
}

/**
 * Starts a service by its 'api' name.
 * @param api name of the service to start
 * @param share_session if true start the servic"e in a shared session
 *                      if false start it in its own session
 * @param onneed if true start the service if possible, if false the api
 *               must be a service
 * @return a positive number on success
 */
int afb_apis_start_service(const char *api, int share_session, int onneed)
{
	int i;

	for (i = 0 ; i < apis_count ; i++) {
		if (!strcasecmp(apis_array[i].name, api))
			return apis_array[i].api.itf->service_start(apis_array[i].api.closure, share_session, onneed);
	}
	ERROR("can't find service %s", api);
	errno = ENOENT;
	return -1;
}

/**
 * Starts all possible services but stops at first error.
 * @param share_session if true start the servic"e in a shared session
 *                      if false start it in its own session
 * @return 0 on success or a negative number when an error is found
 */
int afb_apis_start_all_services(int share_session)
{
	int i, rc;

	for (i = 0 ; i < apis_count ; i++) {
		rc = apis_array[i].api.itf->service_start(apis_array[i].api.closure, share_session, 1);
		if (rc < 0)
			return rc;
	}
	return 0;
}

/**
 * Internal direct dispatch of the request 'xreq'
 * @param xreq the request to dispatch
 */
static void do_call_direct(struct afb_xreq *xreq)
{
	const struct api_desc *a;

	/* search the api */
	a = search(xreq->api);
	if (!a)
		afb_xreq_fail_f(xreq, "unknown-api", "api %s not found", xreq->api);
	else {
		xreq->context.api_key = a->api.closure;
		a->api.itf->call(a->api.closure, xreq);
	}
}

/**
 * Asynchronous dispatch callback for the request 'xreq'
 * @param signum 0 on normal flow or the signal number that interupted the normal flow
 */
static void do_call_async(int signum, void *arg)
{
	struct afb_xreq *xreq = arg;

	if (signum != 0)
		afb_xreq_fail_f(xreq, "aborted", "signal %s(%d) caught", strsignal(signum), signum);
	else {
		do_call_direct(xreq);
	}
	afb_xreq_unref(xreq);
}

/**
 * Dispatch the request 'xreq' synchronously and directly.
 * @param xreq the request to dispatch
 */
void afb_apis_call_direct(struct afb_xreq *xreq)
{
	afb_xreq_begin(xreq);
	do_call_direct(xreq);
}

/**
 * Dispatch the request 'xreq' asynchronously.
 * @param xreq the request to dispatch
 */
void afb_apis_call(struct afb_xreq *xreq)
{
	int rc;

	afb_xreq_begin(xreq);
	afb_xreq_addref(xreq);
	rc = jobs_queue(NULL, apis_timeout, do_call_async, xreq);
	if (rc < 0) {
		/* TODO: allows or not to proccess it directly as when no threading? (see above) */
		ERROR("can't process job with threads: %m");
		afb_xreq_fail_f(xreq, "cancelled", "not able to create a job for the task");
		afb_xreq_unref(xreq);
	}
}

/**
 * Ask to update the hook flags of the 'api'
 * @param api the api to update (NULL updates all)
 */
void afb_apis_update_hooks(const char *api)
{
	const struct api_desc *i, *e;

	if (!api) {
		i = apis_array;
		e = &apis_array[apis_count];
	} else {
		i = search(api);
		e = &i[!!i];
	}
	while (i != e) {
		if (i->api.itf->update_hooks)
			i->api.itf->update_hooks(i->api.closure);
		i++;
	}
}

/**
 * Set the verbosity level of the 'api'
 * @param api the api to set (NULL set all)
 */
void afb_apis_set_verbosity(const char *api, int level)
{
	const struct api_desc *i, *e;

	if (!api) {
		i = apis_array;
		e = &apis_array[apis_count];
	} else {
		i = search(api);
		e = &i[!!i];
	}
	while (i != e) {
		if (i->api.itf->set_verbosity)
			i->api.itf->set_verbosity(i->api.closure, level);
		i++;
	}
}

/**
 * Set the verbosity level of the 'api'
 * @param api the api to set (NULL set all)
 */
int afb_apis_get_verbosity(const char *api)
{
	const struct api_desc *i;

	i = api ? search(api) : NULL;
	if (!i) {
		errno = ENOENT;
		return -1;
	}
	if (!i->api.itf->get_verbosity)
		return 0;

	return i->api.itf->get_verbosity(i->api.closure);
}

/**
 * Get the list of api names
 * @return a NULL terminated array of api names. Must be freed.
 */
const char **afb_apis_get_names()
{
	size_t size;
	char *dest;
	const char **names;
	int i;

	size = apis_count * (1 + sizeof(*names)) + sizeof(*names);
	for (i = 0 ; i < apis_count ; i++)
		size += strlen(apis_array[i].name);

	names = malloc(size);
	if (!names)
		errno = ENOMEM;
	else {
		dest = (void*)&names[apis_count+1];
		for (i = 0 ; i < apis_count ; i++) {
			names[i] = dest;
			dest = stpcpy(dest, apis_array[i].name) + 1;
		}
		names[i] = NULL;
	}
	return names;
}

