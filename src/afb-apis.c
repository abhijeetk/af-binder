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
#include "afb-hook.h"
#include <afb/afb-req-itf.h>

struct api_desc {
	struct afb_api api;
	const char *name;
};

static struct api_desc *apis_array = NULL;
static int apis_count = 0;

/**
 * Returns the current count of APIs
 */
int afb_apis_count()
{
	return apis_count;
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
 * @param name the name of the api to add
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
	int i;

	/* Checks the api name */
	if (!afb_apis_is_valid_api_name(name)) {
		ERROR("invalid api name forbidden (name is '%s')", name);
		errno = EINVAL;
		goto error;
	}

	/* check previously existing plugin */
	for (i = 0 ; i < apis_count ; i++) {
		if (!strcasecmp(apis_array[i].name, name)) {
			ERROR("api of name %s already exists", name);
			errno = EEXIST;
			goto error;
		}
	}

	/* allocates enough memory */
	apis = realloc(apis_array, ((unsigned)apis_count + 1) * sizeof * apis);
	if (apis == NULL) {
		ERROR("out of memory");
		errno = ENOMEM;
		goto error;
	}
	apis_array = apis;

	/* record the plugin */
	apis = &apis_array[apis_count];
	apis->api = api;
	apis->name = name;
	apis_count++;

	return 0;

error:
	return -1;
}

void afb_apis_call(struct afb_req req, struct afb_context *context, const char *api, const char *verb)
{
	int i;
	const struct api_desc *a;

	req = afb_hook_req_call(req, context, api, verb);
	a = apis_array;
	for (i = 0 ; i < apis_count ; i++, a++) {
		if (!strcasecmp(a->name, api)) {
			context->api_index = i;
			a->api.call(a->api.closure, req, context, verb);
			return;
		}
	}
	afb_req_fail(req, "fail", "api not found");
}

int afb_apis_start_service(const char *api, int share_session, int onneed)
{
	int i;

	for (i = 0 ; i < apis_count ; i++) {
		if (!strcasecmp(apis_array[i].name, api))
			return apis_array[i].api.service_start(apis_array[i].api.closure, share_session, onneed);
	}
	ERROR("can't find service %s", api);
	return -1;
}

int afb_apis_start_all_services(int share_session)
{
	int i, rc;

	for (i = 0 ; i < apis_count ; i++) {
		rc = apis_array[i].api.service_start(apis_array[i].api.closure, share_session, 1);
		if (rc < 0)
			return rc;
	}
	return 0;
}

