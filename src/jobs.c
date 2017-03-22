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
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#include "jobs.h"
#include "sig-monitor.h"
#include "verbose.h"

/* control of threads */
struct thread
{
	pthread_t tid;     /* the thread id */
	unsigned stop: 1;  /* stop request */
	unsigned ended: 1; /* ended status */
	unsigned works: 1; /* is it processing a job? */
};

/* describes pending job */
struct job
{
	struct job *next;   /* link to the next job enqueued */
	void *group;        /* group of the request */
	void (*callback)(int,void*,void*,void*);     /* processing callback */
	void *arg1;         /* first arg */
	void *arg2;         /* second arg */
	void *arg3;         /* second arg */
	int timeout;        /* timeout in second for processing the request */
	int blocked;        /* is an other request blocking this one ? */
};

/* synchronisation of threads */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;

/* queue of pending jobs */
static struct job *first_job = NULL;

/* count allowed, started and running threads */
static int allowed = 0;
static int started = 0;
static int running = 0;
static int remains = 0;

/* list of threads */
static struct thread *threads = NULL;

/* add the job to the list */
static inline void job_add(struct job *job)
{
	void *group = job->group;
	struct job *ijob, **pjob;

	pjob = &first_job;
	ijob = first_job;
	group = job->group ? : job;
	while (ijob) {
		if (ijob->group == group)
			job->blocked = 1;
		pjob = &ijob->next;
		ijob = ijob->next;
	}
	*pjob = job;
	job->next = NULL;
	remains--;
}

/* get the next job to process or NULL if none */
static inline struct job *job_get()
{
	struct job *job, **pjob;
	pjob = &first_job;
	job = first_job;
	while (job && job->blocked) {
		pjob = &job->next;
		job = job->next;
	}
	if (job) {
		*pjob = job->next;
		remains++;
	}
	return job;
}

/* unblock a group of job */
static inline void job_unblock(void *group)
{
	struct job *job;

	job = first_job;
	while (job) {
		if (job->group == group) {
			job->blocked = 0;
			break;
		}
		job = job->next;
	}
}

/* call the job */
static inline void job_call(int signum, void *arg)
{
	struct job *job = arg;
	job->callback(signum, job->arg1, job->arg2, job->arg3);
}

/* cancel the job */
static inline void job_cancel(int signum, void *arg)
{
	struct job *job = arg;
	job->callback(SIGABRT, job->arg1, job->arg2, job->arg3);
}

/* main loop of processing threads */
static void *thread_main_loop(void *data)
{
	struct thread *me = data;
	struct job *job;

	me->works = 0;
	me->ended = 0;
	sig_monitor_init_timeouts();
	pthread_mutex_lock(&mutex);
	while (!me->stop) {
		/* get a job */
		job = job_get();
		if (job == NULL && first_job != NULL && running == 0) {
			/* sad situation!! should not happen */
			ERROR("threads are blocked!");
			job = first_job;
			first_job = job->next;
		}
		if (job == NULL) {
			/* no job... */
			pthread_cond_wait(&cond, &mutex);
		} else {
			/* run the job */
			running++;
			me->works = 1;
			pthread_mutex_unlock(&mutex);
			sig_monitor(job->timeout, job_call, job);
			pthread_mutex_lock(&mutex);
			me->works = 0;
			running--;
			if (job->group != NULL)
				job_unblock(job->group);
			free(job);
		}

	}
	me->ended = 1;
	pthread_mutex_unlock(&mutex);
	sig_monitor_clean_timeouts();
	return me;
}

/* start a new thread */
static int start_one_thread()
{
	struct thread *t;
	int rc;

	assert(started < allowed);

	t = &threads[started++];
	t->stop = 0;
	rc = pthread_create(&t->tid, NULL, thread_main_loop, t);
	if (rc != 0) {
		started--;
		errno = rc;
		WARNING("not able to start thread: %m");
		rc = -1;
	}
	return rc;
}

int jobs_queue(
		void *group,
		int timeout,
		void (*callback)(int, void*),
		void *arg)
{
	return jobs_queue3(group, timeout, (void(*)(int,void*,void*,void*))callback, arg, NULL, NULL);
}

int jobs_queue2(
		void *group,
		int timeout,
		void (*callback)(int, void*, void*),
		void *arg1,
		void *arg2)
{
	return jobs_queue3(group, timeout, (void(*)(int,void*,void*,void*))callback, arg1, arg2, NULL);
}

/* queue the job to the 'callback' using a separate thread if available */
int jobs_queue3(
		void *group,
		int timeout,
		void (*callback)(int, void*, void *, void*),
		void *arg1,
		void *arg2,
		void *arg3)
{
	const char *info;
	struct job *job;
	int rc;

	/* allocates the job */
	job = malloc(sizeof *job);
	if (job == NULL) {
		errno = ENOMEM;
		info = "out of memory";
		goto error;
	}

	/* start a thread if needed */
	pthread_mutex_lock(&mutex);
	if (remains == 0) {
		errno = EBUSY;
		info = "too many jobs";
		goto error2;
	}
	if (started == running && started < allowed) {
		rc = start_one_thread();
		if (rc < 0 && started == 0) {
			/* failed to start threading */
			info = "can't start first thread";
			goto error2;
		}
	}

	/* fills and queues the job */
	job->group = group;
	job->timeout = timeout;
	job->callback = callback;
	job->arg1 = arg1;
	job->arg2 = arg2;
	job->arg3 = arg3;
	job->blocked = 0;
	job_add(job);
	pthread_mutex_unlock(&mutex);

	/* signal an existing job */
	pthread_cond_signal(&cond);
	return 0;

error2:
	pthread_mutex_unlock(&mutex);
	free(job);
error:
	ERROR("can't process job with threads: %s, %m", info);
	return -1;
}

/* initialise the threads */
int jobs_init(int allowed_count, int start_count, int waiter_count)
{
	threads = calloc(allowed_count, sizeof *threads);
	if (threads == NULL) {
		errno = ENOMEM;
		ERROR("can't allocate threads");
		return -1;
	}

	/* records the allowed count */
	allowed = allowed_count;
	started = 0;
	running = 0;
	remains = waiter_count;

	/* start at least one thread */
	pthread_mutex_lock(&mutex);
	while (started < start_count && start_one_thread() == 0);
	pthread_mutex_unlock(&mutex);

	/* end */
	return -(started != start_count);
}

/* terminate all the threads and all pending requests */
void jobs_terminate()
{
	int i, n;
	struct job *job;

	/* request all threads to stop */
	pthread_mutex_lock(&mutex);
	allowed = 0;
	n = started;
	for (i = 0 ; i < n ; i++)
		threads[i].stop = 1;

	/* wait until all thread are terminated */
	while (started != 0) {
		/* signal threads */
		pthread_mutex_unlock(&mutex);
		pthread_cond_broadcast(&cond);
		pthread_mutex_lock(&mutex);

		/* join the terminated threads */
		for (i = 0 ; i < n ; i++) {
			if (threads[i].tid && threads[i].ended) {
				pthread_join(threads[i].tid, NULL);
				threads[i].tid = 0;
				started--;
			}
		}
	}
	pthread_mutex_unlock(&mutex);
	free(threads);

	/* cancel pending jobs */
	while (first_job) {
		job = first_job;
		first_job = job->next;
		sig_monitor(0, job_cancel, job);
		free(job);
	}
}

