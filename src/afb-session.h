/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
 * Author: José Bollo <jose.bollo@iot.bzh>
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

struct json_object;
struct afb_session;

extern void afb_session_init(int max_session_count, int timeout, const char *initok, int context_count);

extern struct afb_session *afb_session_create (const char *uuid, int timeout);
extern struct afb_session *afb_session_get (const char *uuid, int *created);
extern const char *afb_session_uuid (struct afb_session *session);

extern struct afb_session *afb_session_addref(struct afb_session *session);
extern void afb_session_unref(struct afb_session *session);

extern void afb_session_close(struct afb_session *session);

extern int afb_session_check_token(struct afb_session *session, const char *token);
extern void afb_session_new_token(struct afb_session *session);
extern const char *afb_session_token(struct afb_session *session);

extern unsigned afb_session_get_LOA(struct afb_session *session);
extern void afb_session_set_LOA (struct afb_session *session, unsigned loa);

extern void *afb_session_get_value(struct afb_session *session, int index);
extern void afb_session_set_value(struct afb_session *session, int index, void *value, void (*freecb)(void*));

extern void *afb_session_get_cookie(struct afb_session *session, const void *key);
extern int afb_session_set_cookie(struct afb_session *session, const void *key, void *value, void (*freecb)(void*));

