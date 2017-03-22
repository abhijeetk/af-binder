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

#pragma once

extern int sig_monitor_init();
extern void sig_monitor_clean_timeouts();
extern int sig_monitor_init_timeouts();

extern void sig_monitor(int timeout, void (*function)(int sig, void*), void *arg);
extern void sig_monitor2(int timeout, void (*function)(int sig, void*, void*), void *arg1, void *arg2);
extern void sig_monitor3(int timeout, void (*function)(int sig, void*, void*, void*), void *arg1, void *arg2, void *arg3);

