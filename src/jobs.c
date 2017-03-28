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

/** control of threads */
struct thread
{
	struct thread *next; /**< next thread of the list */
	pthread_t tid;     /**< the thread id */
	unsigned stop: 1;  /**< stop request */
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

/* count allowed, started and running threads */
static int allowed = 0; /** allowed count of threads */
static int started = 0; /** started count of threads */
static int running = 0; /** running count of threads */
static int remains = 0; /** remaining count of jobs that can be created */

/* list of threads */
static struct thread *threads;

/* queue of pending jobs */
static struct job *first_job;
static struct job *first_evloop;
static struct job *free_jobs;

/**
 * Adds the 'job' at the end of the list of jobs, marking it
 * as blocked if an other job with the same group is pending.
 * @param job the job to add
 */
static inline void job_add(struct job *job)
{
	void *group;
	struct job *ijob, **pjob;

	pjob = &first_job;
	ijob = first_job;
	group = job->group ? : (void*)(intptr_t)1;
	while (ijob) {
		if (ijob->group == group)
			job->blocked = 1;
		pjob = &ijob->next;
		ijob = ijob->next;
	}
	job->next = NULL;
	*pjob = job;
	remains--;
}

/**
 * Get the next job to process or NULL if none.
 * The returned job if any is removed from the list of
 * jobs.
 * @return the job to process
 */
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

/**
 * Unblock the first pending job of a group (if any)
 * @param group the group to unblock
 */
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

static struct job *job_create(
		void *group,
		int timeout,
		void (*callback)(int, void*, void *, void*),
		void *arg1,
		void *arg2,
		void *arg3)
{
	struct job *job;

	/* allocates the job */
	job = free_jobs;
	if (job)
		free_jobs = job->next;
	else {
		pthread_mutex_unlock(&mutex);
		job = malloc(sizeof *job);
		pthread_mutex_lock(&mutex);
		if (!job) {
			errno = -ENOMEM;
			goto end;
		}
	}
	job->group = group;
	job->timeout = timeout;
	job->callback = callback;
	job->arg1 = arg1;
	job->arg2 = arg2;
	job->arg3 = arg3;
	job->blocked = 0;
end:
	return job;
}

static inline void job_destroy(struct job *job)
{
	job->next = free_jobs;
	free_jobs = job;
}

static inline void job_release(struct job *job)
{
	if (job->group)
		job_unblock(job->group);
	job_destroy(job);
}

/** monitored call to the job */
static void job_call(int signum, void *arg)
{
	struct job *job = arg;
	job->callback(signum, job->arg1, job->arg2, job->arg3);
}

/** monitored cancel of the job */
static void job_cancel(int signum, void *arg)
{
	job_call(SIGABRT, arg);
}

/* main loop of processing threads */
static void *thread_main_loop(void *data)
{
	struct thread me, **prv;
	struct job *job;

	/* init */
	me.tid = pthread_self();
	me.stop = 0;
	sig_monitor_init_timeouts();

	/* chain in */
	pthread_mutex_lock(&mutex);
	me.next = threads;
	threads = &me;

	/* loop until stopped */
	running++;
	while (!me.stop) {
		/* get a job */
		job = job_get();
		if (!job && first_job && running == 0) {
			/* sad situation!! should not happen */
			ERROR("threads are blocked!");
			job = first_job;
			first_job = job->next;
		}
		if (job) {
			/* run the job */
			pthread_mutex_unlock(&mutex);
			sig_monitor(job->timeout, job_call, job);
			pthread_mutex_lock(&mutex);
			job_release(job);
		} else {
			/* no job, check evloop */
			job = first_evloop;
			if (job) {
				/* evloop */
				first_evloop = job->next;
				pthread_mutex_unlock(&mutex);
				sig_monitor(job->timeout, job_call, job);
				pthread_mutex_lock(&mutex);
				job->next = first_evloop;
				first_evloop = job;
			} else {
				/* no job and not evloop */
				running--;
				pthread_cond_wait(&cond, &mutex);
				running++;
			}
		}
	}
	running--;

	/* chain out */
	prv = &threads;
	while (*prv != &me)
		prv = &(*prv)->next;
	*prv = me.next;
	pthread_mutex_unlock(&mutex);

	/* uninit and terminate */
	sig_monitor_clean_timeouts();
	return NULL;
}

/* start a new thread */
static int start_one_thread()
{
	pthread_t tid;
	int rc;

	assert(started < allowed);

	started++;
	rc = pthread_create(&tid, NULL, thread_main_loop, NULL);
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

	pthread_mutex_lock(&mutex);

	/* allocates the job */
	job = job_create(group, timeout, callback, arg1, arg2, arg3);
	if (!job) {
		errno = ENOMEM;
		info = "out of memory";
		goto error;
	}

	/* start a thread if needed */
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

	/* queues the job */
	job_add(job);
	pthread_mutex_unlock(&mutex);

	/* signal an existing job */
	pthread_cond_signal(&cond);
	return 0;

error2:
	job_destroy(job);
error:
	ERROR("can't process job with threads: %s, %m", info);
	pthread_mutex_unlock(&mutex);
	return -1;
}

/* initialise the threads */
int jobs_init(int allowed_count, int start_count, int waiter_count)
{
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
void jobs_terminate(int wait)
{
	struct job *job;
	pthread_t me, other;
	struct thread *t;

	/* how am i? */
	me = pthread_self();

	/* request all threads to stop */
	pthread_mutex_lock(&mutex);
	allowed = 0;
	for(;;) {
		/* search the next thread to stop */
		t = threads;
		while (t && pthread_equal(t->tid, me))
			t = t->next;
		if (!t)
			break;
		/* stop it */
		other = t->tid;
		t->stop = 1;
		pthread_mutex_unlock(&mutex);
		pthread_cond_broadcast(&cond);
		pthread_join(other, NULL);
		pthread_mutex_lock(&mutex);
	}

	/* cancel pending jobs */
	while (first_job) {
		job = first_job;
		first_job = job->next;
		sig_monitor(0, job_cancel, job);
		free(job);
	}
}

int jobs_add_event_loop(void *key, int timeout, void (*evloop)(int signum, void*), void *closure)
{
	struct job *job;

	pthread_mutex_lock(&mutex);
	job = job_create(key, timeout, (void (*)(int,  void *, void *, void *))evloop, closure, NULL, NULL);
	if (job) {
		/* adds the loop */
		job->next = first_evloop;
		first_evloop = job;

		/* signal the loop */
		pthread_cond_signal(&cond);
	}
	pthread_mutex_unlock(&mutex);
	return -!job;
}

int jobs_add_me()
{
	pthread_t me;
	struct thread *t;

	/* how am i? */
	me = pthread_self();

	/* request all threads to stop */
	pthread_mutex_lock(&mutex);
	t = threads;
	while (t) {
		if (pthread_equal(t->tid, me)) {
			pthread_mutex_unlock(&mutex);
			ERROR("thread already running");
			errno = EINVAL;
			return -1;
		}
		t = t->next;
	}

	/* allowed... */
	allowed++;
	pthread_mutex_unlock(&mutex);

	/* run */
	thread_main_loop(NULL);

	/* returns */
	pthread_mutex_lock(&mutex);
	allowed--;
	pthread_mutex_unlock(&mutex);
	return 0;
}


