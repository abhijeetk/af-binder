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

#include <systemd/sd-event.h>

#include "jobs.h"
#include "sig-monitor.h"
#include "verbose.h"

#if 0
#define _alert_ "do you really want to remove monitoring?"
#define sig_monitor_init_timeouts()  ((void)0)
#define sig_monitor_clean_timeouts() ((void)0)
#define sig_monitor(to,cb,arg)       (cb(0,arg))
#endif

/** Internal shortcut for callback */
typedef void (*job_cb_t)(int, void*, void *, void*);

/** Description of a pending job */
struct job
{
	struct job *next;    /**< link to the next job enqueued */
	void *group;         /**< group of the request */
	job_cb_t callback;   /**< processing callback */
	void *arg1;          /**< first arg */
	void *arg2;          /**< second arg */
	void *arg3;          /**< third arg */
	int timeout;         /**< timeout in second for processing the request */
	unsigned blocked: 1; /**< is an other request blocking this one ? */
	unsigned dropped: 1; /**< is removed ? */
};

/** Description of handled event loops */
struct events
{
	struct events *next;
	struct sd_event *event;
	unsigned runs: 1;
};

/** Description of threads */
struct thread
{
	struct thread *next;   /**< next thread of the list */
	struct thread *upper;  /**< upper same thread */
	struct job *job;       /**< currently processed job */
	struct events *events; /**< currently processed job */
	pthread_t tid;         /**< the thread id */
	unsigned stop: 1;      /**< stop requested */
	unsigned lowered: 1;   /**< has a lower same thread */
	unsigned waits: 1;     /**< is waiting? */
};

/* synchronisation of threads */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;

/* count allowed, started and waiting threads */
static int allowed = 0; /** allowed count of threads */
static int started = 0; /** started count of threads */
static int waiting = 0; /** waiting count of threads */
static int remains = 0; /** allowed count of waiting jobs */
static int nevents = 0; /** count of events */

/* list of threads */
static struct thread *threads;
static _Thread_local struct thread *current;

/* queue of pending jobs */
static struct job *first_job;
static struct events *first_events;
static struct job *free_jobs;

/**
 * Create a new job with the given parameters
 * @param group    the group of the job
 * @param timeout  the timeout of the job (0 if none)
 * @param callback the function that achieves the job
 * @param arg1     the first argument of the callback
 * @param arg2     the second argument of the callback
 * @param arg3     the third argument of the callback
 * @return the created job unblock or NULL when no more memory
 */
static struct job *job_create(
		void *group,
		int timeout,
		job_cb_t callback,
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
	job->dropped = 0;
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

	/* search end and blockers */
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
 * @return the first job that isn't blocked or NULL
 */
static inline struct job *job_get()
{
	struct job *job = first_job;
	while (job && job->blocked)
		job = job->next;
	return job;
}

/**
 * Get the next events to process or NULL if none.
 * @return the first events that isn't running or NULL
 */
static inline struct events *events_get()
{
	struct events *events = first_events;
	while (events && events->runs)
		events = events->next;
	return events;
}

/**
 * Releases the processed 'job': removes it
 * from the list of jobs and unblock the first
 * pending job of the same group if any.
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

/**
 * Monitored normal callback for a job.
 * This function is called by the monitor
 * to run the job when the safe environment
 * is set.
 * @param signum 0 on normal flow or the number
 *               of the signal that interrupted the normal
 *               flow
 * @param arg     the job to run
 */
static void job_call(int signum, void *arg)
{
	struct job *job = arg;
	job->callback(signum, job->arg1, job->arg2, job->arg3);
}

/**
 * Monitored cancel callback for a job.
 * This function is called by the monitor
 * to cancel the job when the safe environment
 * is set.
 * @param signum 0 on normal flow or the number
 *               of the signal that interrupted the normal
 *               flow, isn't used
 * @param arg    the job to run
 */
static void job_cancel(int signum, void *arg)
{
	job_call(SIGABRT, arg);
}

/**
 * Monitored normal callback for events.
 * This function is called by the monitor
 * to run the event loop when the safe environment
 * is set.
 * @param signum 0 on normal flow or the number
 *               of the signal that interrupted the normal
 *               flow
 * @param arg     the events to run
 */
static void events_call(int signum, void *arg)
{
	struct events *events = arg;
	if (!signum)
		sd_event_run(events->event, (uint64_t) -1);
}

/**
 * Main processing loop of threads processing jobs.
 * The loop must be called with the mutex locked
 * and it returns with the mutex locked.
 * @param me the description of the thread to use
 * TODO: how are timeout handled when reentering?
 */
static void thread_run(volatile struct thread *me)
{
	struct thread **prv;
	struct job *job;
	struct events *events;

	/* initialize description of itself and link it in the list */
	me->tid = pthread_self();
	me->stop = 0;
	me->lowered = 0;
	me->waits = 0;
	me->upper = current;
	if (current)
		current->lowered = 1;
	else
		sig_monitor_init_timeouts();
	current = (struct thread*)me;
	me->next = threads;
	threads = (struct thread*)me;
	started++;

	/* loop until stopped */
	me->events = NULL;
	while (!me->stop) {
		/* get a job */
		job = job_get(first_job);
		if (job) {
			/* prepare running the job */
			remains++; /* increases count of job that can wait */
			job->blocked = 1; /* mark job as blocked */
			me->job = job; /* record the job (only for terminate) */

			/* run the job */
			pthread_mutex_unlock(&mutex);
			sig_monitor(job->timeout, job_call, job);
			pthread_mutex_lock(&mutex);

			/* release the run job */
			job_release(job);

			/* release event if any */
			events = me->events;
			if (events) {
				events->runs = 0;
				me->events = NULL;
			}
		} else {
			/* no job, check events */
			events = events_get();
			if (events) {
				/* run the events */
				events->runs = 1;
				me->events = events;
				pthread_mutex_unlock(&mutex);
				sig_monitor(0, events_call, events);
				pthread_mutex_lock(&mutex);
				events->runs = 0;
				me->events = NULL;
			} else {
				/* no job and not events */
				waiting++;
				me->waits = 1;
				pthread_cond_wait(&cond, &mutex);
				me->waits = 0;
				waiting--;
			}
		}
	}

	/* unlink the current thread and cleanup */
	started--;
	prv = &threads;
	while (*prv != me)
		prv = &(*prv)->next;
	*prv = me->next;
	current = me->upper;
	if (current)
		current->lowered = 0;
	else
		sig_monitor_clean_timeouts();
}

/**
 * Entry point for created threads.
 * @param data not used
 * @return NULL
 */
static void *thread_main(void *data)
{
	struct thread me;

	pthread_mutex_lock(&mutex);
	thread_run(&me);
	pthread_mutex_unlock(&mutex);
	return NULL;
}

/**
 * Starts a new thread
 * @return 0 in case of success or -1 in case of error
 */
static int start_one_thread()
{
	pthread_t tid;
	int rc;

	rc = pthread_create(&tid, NULL, thread_main, NULL);
	if (rc != 0) {
		/* errno = rc; */
		WARNING("not able to start thread: %m");
		rc = -1;
	}
	return rc;
}

/**
 * Queues a new asynchronous job represented by 'callback'
 * for the 'group' and the 'timeout'.
 * Jobs are queued FIFO and are possibly executed in parallel
 * concurrently except for job of the same group that are
 * executed sequentially in FIFO order.
 * @param group    The group of the job or NULL when no group.
 * @param timeout  The maximum execution time in seconds of the job
 *                 or 0 for unlimited time.
 * @param callback The function to execute for achieving the job.
 *                 Its first parameter is either 0 on normal flow
 *                 or the signal number that broke the normal flow.
 * @return 0 in case of success or -1 in case of error
 */
int jobs_queue0(
		void *group,
		int timeout,
		void (*callback)(int signum))
{
	return jobs_queue3(group, timeout, (job_cb_t)callback, NULL, NULL, NULL);
}

/**
 * Queues a new asynchronous job represented by 'callback' and 'arg1'
 * for the 'group' and the 'timeout'.
 * Jobs are queued FIFO and are possibly executed in parallel
 * concurrently except for job of the same group that are
 * executed sequentially in FIFO order.
 * @param group    The group of the job or NULL when no group.
 * @param timeout  The maximum execution time in seconds of the job
 *                 or 0 for unlimited time.
 * @param callback The function to execute for achieving the job.
 *                 Its first parameter is either 0 on normal flow
 *                 or the signal number that broke the normal flow.
 *                 The remaining parameter is the parameter 'arg1'
 *                 given here.
 * @param arg1     The second argument for 'callback'
 * @return 0 in case of success or -1 in case of error
 */
int jobs_queue(
		void *group,
		int timeout,
		void (*callback)(int, void*),
		void *arg)
{
	return jobs_queue3(group, timeout, (job_cb_t)callback, arg, NULL, NULL);
}

/**
 * Queues a new asynchronous job represented by 'callback' and 'arg[12]'
 * for the 'group' and the 'timeout'.
 * Jobs are queued FIFO and are possibly executed in parallel
 * concurrently except for job of the same group that are
 * executed sequentially in FIFO order.
 * @param group    The group of the job or NULL when no group.
 * @param timeout  The maximum execution time in seconds of the job
 *                 or 0 for unlimited time.
 * @param callback The function to execute for achieving the job.
 *                 Its first parameter is either 0 on normal flow
 *                 or the signal number that broke the normal flow.
 *                 The remaining parameters are the parameters 'arg[12]'
 *                 given here.
 * @param arg1     The second argument for 'callback'
 * @param arg2     The third argument for 'callback'
 * @return 0 in case of success or -1 in case of error
 */
int jobs_queue2(
		void *group,
		int timeout,
		void (*callback)(int, void*, void*),
		void *arg1,
		void *arg2)
{
	return jobs_queue3(group, timeout, (job_cb_t)callback, arg1, arg2, NULL);
}

/**
 * Queues a new asynchronous job represented by 'callback' and 'arg[123]'
 * for the 'group' and the 'timeout'.
 * Jobs are queued FIFO and are possibly executed in parallel
 * concurrently except for job of the same group that are
 * executed sequentially in FIFO order.
 * @param group    The group of the job or NULL when no group.
 * @param timeout  The maximum execution time in seconds of the job
 *                 or 0 for unlimited time.
 * @param callback The function to execute for achieving the job.
 *                 Its first parameter is either 0 on normal flow
 *                 or the signal number that broke the normal flow.
 *                 The remaining parameters are the parameters 'arg[123]'
 *                 given here.
 * @param arg1     The second argument for 'callback'
 * @param arg2     The third argument for 'callback'
 * @param arg3     The forth argument for 'callback'
 * @return 0 in case of success or -1 in case of error
 */
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
	if (waiting == 0 && started < allowed) {
		/* all threads are busy and a new can be started */
		rc = start_one_thread();
		if (rc < 0 && started == 0) {
			info = "can't start first thread";
			goto error2;
		}
	}

	/* queues the job */
	remains--;
	job_add2(job, NULL);

	/* signal an existing job */
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
	return 0;

error2:
	job->next = free_jobs;
	free_jobs = job;
error:
	ERROR("can't process job with threads: %s, %m", info);
	pthread_mutex_unlock(&mutex);
	return -1;
}

/**
 * Run a asynchronous job represented by 'callback'
 * with the 'timeout' but only returns after job completion.
 * @param timeout  The maximum execution time in seconds of the job
 *                 or 0 for unlimited time.
 * @param callback The function to execute for achieving the job.
 *                 Its first parameter is either 0 on normal flow
 *                 or the signal number that broke the normal flow.
 * @return 0 in case of success or -1 in case of error
 */
int jobs_invoke0(
		int timeout,
		void (*callback)(int signum))
{
	return jobs_invoke3(timeout, (job_cb_t)callback, NULL, NULL, NULL);
}

/**
 * Run a asynchronous job represented by 'callback' and 'arg1'
 * with the 'timeout' but only returns after job completion.
 * @param timeout  The maximum execution time in seconds of the job
 *                 or 0 for unlimited time.
 * @param callback The function to execute for achieving the job.
 *                 Its first parameter is either 0 on normal flow
 *                 or the signal number that broke the normal flow.
 *                 The remaining parameter is the parameter 'arg1'
 *                 given here.
 * @param arg1     The second argument for 'callback'
 * @return 0 in case of success or -1 in case of error
 */
int jobs_invoke(
		int timeout,
		void (*callback)(int, void*),
		void *arg)
{
	return jobs_invoke3(timeout, (job_cb_t)callback, arg, NULL, NULL);
}

/**
 * Run a asynchronous job represented by 'callback' and 'arg[12]'
 * with the 'timeout' but only returns after job completion.
 * @param timeout  The maximum execution time in seconds of the job
 *                 or 0 for unlimited time.
 * @param callback The function to execute for achieving the job.
 *                 Its first parameter is either 0 on normal flow
 *                 or the signal number that broke the normal flow.
 *                 The remaining parameters are the parameters 'arg[12]'
 *                 given here.
 * @param arg1     The second argument for 'callback'
 * @param arg2     The third argument for 'callback'
 * @return 0 in case of success or -1 in case of error
 */
int jobs_invoke2(
		int timeout,
		void (*callback)(int, void*, void*),
		void *arg1,
		void *arg2)
{
	return jobs_invoke3(timeout, (job_cb_t)callback, arg1, arg2, NULL);
}

/**
 * Stops the thread pointed by 'arg1'. Used with
 * invoke familly to return to the caller after completion.
 * @param signum Unused
 * @param arg1   The thread to stop
 * @param arg2   Unused
 * @param arg3   Unused
 */
static void unlock_invoker(int signum, void *arg1, void *arg2, void *arg3)
{
	struct thread *t = arg1;
	pthread_mutex_lock(&mutex);
	t->stop = 1;
	if (t->waits)
		pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);
}

/**
 * Run a asynchronous job represented by 'callback' and 'arg[123]'
 * with the 'timeout' but only returns after job completion.
 * @param timeout  The maximum execution time in seconds of the job
 *                 or 0 for unlimited time.
 * @param callback The function to execute for achieving the job.
 *                 Its first parameter is either 0 on normal flow
 *                 or the signal number that broke the normal flow.
 *                 The remaining parameters are the parameters 'arg[123]'
 *                 given here.
 * @param arg1     The second argument for 'callback'
 * @param arg2     The third argument for 'callback'
 * @param arg3     The forth argument for 'callback'
 * @return 0 in case of success or -1 in case of error
 */
int jobs_invoke3(
		int timeout,
		void (*callback)(int, void*, void *, void*),
		void *arg1,
		void *arg2,
		void *arg3)
{
	struct job *job1, *job2;
	struct thread me;
	
	pthread_mutex_lock(&mutex);

	/* allocates the job */
	job1 = job_create(&me, timeout, callback, arg1, arg2, arg3);
	job2 = job_create(&me, 0, unlock_invoker, &me, NULL, NULL);
	if (!job1 || !job2) {
		ERROR("out of memory");
		errno = ENOMEM;
		if (job1) {
			job1->next = free_jobs;
			free_jobs = job1;
		}
		if (job2) {
			job2->next = free_jobs;
			free_jobs = job2;
		}
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	/* queues the job */
	job_add2(job1, job2);

	/* run until stopped */
	thread_run(&me);
	pthread_mutex_unlock(&mutex);
	return 0;
}

/**
 * Initialise the job stuff.
 * @param allowed_count Maximum count of thread for jobs (can be 0,
 *                      see 'jobs_add_me' for merging new threads)
 * @param start_count   Count of thread to start now, must be lower.
 * @param waiter_count  Maximum count of jobs that can be waiting.
 * @return 0 in case of success or -1 in case of error.
 */
int jobs_init(int allowed_count, int start_count, int waiter_count)
{
	int rc, launched;

	assert(allowed_count >= 0);
	assert(start_count >= 0);
	assert(waiter_count > 0);
	assert(start_count <= allowed_count);

	/* records the allowed count */
	allowed = allowed_count;
	started = 0;
	waiting = 0;
	remains = waiter_count;

	/* start at least one thread */
	pthread_mutex_lock(&mutex);
	launched = 0;
	while (launched < start_count && start_one_thread() == 0)
		launched++;
	rc = -(launched != start_count);
	pthread_mutex_unlock(&mutex);

	/* end */
	if (rc)
		ERROR("Not all threads can be started");
	return rc;
}

/**
 * Terminate all the threads and cancel all pending jobs.
 */
void jobs_terminate()
{
	struct job *job, *head, *tail;
	pthread_t me, *others;
	struct thread *t;
	int count;

	/* how am i? */
	me = pthread_self();

	/* request all threads to stop */
	pthread_mutex_lock(&mutex);
	allowed = 0;

	/* count the number of threads */
	count = 0;
	t = threads;
	while (t) {
		if (!t->upper && !pthread_equal(t->tid, me))
			count++;
		t = t->next;
	}

	/* fill the array of threads */
	others = alloca(count * sizeof *others);
	count = 0;
	t = threads;
	while (t) {
		if (!t->upper && !pthread_equal(t->tid, me))
			others[count++] = t->tid;
		t = t->next;
	}

	/* stops the threads */
	t = threads;
	while (t) {
		t->stop = 1;
		t = t->next;
	}

	/* wait the threads */
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);
	while (count)
		pthread_join(others[--count], NULL);
	pthread_mutex_lock(&mutex);

	/* cancel pending jobs of other threads */
	remains = 0;
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

/**
 * Adds the current thread to the pool of threads
 * processing the jobs. Returns normally when the threads are
 * terminated or immediately with an error if the thread is
 * already in the pool.
 * @return 0 in case of success or -1 in case of error
 */
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


struct sd_event *jobs_get_sd_event()
{
	struct events *events;
	struct thread *me;
	int rc;

	pthread_mutex_lock(&mutex);

	/* search events on stack */
	me = current;
	while (me && !me->events)
		me = me->upper;
	if (me)
		/* return the stacked events */
		events = me->events;
	else {
		/* search an available events */
		events = events_get();
		if (!events) {
			/* not found, check if creation possible */
			if (nevents >= allowed) {
				ERROR("not possible to add a new event");
				events = NULL;
			} else {
				events = malloc(sizeof *events);
				if (events && (rc = sd_event_new(&events->event)) >= 0) {
					if (nevents < started || start_one_thread() >= 0) {
						events->runs = 0;
						events->next = first_events;
						first_events = events;
					} else {
						ERROR("can't start thread for events");
						sd_event_unref(events->event);
						free(events);
						events = NULL;
					}
				} else {
					if (!events)
						ERROR("out of memory");
					else {
						free(events);
						ERROR("creation of sd_event failed: %m");
						events = NULL;
						errno = -rc;
					} 
				}
			}
		}
		if (events) {
			/* */
			me = current;
			if (me) {
				events->runs = 1;
				me->events = events;
			} else {
				WARNING("event returned for unknown thread!");
			}
		}
	}
	pthread_mutex_unlock(&mutex);
	return events ? events->event : NULL;
}

/**
 * run the jobs as 
 * @param allowed_count Maximum count of thread for jobs including this one
 * @param start_count   Count of thread to start now, must be lower.
 * @param waiter_count  Maximum count of jobs that can be waiting.
 * @param start         The start routine to activate (can't be NULL)
 * @return 0 in case of success or -1 in case of error.
 */
int jobs_enter(int allowed_count, int start_count, int waiter_count, void (*start)())
{
	/* start */
	if (sig_monitor_init() < 0) {
		ERROR("failed to initialise signal handlers");
		return -1;
	}

	/* init job processing */
	if (jobs_init(allowed_count, start_count, waiter_count) < 0) {
		ERROR("failed to initialise threading");
		return -1;
	}

	/* queue the start job */
	if (jobs_queue0(NULL, 0, (void(*)(int))start) < 0) {
		ERROR("failed to start runnning jobs");
		return -1;
	}

	/* turn as processing thread */
	return jobs_add_me();
};
