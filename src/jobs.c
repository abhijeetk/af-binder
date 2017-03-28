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

/** control of threads */
struct thread
{
	struct thread *next;  /**< next thread of the list */
	struct thread *upper; /**< upper same thread */
	struct job *job;      /**< currently processed job */
	pthread_t tid;        /**< the thread id */
	unsigned stop: 1;     /**< stop requested */
	unsigned lowered: 1;  /**< has a lower same thread */
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
static _Thread_local struct thread *current;

/* queue of pending jobs */
static struct job *first_job;
static struct job *first_evloop;
static struct job *free_jobs;

/**
 * Create a new job with the given parameters
 * @param group the group of the job
 * @param timeout the timeout of the job (0 if none)
 * @param callback the function that achieves the job
 * @param arg1 the first argument of the callback
 * @param arg2 the second argument of the callback
 * @param arg3 the third argument of the callback
 * @return the created job unblock or NULL when no more memory
 */
static struct job *job_create(
		void *group,
		int timeout,
		void (*callback)(int, void*, void *, void*),
		void *arg1,
		void *arg2,
		void *arg3)
{
	struct job *job;

	/* try recyle existing job */
	job = free_jobs;
	if (job)
		free_jobs = job->next;
	else {
		/* allocation  without blocking */
		pthread_mutex_unlock(&mutex);
		job = malloc(sizeof *job);
		pthread_mutex_lock(&mutex);
		if (!job) {
			errno = -ENOMEM;
			goto end;
		}
	}
	/* initialises the job */
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

/**
 * Adds 'job1' and 'job2' at the end of the list of jobs, marking it
 * as blocked if an other job with the same group is pending.
 * @param job1 the first job to add
 * @param job2 the second job to add or NULL
 */
static void job_add2(struct job *job1, struct job *job2)
{
	void *group1, *group2, *group;
	struct job *ijob, **pjob;

	/* prepare to add */
	group1 = job1->group;
	job1->next = job2;
	if (!job2)
		group2 = NULL;
	else {
		job2->next = NULL;
		group2 = job2->group;
		if (group2 && group2 == group1)
			job2->blocked = 1;
	}

	/* search end and blackers */
	pjob = &first_job;
	ijob = first_job;
	while (ijob) {
		group = ijob->group;
		if (group) {
			if (group == group1)
				job1->blocked = 1;
			if (group == group2)
				job2->blocked = 1;
		}
		pjob = &ijob->next;
		ijob = ijob->next;
	}

	/* queue the jobs */
	*pjob = job1;
}

/**
 * Get the next job to process or NULL if none.
 * The returned job if any isn't removed from
 * the list of jobs.
 * @return the job to process
 */
static inline struct job *job_get()
{
	struct job *job;

	job = first_job;
	while (job && job->blocked)
		job = job->next;
	return job;
}

/**
 * Releases the processed 'job'
 * @param job the job to release
 */
static inline void job_release(struct job *job)
{
	struct job *ijob, **pjob;
	void *group;

	/* first unqueue the job */
	pjob = &first_job;
	ijob = first_job;
	while (ijob != job) {
		pjob = &ijob->next;
		ijob = ijob->next;
	}
	*pjob = job->next;

	/* then unblock jobs of the same group */
	group = job->group;
	if (group) {
		ijob = job->next;
		while (ijob && ijob->group != group)
			ijob = ijob->next;
		if (ijob)
			ijob->blocked = 0;
	}

	/* recycle the job */
	job->next = free_jobs;
	free_jobs = job;
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
static void thread_run(struct thread *me)
{
	struct thread **prv;
	struct job *job;

	/* init */
	me->tid = pthread_self();
	me->stop = 0;
	me->lowered = 0;
	me->upper = current;
	if (current)
		current->lowered = 1;
	else
		sig_monitor_init_timeouts();
	current = me;
	me->next = threads;
	threads = me;

	/* loop until stopped */
	running++;
	while (!me->stop) {
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
			remains++;
			job->blocked = 1;
			me->job = job;
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

	/* uninit */
	prv = &threads;
	while (*prv != me)
		prv = &(*prv)->next;
	*prv = me->next;
	current = me->upper;
	if (current)
		current->lowered = 0;
	else
		sig_monitor_clean_timeouts();
	pthread_mutex_unlock(&mutex);
}

/* main loop of processing threads */
static void *thread_create(void *data)
{
	struct thread me;

	pthread_mutex_lock(&mutex);
	thread_run(&me);
	pthread_mutex_unlock(&mutex);
	return NULL;
}

/* start a new thread */
static int start_one_thread()
{
	pthread_t tid;
	int rc;

	assert(started < allowed);

	started++;
	rc = pthread_create(&tid, NULL, thread_create, NULL);
	if (rc != 0) {
		started--;
		errno = rc;
		WARNING("not able to start thread: %m");
		rc = -1;
	}
	return rc;
}

static int start_one_thread_if_needed()
{
	int rc;

	if (started == running && started < allowed) {
		/* all threads are busy and a new can be started */
		rc = start_one_thread();
		if (rc < 0 && started == 0)
			return rc; /* no thread available */
	}
	return 0;
}

int jobs_queue0(
		void *group,
		int timeout,
		void (*callback)(int signum))
{
	return jobs_queue3(group, timeout, (void(*)(int,void*,void*,void*))callback, NULL, NULL, NULL);
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

	/* check availability */
	if (remains == 0) {
		errno = EBUSY;
		info = "too many jobs";
		goto error2;
	}

	/* start a thread if needed */
	rc = start_one_thread_if_needed();
	if (rc < 0) {
		/* failed to start threading */
		info = "can't start first thread";
		goto error2;
	}

	/* queues the job */
	remains--;
	job_add2(job, NULL);
	pthread_mutex_unlock(&mutex);

	/* signal an existing job */
	pthread_cond_signal(&cond);
	return 0;

error2:
	job->next = free_jobs;
	free_jobs = job;
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

int jobs_invoke0(
		int timeout,
		void (*callback)(int signum))
{
	return jobs_invoke3(timeout, (void(*)(int,void*,void*,void*))callback, NULL, NULL, NULL);
}

int jobs_invoke(
		int timeout,
		void (*callback)(int, void*),
		void *arg)
{
	return jobs_invoke3(timeout, (void(*)(int,void*,void*,void*))callback, arg, NULL, NULL);
}

int jobs_invoke2(
		int timeout,
		void (*callback)(int, void*, void*),
		void *arg1,
		void *arg2)
{
	return jobs_invoke3(timeout, (void(*)(int,void*,void*,void*))callback, arg1, arg2, NULL);
}

static void unlock_invoker(int signum, void *arg1, void *arg2, void *arg3)
{
	struct thread *t = arg1;
	pthread_mutex_lock(&mutex);
	t->stop = 1;
	pthread_mutex_unlock(&mutex);
}

/* invoke the job to the 'callback' using a separate thread if available */
int jobs_invoke3(
		int timeout,
		void (*callback)(int, void*, void *, void*),
		void *arg1,
		void *arg2,
		void *arg3)
{
	const char *info;
	struct job *job1, *job2;
	int rc;
	struct thread me;
	
	pthread_mutex_lock(&mutex);

	/* allocates the job */
	job1 = job_create(&me, timeout, callback, arg1, arg2, arg3);
	job2 = job_create(&me, 0, unlock_invoker, &me, NULL, NULL);
	if (!job1 || !job2) {
		errno = ENOMEM;
		info = "out of memory";
		goto error;
	}

	/* start a thread if needed */
	rc = start_one_thread_if_needed();
	if (rc < 0) {
		/* failed to start threading */
		info = "can't start first thread";
		goto error;
	}

	/* queues the job */
	job_add2(job1, job2);

	/* run untill stopped */
	thread_run(&me);
	pthread_mutex_unlock(&mutex);
	return 0;

error:
	if (job1) {
		job1->next = free_jobs;
		free_jobs = job1;
	}
	if (job2) {
		job2->next = free_jobs;
		free_jobs = job2;
	}
	ERROR("can't process job with threads: %s, %m", info);
	pthread_mutex_unlock(&mutex);
	return -1;
}

/* terminate all the threads and all pending requests */
void jobs_terminate()
{
	struct job *job, *head, *tail;
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

	/* cancel pending jobs of other threads */
	head = first_job;
	first_job = NULL;
	tail = NULL;
	while (head) {
		/* unlink the job */
		job = head;
		head = job->next;

		/* search if job is stacked for current */
		t = current;
		while (t && t->job != job)
			t = t->upper;
		if (t) {
			/* yes, relink it at end */
			if (tail)
				tail->next = job;
			else
				first_job = job;
			tail = job;
			job->next = NULL;
		} else {
			/* no cancel the job */
			pthread_mutex_unlock(&mutex);
			sig_monitor(0, job_cancel, job);
			free(job);
			pthread_mutex_lock(&mutex);
		}
	}
	pthread_mutex_unlock(&mutex);
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
	struct thread me;

	/* check whether already running */
	if (current) {
		ERROR("thread already running");
		errno = EINVAL;
		return -1;
	}

	/* allowed... */
	pthread_mutex_lock(&mutex);
	allowed++;
	thread_run(&me);
	allowed--;
	pthread_mutex_unlock(&mutex);
	return 0;
}


