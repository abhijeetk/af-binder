/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <json-c/json.h>
#include "duktape.h"

#include "afb-common.h"
#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-api-js.h"
#include "afb-xreq.h"
#include "jobs.h"
#include "verbose.h"

/********************************************************************/
struct jsapi
{
	int logmask;
	duk_context *context;
	char api[1];
};

/********************************************************************/
static void jsapi_call(void *closure, struct afb_xreq *xreq);
static int jsapi_service_start(void *closure);
static int jsapi_get_logmask(void *closure);
static void jsapi_set_logmask(void *closure, int level);
static struct json_object *jsapi_describe(void *closure);

static struct afb_api_itf jsapi_itf =
{
	.call = jsapi_call,
	.service_start = jsapi_service_start,
	.set_logmask = jsapi_set_logmask,
	.get_logmask = jsapi_get_logmask,
	.describe = jsapi_describe
};

/********************************************************************/
static duk_ret_t do_success(duk_context *ctx);
static duk_ret_t do_fail(duk_context *ctx);
static duk_ret_t do_subcall_sync(duk_context *ctx);
static duk_ret_t do_error(duk_context *ctx);
static duk_ret_t do_require(duk_context *ctx);

static const duk_function_list_entry funcs[] =
{
	{ "afb_req_success", do_success, 3 },
	{ "afb_req_fail", do_fail, 3 },
	{ "afb_req_subcall_sync", do_subcall_sync, 4 },
	{ "afb_error", do_error, 1 },
	{ "require", do_require, 1 },
	{ NULL, NULL, 0 }
};

/********************************************************************/

static void on_heap_fatal(void *udata, const char *msg)
{
	ERROR("Got fatal from duktape: %s", msg);
	abort();
}

static int jsapi_load(duk_context *ctx, const char *path)
{
	static const char prefix[] = "function(exports){";
	static const char suffix[] = "}";

	int fd, rc;
	struct stat st;
	char *buffer;
	ssize_t s;

	fd = afb_common_rootdir_open_locale(path, O_RDONLY, NULL);
	if (fd < 0) {
		fd = open(path, O_RDONLY);
		if (fd < 0) {
			ERROR("Can't open %s: %m", path);
			duk_push_error_object(ctx, DUK_ERR_ERROR, "Can't open file %s: %m", path);
			goto error;
		}
	}

	rc = fstat(fd, &st);
	if (rc < 0) {
		goto error2;
	}

	buffer = alloca(st.st_size + sizeof prefix + sizeof suffix);
	s = read(fd, &buffer[sizeof prefix - 1], st.st_size);
	if (s < 0)
		goto error2;
	if (s != st.st_size)
		goto error2;

	memcpy(buffer, prefix, sizeof prefix - 1);
	memcpy(&buffer[sizeof prefix - 1 + st.st_size], suffix, sizeof suffix);
	close(fd);

	duk_push_object(ctx); /* exports */
	duk_push_string(ctx, path); /* exports path */
	rc = duk_pcompile_string_filename(ctx, DUK_COMPILE_FUNCTION|DUK_COMPILE_STRICT, buffer); /* exports func */
	if (rc) {
		duk_dup_top(ctx); /* exports error error */
		ERROR("compiling of %s failed: %s", path, duk_safe_to_string(ctx, -1)); /* exports error error */
		duk_pop(ctx); /* exports error */
		duk_replace(ctx, -2); /* error */
		goto error;
	}
	duk_dup(ctx, -2); /* exports func exports */
	rc = duk_pcall(ctx, 1); /* exports ret */
	if (rc) {
		duk_dup_top(ctx); /* exports error error */
		if (!duk_is_error(ctx, -1)) {
			duk_push_error_object(ctx, DUK_ERR_ERROR, "%s", duk_safe_to_string(ctx, -1)); /* exports error error error */
			duk_replace(ctx, -3); /* exports error error */
		}
		ERROR("initialisation of %s failed: %s", path, duk_safe_to_string(ctx, -1)); /* exports error */
		duk_pop(ctx); /* exports error */
		duk_replace(ctx, -2); /* error */
		goto error;
	}
	duk_pop(ctx); /* exports */
	return 1;
error2:
	ERROR("can't process file %s: %m", path);
	duk_push_error_object(ctx, DUK_ERR_ERROR, "Can't process file %s: %m", path);
	close(fd);
error:
	return duk_throw(ctx);
}

static duk_ret_t do_require(duk_context *ctx)
{
	int rc;
	const char *path;

	path = duk_require_string(ctx, -1); /* path */
	duk_push_global_stash(ctx); /* path gstash */
	duk_dup(ctx, -2); /* path gstash path */
	rc = duk_get_prop(ctx, -2); /* path gstash ? */
	if (!rc) {
		/* path gstash error (ELSE: path gstash exports) */
		duk_pop(ctx); /* path gstash */
		rc = jsapi_load(ctx, path); /* path gstash ? */
		if (rc > 0) {
			/* path gstash exports (ELSE: path gstash error) */
			duk_dup_top(ctx); /* path gstash exports exports */
			duk_swap(ctx, -2, -4); /* exports gstash path exports */
			duk_put_prop(ctx, -3); /* exports gstash */
			duk_pop(ctx); /* exports */
		}
	}
	return 1;
}

static void jsapi_destroy(struct jsapi *jsapi)
{
	duk_destroy_heap(jsapi->context);
	free(jsapi);
}

static struct jsapi *jsapi_create(const char *path)
{
	struct jsapi *jsapi;
	duk_context *ctx;
	const char *api, *ext;

	/* allocate and initialise names */
	api = strrchr(path, '/');
	api = api ? api + 1 : path;
	ext = strrchr(api, '.') ?: &api[strlen(api)];
	jsapi = malloc(sizeof *jsapi + 1 + (ext - api));
	if (!jsapi)
		goto error;
	memcpy(jsapi->api, api, ext - api);
	jsapi->api[ext - api] = 0;
	jsapi->logmask = logmask;

	/* create the duktape context */
	ctx = duk_create_heap(NULL, NULL, NULL, NULL, on_heap_fatal);
	if (!ctx)
		goto error;

	jsapi->context = ctx;

	/* populate global with functions */
	duk_push_global_object(ctx);
	duk_put_function_list(ctx, -1, funcs);
	duk_pop(ctx);

	/* call the require path */
	ctx = jsapi->context;
	duk_get_global_string(ctx, "require");
	duk_push_string(ctx, path);
	duk_pcall(ctx, 1);
	if (duk_is_error(ctx, -1)) {
		const char *message, *file, *stack;
		int line;
		duk_get_prop_string(ctx, -1, "message");
		message = duk_get_string(ctx, -1);
		duk_get_prop_string(ctx, -2, "fileName");
		file = duk_get_string(ctx, -1);
		duk_get_prop_string(ctx, -3, "lineNumber");
		line = (int)duk_get_int(ctx, -1);
		duk_get_prop_string(ctx, -4, "stack");
		stack = duk_get_string(ctx, -1);
		ERROR("Initialisation of API %s from jsapi %s failed file %s (file %s, line %d) stack:\n%s\n", jsapi->api, path, message, file, line, stack);
		jsapi_destroy(jsapi);
		return NULL;
	}
	duk_put_global_string(ctx, "exports");
	return jsapi;

error:
	ERROR("out of memory");
	free(jsapi);
	errno = ENOMEM;
	return NULL;
}

int afb_api_js_add(const char *path, struct afb_apiset *declare_set, struct afb_apiset* call_set)
{
	int rc;
	struct jsapi *jsapi;
	struct afb_api_item api;

	jsapi = jsapi_create(path);
	if (!jsapi)
		goto error;

	api.closure = jsapi;
	api.itf = &jsapi_itf;
	api.group = jsapi;
	rc = afb_apiset_add(declare_set, jsapi->api, api);
	if (!rc)
		return 0;

	duk_destroy_heap(jsapi->context);
	free(jsapi);
error:
	return -1;
}

/********************************************************************/

static duk_ret_t do_success(duk_context *ctx)
{
	struct afb_xreq *xreq;
	const char *json, *info;

	xreq = duk_get_pointer(ctx, -3);
	duk_json_encode(ctx, -2);
	json = duk_get_string(ctx, -2);
	info = duk_get_string(ctx, -1);

	afb_xreq_reply(xreq, json ? json_tokener_parse(json) : NULL, NULL, info);
	return 0;
}

static duk_ret_t do_fail(duk_context *ctx)
{
	struct afb_xreq *xreq;
	const char *status, *info;

	xreq = duk_get_pointer(ctx, -3);
	status = duk_get_string(ctx, -2);
	info = duk_get_string(ctx, -1);

	afb_xreq_reply(xreq, NULL, status ?: "error", info);
	return 0;
}

static duk_ret_t do_subcall_sync(duk_context *ctx)
{
	int rc;
	struct afb_xreq *xreq;
	const char *api, *verb, *json;
	struct json_object *resu;

	xreq = duk_get_pointer(ctx, -4);
	api = duk_get_string(ctx, -3);
	verb = duk_get_string(ctx, -2);
	duk_json_decode(ctx, -1);
	json = duk_get_string(ctx, -1);

	resu = NULL;
	rc = afb_xreq_legacy_subcall_sync(xreq, api, verb, json ? json_tokener_parse(json) : NULL, &resu);
	if (rc)
		duk_push_null(ctx);
	else {
		duk_push_string(ctx, json_object_to_json_string(resu));
		duk_json_decode(ctx, -1);
	}
	json_object_put(resu);
	return 1;
}

static duk_ret_t do_error(duk_context *ctx)
{
	const char *message;

	message = duk_get_string(ctx, -1);

	ERROR("%s", message ? : "null");
	return 0;
}

/********************************************************************/

static void jsapi_call(void *closure, struct afb_xreq *xreq)
{
	duk_idx_t top;
	duk_context *ctx;
	json_object *args;
	const char *json;
	struct jsapi *jsapi = closure;

	ctx = jsapi->context;
	top = duk_get_top(ctx);
	duk_get_global_string(ctx, "exports");
	if (!duk_is_object(ctx, -1)) {
		afb_xreq_reply(xreq, NULL, "internal-error", "no exports!?");
		goto end;
	}
	duk_get_prop_string(ctx, -1, xreq->request.called_verb);
	if (!duk_is_function(ctx, -1)) {
		afb_xreq_reply_unknown_verb(xreq);
		goto end;
	}
	duk_push_pointer(ctx, xreq);
	args = afb_xreq_json(xreq);
	json = json_object_to_json_string(args);
	duk_push_string(ctx, json);
	duk_json_decode(ctx, -1);
	duk_pcall(ctx, 2);
end:
	duk_pop_n(ctx, duk_get_top(ctx) - top);
}

static int jsapi_service_start(void *closure)
{
	struct jsapi *jsapi = closure;
	return 0;
}

static int jsapi_get_logmask(void *closure)
{
	struct jsapi *jsapi = closure;
	return jsapi->logmask;
}

static void jsapi_set_logmask(void *closure, int level)
{
	struct jsapi *jsapi = closure;
	jsapi->logmask = level;
}

static struct json_object *jsapi_describe(void *closure)
{
	struct jsapi *jsapi = closure;
	return NULL;
}

