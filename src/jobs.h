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

#pragma once

extern int jobs_queue0(
		void *group,
		int timeout,
		void (*callback)(int signum));

extern int jobs_queue(
		void *group,
		int timeout,
		void (*callback)(int signum, void* arg),
		void *arg);

extern int jobs_queue2(
		void *group,
		int timeout,
		void (*callback)(int signum, void* arg1, void *arg2),
		void *arg1,
		void *arg2);

extern int jobs_queue3(
		void *group,
		int timeout,
		void (*callback)(int signum, void* arg1, void *arg2, void *arg3),
		void *arg1,
		void *arg2,
		void *arg3);

extern int jobs_invoke0(
		int timeout,
		void (*callback)(int signum));

extern int jobs_invoke(
		int timeout,
		void (*callback)(int signum, void* arg),
		void *arg);

extern int jobs_invoke2(
		int timeout,
		void (*callback)(int signum, void* arg1, void *arg2),
		void *arg1,
		void *arg2);

extern int jobs_invoke3(
		int timeout,
		void (*callback)(int signum, void* arg1, void *arg2, void *arg3),
		void *arg1,
		void *arg2,
		void *arg3);

extern int jobs_add_events(void *key, int timeout, void (*events)(int, void*), void *closure);
extern int jobs_del_events(void *key);

extern int jobs_init(int allowed_count, int start_count, int waiter_count);
extern int jobs_add_me();
extern void jobs_terminate();


