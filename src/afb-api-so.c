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
#define NO_BINDING_VERBOSE_MACRO

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include "afb-api-so.h"
#include "afb-api-so-v1.h"
#include "verbose.h"

int afb_api_so_timeout = 15;

void afb_api_so_set_timeout(int to)
{
	afb_api_so_timeout = to;
}

static int load_binding(const char *path, int force)
{
	int rc;
	void *handle;

	// This is a loadable library let's check if it's a binding
	rc = -!!force;
	handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (handle == NULL) {
		if (force)
			ERROR("binding [%s] not loadable: %s", path, dlerror());
		else
			INFO("binding [%s] not loadable: %s", path, dlerror());
		goto error;
	}

	/* retrieves the register function */
	rc = afb_api_so_v1_add(path, handle);
	if (rc < 0) {
		/* error when loading a valid v& binding */
		goto error2;
	}
	if (rc == 0) {
		/* not a v1 binding */
		if (force)
			ERROR("binding [%s] is not an AFB binding", path);
		else
			INFO("binding [%s] is not an AFB binding", path);
		goto error2;
	}
	return 0;

error2:
	dlclose(handle);
error:
	return rc;
}


int afb_api_so_add_binding(const char *path)
{
	return load_binding(path, 1);
}

static int adddirs(char path[PATH_MAX], size_t end)
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
					continue;
				if (dent->d_name[1] == '.' && len == 2)
					continue;
			}
			memcpy(&path[end], dent->d_name, len+1);
			adddirs(path, end+len);;
		} else if (dent->d_type == DT_REG) {
			/* case of files */
			if (memcmp(&dent->d_name[len - 3], ".so", 4))
				continue;
			memcpy(&path[end], dent->d_name, len+1);
			if (load_binding(path, 0) < 0)
				return -1;
		}
	}
	closedir(dir);
	return 0;
}

int afb_api_so_add_directory(const char *path)
{
	size_t length;
	char buffer[PATH_MAX];

	length = strlen(path);
	if (length >= sizeof(buffer)) {
		ERROR("path too long %lu [%.99s...]", (unsigned long)length, path);
		return -1;
	}

	memcpy(buffer, path, length + 1);
	return adddirs(buffer, length);
}

int afb_api_so_add_path(const char *path)
{
	struct stat st;
	int rc;

	rc = stat(path, &st);
	if (rc < 0)
		ERROR("Invalid binding path [%s]: %m", path);
	else if (S_ISDIR(st.st_mode))
		rc = afb_api_so_add_directory(path);
	else if (strstr(path, ".so"))
		rc = load_binding(path, 0);
	else
		INFO("not a binding [%s], skipped", path);
	return rc;
}

int afb_api_so_add_pathset(const char *pathset)
{
	static char sep[] = ":";
	char *ps, *p;

	ps = strdupa(pathset);
	for (;;) {
		p = strsep(&ps, sep);
		if (!p)
			return 0;
		if (afb_api_so_add_path(p) < 0)
			return -1;
	}
}

