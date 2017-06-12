/*
 Copyright 2017 IoT.bzh

 author: José Bollo <jose.bollo@iot.bzh>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#define _GNU_SOURCE

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <systemd/sd-daemon.h>

static int init_done;
static char *null;
static char **names;

int sd_fds_init()
{
	int rc;

	if (init_done)
		rc = 0;
	else {
		init_done = 1;
		rc = sd_listen_fds_with_names(1, &names);
		if (rc <= 0) {
			errno = -rc;
			rc = -!!rc;
			names = &null;
		}
	}
	return rc;
}

int sd_fds_count()
{
	int count;

	assert(init_done);
	for (count = 0 ; names[count] != NULL ; count++);
	return count;
}

int sd_fds_for(const char *name)
{
	int idx;

	assert(init_done);
	for (idx = 0 ; names[idx] != NULL ; idx++)
		if (!strcmp(name, names[idx]))
			return idx + SD_LISTEN_FDS_START;

	errno = ENOENT;
	return -1;
}

