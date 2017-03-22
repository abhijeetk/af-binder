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

#include <string.h>

#include <afb/afb-req-itf.h>

#include "afb-thread.h"
#include "jobs.h"
#include "sig-monitor.h"
#include "verbose.h"

static void req_call(int signum, void *arg1, void *arg2, void *arg3)
{
	struct afb_req req = { .itf = arg1, .closure = arg2 };
	void (*callback)(struct afb_req) = arg3;

	if (signum != 0)
		afb_req_fail_f(req, "aborted", "signal %s(%d) caught", strsignal(signum), signum);
	else
		callback(req);
	afb_req_unref(req);
}

void afb_thread_req_call(struct afb_req req, void (*callback)(struct afb_req req), int timeout, void *group)
{
	int rc;

	afb_req_addref(req);
	if (0) {
		/* no threading */
		sig_monitor3(timeout, req_call, (void*)req.itf, req.closure, callback);
	} else {
		/* threading */
		rc = jobs_queue3(group, timeout, req_call, (void*)req.itf, req.closure, callback);
		if (rc < 0) {
			/* TODO: allows or not to proccess it directly as when no threading? (see above) */
			ERROR("can't process job with threads: %m");
			afb_req_fail_f(req, "cancelled", "not able to pipe a job for the task");
			afb_req_unref(req);
		}
	}
}

