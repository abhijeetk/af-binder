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

#include "afb-debug.h"

#if defined(AFB_INSERT_DEBUG_FEATURES)

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "verbose.h"

static char key_env_break[] = "AFB_DEBUG_BREAK";
static char key_env_wait[] = "AFB_DEBUG_WAIT";
static char separs[] = ", \t\n";

static int has_key(const char *key, const char *list)
{
	if (list && key) {
		list += strspn(list, separs);
		while (*list) {
			size_t s = strcspn(list, separs);
			if (!strncasecmp(list, key, s) && !key[s])
				return 1;
			list += s;
			list += strspn(list, separs);
		}
	}
	return 0;
}

static void handler(int signum)
{
}

void afb_debug(const char *key)
{
	enum { None, Break, Wait } action;

	if (has_key(key, secure_getenv(key_env_break)))
		action = Break;
	else if (has_key(key, secure_getenv(key_env_wait)))
		action = Wait;
	else
		action = None;

	if (action != None) {
		const char *a = action == Break ? "BREAK" : "WAIT";
		struct sigaction sa, psa;
		sigset_t ss;

		NOTICE("DEBUG %s before %s", a, key);
		memset(&sa, 0, sizeof sa);
		sa.sa_handler = handler;
		sigaction(SIGINT, &sa, &psa);
		if (action == Break) {
			raise(SIGINT);
		} else {
			sigemptyset(&ss);
			sigaddset(&ss, SIGINT);
			sigwaitinfo(&ss, NULL);
		}
		sigaction(SIGINT, &psa, NULL);
		NOTICE("DEBUG %s after %s", a, key);
	}
}

#endif

