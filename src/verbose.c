/*
 Copyright (C) 2016, 2017 "IoT.bzh"

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

#include <stdio.h>
#include <stdarg.h>

#include "verbose.h"

#if !defined(DEFAULT_VERBOSITY)
# define DEFAULT_VERBOSITY Verbosity_Level_Warning
#endif

int verbosity = 1;
void (*verbose_observer)(int loglevel, const char *file, int line, const char *function, const char *fmt, va_list args);

#define CROP_LOGLEVEL(x) ((x) < Log_Level_Emergency ? Log_Level_Emergency : (x) > Log_Level_Debug ? Log_Level_Debug : (x))

#if defined(VERBOSE_WITH_SYSLOG)

#include <syslog.h>

static void _vverbose_(int loglevel, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	char *p;

	if (file == NULL || vasprintf(&p, fmt, args) < 0)
		vsyslog(loglevel, fmt, args);
	else {
		syslog(CROP_LOGLEVEL(loglevel), "%s [%s:%d, function]", p, file, line, function);
		free(p);
	}
}

void verbose_set_name(const char *name, int authority)
{
	openlog(name, LOG_PERROR, authority ? LOG_AUTH : LOG_USER);
}

#elif defined(VERBOSE_WITH_SYSTEMD)

#define SD_JOURNAL_SUPPRESS_LOCATION

#include <systemd/sd-journal.h>

static const char *appname;

static int appauthority;

static void _vverbose_(int loglevel, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	char lino[20];

	if (file == NULL) {
		sd_journal_printv(loglevel, fmt, args);
	} else {
		sprintf(lino, "%d", line);
		sd_journal_printv_with_location(loglevel, file, lino, function, fmt, args);
	}
}

void verbose_set_name(const char *name, int authority)
{
	appname = name;
	appauthority = authority;
}

#else

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <pthread.h>

static const char *appname;

static int appauthority;

static const char *prefixes[] = {
	"<0> EMERGENCY",
	"<1> ALERT",
	"<2> CRITICAL",
	"<3> ERROR",
	"<4> WARNING",
	"<5> NOTICE",
	"<6> INFO",
	"<7> DEBUG"
};

static int tty;

static const char chars[] = { '\n', '?', ':', ' ', '[', ',', ']' };

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void _vverbose_(int loglevel, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	char buffer[4000];
	char lino[40];
	int saverr, n, rc;
	struct iovec iov[20];

	saverr = errno;

	if (!tty)
		tty = 1 + isatty(STDERR_FILENO);

	iov[0].iov_base = (void*)prefixes[CROP_LOGLEVEL(loglevel)] + (tty - 1 ? 4 : 0);
	iov[0].iov_len = strlen(iov[0].iov_base);

	iov[1].iov_base = (void*)&chars[2];
	iov[1].iov_len = 2;

	n = 2;
	if (fmt) {
		iov[n].iov_base = buffer;
		errno = saverr;
		rc = vsnprintf(buffer, sizeof buffer, fmt, args);
		if (rc < 0)
			rc = 0;
		else if ((size_t)rc > sizeof buffer) {
			rc = (int)sizeof buffer;
			buffer[rc - 1] = buffer[rc - 2]  = buffer[rc - 3] = '.';
		}
		iov[n++].iov_len = (size_t)rc;
	}
	if (file && (!fmt || tty == 1 || loglevel <= Log_Level_Warning)) {
		iov[n].iov_base = (void*)&chars[3 + !fmt];
		iov[n++].iov_len = 2 - !fmt;
		iov[n].iov_base = (void*)file;
		iov[n++].iov_len = strlen(file);
		iov[n].iov_base = (void*)&chars[2];
		iov[n++].iov_len = 1;
		if (line) {
			iov[n].iov_base = lino;
			iov[n++].iov_len = snprintf(lino, sizeof lino, "%d", line);
		} else {
			iov[n].iov_base = (void*)&chars[1];
			iov[n++].iov_len = 1;
		}
		iov[n].iov_base = (void*)&chars[5];
		iov[n++].iov_len = 1;
		if (function) {
			iov[n].iov_base = (void*)function;
			iov[n++].iov_len = strlen(function);
		} else {
			iov[n].iov_base = (void*)&chars[1];
			iov[n++].iov_len = 1;
		}
		iov[n].iov_base = (void*)&chars[6];
		iov[n++].iov_len = 1;
	}
	if (n == 2) {
		iov[n].iov_base = (void*)&chars[1];
		iov[n++].iov_len = 1;
	}
	iov[n].iov_base = (void*)&chars[0];
	iov[n++].iov_len = 1;

	pthread_mutex_lock(&mutex);
	writev(STDERR_FILENO, iov, n);
	pthread_mutex_unlock(&mutex);

	errno = saverr;
}

void verbose_set_name(const char *name, int authority)
{
	appname = name;
	appauthority = authority;
}

#endif

void verbose(int loglevel, const char *file, int line, const char *function, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vverbose(loglevel, file, line, function, fmt, ap);
	va_end(ap);
}

void vverbose(int loglevel, const char *file, int line, const char *function, const char *fmt, va_list args)
{
	void (*observer)(int loglevel, const char *file, int line, const char *function, const char *fmt, va_list args) = verbose_observer;

	if (!observer)
		_vverbose_(loglevel, file, line, function, fmt, args);
	else {
		va_list ap;
		va_copy(ap, args);
		_vverbose_(loglevel, file, line, function, fmt, args);
		observer(loglevel, file, line, function, fmt, ap);
		va_end(ap);
	}
}

