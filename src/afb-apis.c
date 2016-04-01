/*
 * Copyright (C) 2016 "IoT.bzh"
 * Author "Fulup Ar Foll"
 * Author José Bollo <jose.bollo@iot.bzh>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Contain all generic part to handle REST/API
 * 
 *  https://www.gnu.org/software/libmicrohttpd/tutorial.html [search 'largepost.c']
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>
#include <setjmp.h>

#include "../include/local-def.h"

#include "afb-req-itf.h"
#include "afb-apis.h"

struct api_desc {
	AFB_plugin *plugin;	/* descriptor */
	size_t prefixlen;
	const char *prefix;
	void *handle;		/* context of dlopen */
};

static struct api_desc *apis_array = NULL;
static int apis_count = 0;

static const char plugin_register_function[] = "pluginRegister";

int afb_apis_count()
{
	return apis_count;
}

void afb_apis_free_context(int apiidx, void *context)
{
	void (*cb)(void*);

	assert(0 <= apiidx && apiidx < apis_count);
	cb = apis_array[apiidx].plugin->freeCtxCB;
	if (cb)
		cb(context);
	else
		free(context);
}

const struct AFB_restapi *afb_apis_get(int apiidx, int verbidx)
{
	assert(0 <= apiidx && apiidx < apis_count);
	return &apis_array[apiidx].plugin->apis[verbidx];
}

int afb_apis_get_verbidx(int apiidx, const char *name)
{
	const struct AFB_restapi *apis;
	int idx;

	assert(0 <= apiidx && apiidx < apis_count);
	apis = apis_array[apiidx].plugin->apis;
	for (idx = 0 ; apis[idx].name ; idx++)
		if (!strcasecmp(apis[idx].name, name))
			return idx;
	return -1;
}

int afb_apis_get_apiidx(const char *prefix, size_t length)
{
	int i;
	const struct api_desc *a;

	if (!length)
		length = strlen(prefix);

	for (i = 0 ; i < apis_count ; i++) {
		a = &apis_array[i];
		if (a->prefixlen == length && !strcasecmp(a->prefix, prefix))
			return i;
	}
	return -1;
}

int afb_apis_add_plugin(const char *path)
{
	struct api_desc *apis;
	AFB_plugin *plugin;
	AFB_plugin *(*pluginRegisterFct) (void);
	void *handle;
	size_t len;

	// This is a loadable library let's check if it's a plugin
	handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (handle == NULL) {
		fprintf(stderr, "[%s] not loadable, continuing...\n", path);
		goto error;
	}

	/* retrieves the register function */
	pluginRegisterFct = dlsym(handle, plugin_register_function);
	if (!pluginRegisterFct) {
		fprintf(stderr, "[%s] not an AFB plugin, continuing...\n", path);
		goto error2;
	}
	if (verbose)
		fprintf(stderr, "[%s] is a valid AFB plugin\n", path);

	/* allocates enough memory */
	apis = realloc(apis_array, ((unsigned)apis_count + 1) * sizeof * apis);
	if (apis == NULL) {
		fprintf(stderr, "ERROR: plugin [%s] memory missing. continuing...\n", path);
		goto error2;
	}
	apis_array = apis;

	/* init the plugin */
	plugin = pluginRegisterFct();
	if (plugin == NULL) {
		fprintf(stderr, "ERROR: plugin [%s] register function failed. continuing...\n", path);
		goto error2;
	}

	/* check the returned structure */
	if (plugin->type != AFB_PLUGIN_JSON) {
		fprintf(stderr, "ERROR: plugin [%s] invalid type %d...\n", path, plugin->type);
		goto error2;
	}
	if (plugin->prefix == NULL || *plugin->prefix == 0) {
		fprintf(stderr, "ERROR: plugin [%s] bad prefix...\n", path);
		goto error2;
	}
	if (plugin->info == NULL || *plugin->info == 0) {
		fprintf(stderr, "ERROR: plugin [%s] bad description...\n", path);
		goto error2;
	}
	if (plugin->apis == NULL) {
		fprintf(stderr, "ERROR: plugin [%s] no APIs...\n", path);
		goto error2;
	}

	/* check previously existing plugin */
	len = strlen(plugin->prefix);
	if (afb_apis_get_apiidx(plugin->prefix, len) >= 0) {
		fprintf(stderr, "ERROR: plugin [%s] prefix %s duplicated...\n", path, plugin->prefix);
		goto error2;
	}

	/* record the plugin */
	if (verbose)
		fprintf(stderr, "Loading plugin[%lu] prefix=[%s] info=%s\n", (unsigned long)apis_count, plugin->prefix, plugin->info);
	apis = &apis_array[apis_count];
	apis->plugin = plugin;
	apis->prefixlen = len;
	apis->prefix = plugin->prefix;
	apis->handle = handle;
	apis_count++;

	return 0;

error2:
	dlclose(handle);
error:
	return -1;
}

static int adddirs(char path[PATH_MAX], size_t end)
{
	int rc;
	DIR *dir;
	struct dirent ent, *result;
	size_t len;

	/* open the DIR now */
	dir = opendir(path);
	if (dir == NULL) {
		fprintf(stderr, "ERROR in scanning plugin directory %s, %m\n", path);
		return -1;
	}
	if (verbose)
		fprintf(stderr, "Scanning dir=[%s] for plugins\n", path);

	/* scan each entry */
	if (end)
		path[end++] = '/';
	for (;;) {
		readdir_r(dir, &ent, &result);
		if (result == NULL)
			break;

		len = strlen(ent.d_name);
		if (len + end >= PATH_MAX) {
			fprintf(stderr, "path too long for %s\n", ent.d_name);
			continue;
		}
		memcpy(&path[end], ent.d_name, len+1);
		if (ent.d_type == DT_DIR) {
			/* case of directories */
			if (ent.d_name[0] == '.') {
				if (len == 1)
					continue;
				if (ent.d_name[1] == '.' && len == 2)
					continue;
			}
			rc = adddirs(path, end+len);;
		} else if (ent.d_type == DT_REG) {
			/* case of files */
			if (!strstr(ent.d_name, ".so"))
				continue;
			rc = afb_apis_add_plugin(path);
		}
	}
	closedir(dir);
	return 0;
}

int afb_apis_add_directory(const char *path)
{
	size_t length;
	char buffer[PATH_MAX];

	length = strlen(path);
	if (length >= sizeof(buffer)) {
		fprintf(stderr, "path too long %lu [%.99s...]\n", (unsigned long)length, path);
		return -1;
	}

	memcpy(buffer, path, length + 1);
	return adddirs(buffer, length);
}

int afb_apis_add_path(const char *path)
{
	struct stat st;
	int rc;

	rc = stat(path, &st);
	if (rc < 0)
		fprintf(stderr, "Invalid plugin path [%s]: %m\n", path);
	else if (S_ISDIR(st.st_mode))
		rc = afb_apis_add_directory(path);
	else
		rc = afb_apis_add_plugin(path);
	return rc;
}

int afb_apis_add_pathset(const char *pathset)
{
	static char sep[] = ":";
	char *ps, *p;
	int rc;

	ps = strdupa(pathset);
	for (;;) {
		p = strsep(&ps, sep);
		if (!p)
			return 0;
		rc = afb_apis_add_path(p);
	};
}

// Check of apiurl is declare in this plugin and call it
extern __thread sigjmp_buf *error_handler;
static int callPluginApi(AFB_request * request)
{
	volatile int status, timerset;
	timer_t timerid;
	sigjmp_buf jmpbuf, *older;
	struct sigevent sevp;
	struct itimerspec its;

	// save context before calling the API
	timerset = 0;
	older = error_handler;
	status = setjmp(jmpbuf);
	if (status != 0) {
		status = 0;
	}
	else {
		error_handler = &jmpbuf;
		if (request->config->apiTimeout > 0) {
			timerset = 1; /* TODO: check statuses */
			sevp.sigev_notify = SIGEV_THREAD_ID;
			sevp.sigev_signo = SIGALRM;
#if defined(sigev_notify_thread_id)
			sevp.sigev_notify_thread_id = syscall(SYS_gettid);
#else
			sevp._sigev_un._tid = syscall(SYS_gettid);
#endif
			timer_create(CLOCK_THREAD_CPUTIME_ID, &sevp, &timerid);
			its.it_interval.tv_sec = 0;
			its.it_interval.tv_nsec = 0;
			its.it_value.tv_sec = 15;
			its.it_value.tv_nsec = 0;
			timer_settime(timerid, 0, &its, NULL);
		}

		//doCallPluginApi(request, apiidx, verbidx, context);
		status = 1;
	}
	if (timerset)
		timer_delete(timerid);
	error_handler = older;

	return status;
}

static void handle(struct afb_req req, const struct api_desc *api, const struct AFB_restapi *verb)
{
	json_object *jresp, *jcall, *jreqt;

	AFB_request request;

	request.uuid = request.url = "fake";
	request.prefix = api->prefix;
	request.method = verb->name;
	request.context = NULL;
	request.restfull = 0;
	request.errcode = 0;
	request.config = NULL;
	request.areq = &req;

	switch(verb->session) {
	case AFB_SESSION_CREATE:
	case AFB_SESSION_RENEW:
		/*if (check) new*/
		break;
	case AFB_SESSION_CLOSE:
	case AFB_SESSION_CHECK:
		/*check*/
		break;
	case AFB_SESSION_NONE:
	default:
		break;
	}
	verb->callback(&request, NULL);

	if (verb->session == AFB_SESSION_CLOSE)
		/*close*/;
}

int afb_apis_handle(struct afb_req req, const char *api, size_t lenapi, const char *verb, size_t lenverb)
{
	int i, j;
	const struct api_desc *a;
	const struct AFB_restapi *v;

//fprintf(stderr,"afb_apis_handle prefix:%.*s verb:%.*s\n",(int)lenapi,api,(int)lenverb,verb);
	a = apis_array;
	for (i = 0 ; i < apis_count ; i++, a++) {
		if (a->prefixlen == lenapi && !strncasecmp(a->prefix, api, lenapi)) {
//fprintf(stderr,"afb_apis_handle found prefix:%.*s -> %s\n",(int)lenapi,api,a->prefix);
			v = a->plugin->apis;
			for (j = 0 ; v->name ; j++, v++) {
				if (!strncasecmp(v->name, verb, lenverb) && !v->name[lenverb]) {
//fprintf(stderr,"afb_apis_handle found prefix:%.*s verb:%.*s -> %s/%s\n",(int)lenapi,api,(int)lenverb,verb,a->prefix,v->name);
					handle(req, a, v);
					return 1;
				}
			}
			break;
		}
	}
	return 0;
}

