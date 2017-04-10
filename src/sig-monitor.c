/*
 * Copyright (C) 2017 "IoT.bzh"
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
#include <signal.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "sig-monitor.h"
#include "verbose.h"

/* local handler */
static _Thread_local sigjmp_buf *error_handler;

/* local timers */
static _Thread_local int thread_timer_set;
static _Thread_local timer_t thread_timerid;

/*
 * Creates a timer for the current thread
 *
 * Returns 0 in case of success
 */
static inline int timeout_create()
{
	int rc;
	struct sigevent sevp;

	if (thread_timer_set)
		rc = 0;
	else {
		sevp.sigev_notify = SIGEV_THREAD_ID;
		sevp.sigev_signo = SIGALRM;
		sevp.sigev_value.sival_ptr = NULL;
#if defined(sigev_notify_thread_id)
		sevp.sigev_notify_thread_id = (pid_t)syscall(SYS_gettid);
#else
		sevp._sigev_un._tid = (pid_t)syscall(SYS_gettid);
#endif
		rc = timer_create(CLOCK_THREAD_CPUTIME_ID, &sevp, &thread_timerid);
		thread_timer_set = !rc;
	}
	return 0;
}

/*
 * Arms the alarm in timeout seconds for the current thread
 */
static inline int timeout_arm(int timeout)
{
	int rc;
	struct itimerspec its;

	rc = timeout_create();
	if (rc == 0) {
		its.it_interval.tv_sec = 0;
		its.it_interval.tv_nsec = 0;
		its.it_value.tv_sec = timeout;
		its.it_value.tv_nsec = 0;
		rc = timer_settime(thread_timerid, 0, &its, NULL);
	}

	return rc;
}

/*
 * Disarms the current alarm
 */
static inline void timeout_disarm()
{
	if (thread_timer_set)
		timeout_arm(0);
}

/*
 * Destroy any alarm resource for the current thread
 */
static inline void timeout_delete()
{
	if (thread_timer_set) {
		timer_delete(thread_timerid);
		thread_timer_set = 0;
	}
}


/* Handles signals that terminate the process */
static void on_signal_terminate (int signum)
{
	ERROR("Terminating signal %d received: %s", signum, strsignal(signum));
	exit(1);
}

/* Handles monitored signals that can be continued */
static void on_signal_error(int signum)
{
	sigset_t sigset;

	ERROR("ALERT! signal %d received: %s", signum, strsignal(signum));

	// unlock signal to allow a new signal to come
	if (error_handler != NULL) {
		sigemptyset(&sigset);
		sigaddset(&sigset, signum);
		sigprocmask(SIG_UNBLOCK, &sigset, 0);
		longjmp(*error_handler, signum);
	}
	if (signum == SIGALRM)
		return;
	ERROR("Unmonitored signal %d received: %s", signum, strsignal(signum));
	exit(2);
}

/* install the handlers */
static int install(void (*handler)(int), int *signals)
{
	int result = 1;
	while(*signals > 0) {
		if (signal(*signals, handler) == SIG_ERR) {
			ERROR("failed to install signal handler for signal %s", strsignal(*signals));
			result = 0;
		}
		signals++;
	}
	return result;
}

int sig_monitor_init()
{
	static int sigerr[] = { SIGALRM, SIGSEGV, SIGFPE, 0 };
	static int sigterm[] = { SIGINT, SIGABRT, 0 };

	return (install(on_signal_error, sigerr) & install(on_signal_terminate, sigterm)) - 1;
}

int sig_monitor_init_timeouts()
{
	return timeout_create();
}

void sig_monitor_clean_timeouts()
{
	timeout_delete();
}

void sig_monitor(int timeout, void (*function)(int sig, void*), void *arg)
{
	sig_monitor3(timeout, (void (*)(int,void*,void*,void*))function, arg, NULL, NULL);
}

void sig_monitor2(int timeout, void (*function)(int sig, void*, void*), void *arg1, void *arg2)
{
	sig_monitor3(timeout, (void (*)(int,void*,void*,void*))function, arg1, arg2, NULL);
}

void sig_monitor3(int timeout, void (*function)(int sig, void*, void*, void*), void *arg1, void *arg2, void *arg3)
{
	volatile int signum, signum2;
	sigjmp_buf jmpbuf, *older;

	older = error_handler;
	signum = setjmp(jmpbuf);
	if (signum == 0) {
		error_handler = &jmpbuf;
		if (timeout)
			timeout_arm(timeout);
		function(0, arg1, arg2, arg3);
	} else {
		signum2 = setjmp(jmpbuf);
		if (signum2 == 0)
			function(signum, arg1, arg2, arg3);
	}
	error_handler = older;
	if (timeout)
		timeout_disarm();
}





