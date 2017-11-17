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

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#include <systemd/sd-event.h>

#include "jobs.h"
#include "sig-monitor.h"
#include "verbose.h"

#include "jobs.h"

struct jobloop;

struct job
{
	struct job *next;
	const void *group;
	int timeout;
	void (*callback)(int signum, void* arg);
	void *closure;
};

static struct job *first, *last;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int add_job(const void *group, int timeout, void (*callback)(int signum, void *closure), void *closure)
{
	struct job *j;

	j = malloc(sizeof*j);
	if (!j) {
		errno = ENOMEM;
		return -1;
	}

	j->next = 0;
	j->group = group;
	j->timeout = timeout;
	j->callback = callback;
	j->closure = closure;

	pthread_mutex_lock(&mutex);
	if (first)
		last->next = j;
	else
		first = j;
	last = j;
	pthread_mutex_unlock(&mutex);	
	return 0;
}

static void *thrrun(void *arg)
{
	struct job *j;

	pthread_mutex_lock(&mutex);
	j = first;
	if (j)
		first = j->next;
	pthread_mutex_unlock(&mutex);	
	if (j) {
		j->callback(0, j->closure);
		free(j);
	}
	return 0;
}

int jobs_queue(
	const void *group,
	int timeout,
	void (*callback)(int signum, void* arg),
	void *arg)
{
	pthread_t tid;
	int rc = add_job(group, timeout, callback, arg);
	if (!rc) {
		rc = pthread_create(&tid, NULL, thrrun, NULL);
		if (rc)
			rc = -1;
	}
	return rc;
}

#if 0
int jobs_enter(
	const void *group,
	int timeout,
	void (*callback)(int signum, void *closure, struct jobloop *jobloop),
	void *closure)
{
	return 0;
}

int jobs_leave(struct jobloop *jobloop)
{
	return 0;
}

int jobs_call(
	const void *group,
	int timeout,
	void (*callback)(int, void*),
	void *arg)
{
	return 0;
}

struct sd_event *jobs_get_sd_event()
{
	struct sd_event *r;
	int rc = sd_event_default(&r);
	return rc < 0 ? NULL : r;
}

void jobs_terminate()
{
}

int jobs_start(int allowed_count, int start_count, int waiter_count, void (*start)(int signum))
{
	start(0);
	return 0;
}
#endif
