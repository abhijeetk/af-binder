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

struct afb_perm;

extern struct afb_perm *afb_perm_parse(const char *desc);
extern void afb_perm_addref(struct afb_perm *perm);
extern void afb_perm_unref(struct afb_perm *perm);
extern int afb_perm_check(struct afb_perm *perm, int (*check)(void *closure, const char *name), void *closure);

