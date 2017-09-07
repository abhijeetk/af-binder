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
#include <dlfcn.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include "afb-api-so.h"
#include "afb-api-so-v1.h"
#include "afb-api-so-v2.h"
#include "verbose.h"
#include "sig-monitor.h"

struct safe_dlopen
{
	const char *path;
	void *handle;
	int flags;
};

static void safe_dlopen_cb(int sig, void *closure)
{
	struct safe_dlopen *sd = closure;
	if (!sig)
		sd->handle = dlopen(sd->path, sd->flags);
	else {
		ERROR("dlopen of %s raised signal %s", sd->path, strsignal(sig));
		sd->handle = NULL;
	}
}

static void *safe_dlopen(const char *filename, int flags)
{
	struct safe_dlopen sd;
	sd.path = filename;
	sd.flags = flags;
	sd.handle = NULL;
	sig_monitor(0, safe_dlopen_cb, &sd);
	return sd.handle;
}

static int load_binding(const char *path, int force, struct afb_apiset *apiset)
{
	int rc;
	void *handle;

	// This is a loadable library let's check if it's a binding
	rc = -!!force;
	handle = safe_dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (handle == NULL) {
		if (force)
			ERROR("binding [%s] not loadable: %s", path, dlerror());
		else
			INFO("binding [%s] not loadable: %s", path, dlerror());
		goto error;
	}

	/* try the version 2 */
	rc = afb_api_so_v2_add(path, handle, apiset);
	if (rc < 0) {
		/* error when loading a valid v2 binding */
		goto error2;
	}
	if (rc)
		return 0; /* yes version 2 */

	/* try the version 1 */
	rc = afb_api_so_v1_add(path, handle, apiset);
	if (rc < 0) {
		/* error when loading a valid v1 binding */
		goto error2;
	}
	if (rc)
		return 0; /* yes version 1 */

	/* not a valid binding */
	if (force)
		ERROR("binding [%s] is not an AFB binding", path);
	else
		INFO("binding [%s] is not an AFB binding", path);

error2:
	dlclose(handle);
error:
	return rc;
}


int afb_api_so_add_binding(const char *path, struct afb_apiset *apiset)
{
	return load_binding(path, 1, apiset);
}

static int adddirs(char path[PATH_MAX], size_t end, struct afb_apiset *apiset)
{
	DIR *dir;
	struct dirent *dent;
	size_t len;

	/* open the DIR now */
	dir = opendir(path);
	if (dir == NULL) {
		ERROR("can't scan binding directory %s, %m", path);
		return -1;
	}
	INFO("Scanning dir=[%s] for bindings", path);

	/* scan each entry */
	if (end)
		path[end++] = '/';
	for (;;) {
		errno = 0;
		dent = readdir(dir);
		if (dent == NULL) {
			if (errno != 0)
				ERROR("read error while scanning directory %.*s: %m", (int)(end - 1), path);
			break;
		}

		len = strlen(dent->d_name);
		if (len + end >= PATH_MAX) {
			ERROR("path too long while scanning bindings for %s", dent->d_name);
			continue;
		}
		if (dent->d_type == DT_DIR) {
			/* case of directories */
			if (dent->d_name[0] == '.') {
				if (len == 1)
					continue; /* . */
				if (dent->d_name[1] == '.' && len == 2)
					continue; /* .. */
			}
			memcpy(&path[end], dent->d_name, len+1);
			adddirs(path, end+len, apiset);
		} else if (dent->d_type == DT_REG) {
			/* case of files */
			if (memcmp(&dent->d_name[len - 3], ".so", 4))
				continue;
			memcpy(&path[end], dent->d_name, len+1);
			if (load_binding(path, 0, apiset) < 0)
				return -1;
		}
	}
	closedir(dir);
	return 0;
}

int afb_api_so_add_directory(const char *path, struct afb_apiset *apiset)
{
	size_t length;
	char buffer[PATH_MAX];

	length = strlen(path);
	if (length >= sizeof(buffer)) {
		ERROR("path too long %lu [%.99s...]", (unsigned long)length, path);
		return -1;
	}

	memcpy(buffer, path, length + 1);
	return adddirs(buffer, length, apiset);
}

int afb_api_so_add_path(const char *path, struct afb_apiset *apiset)
{
	struct stat st;
	int rc;

	rc = stat(path, &st);
	if (rc < 0)
		ERROR("Invalid binding path [%s]: %m", path);
	else if (S_ISDIR(st.st_mode))
		rc = afb_api_so_add_directory(path, apiset);
	else if (strstr(path, ".so"))
		rc = load_binding(path, 0, apiset);
	else
		INFO("not a binding [%s], skipped", path);
	return rc;
}

int afb_api_so_add_pathset(const char *pathset, struct afb_apiset *apiset)
{
	static char sep[] = ":";
	char *ps, *p;

	ps = strdupa(pathset);
	for (;;) {
		p = strsep(&ps, sep);
		if (!p)
			return 0;
		if (afb_api_so_add_path(p, apiset) < 0)
			return -1;
	}
}

