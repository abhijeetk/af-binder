/*
 * Copyright (C) 2016, 2017, 2018 "IoT.bzh"
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
struct afb_eventid;
struct afb_eventid_itf;

/*
 * Interface for handling requests.
 * It records the functions to be called for the request.
 * Don't use this structure directly.
 * Use the helper functions documented below.
 */
struct afb_eventid_itf
{
	/* CAUTION: respect the order, add at the end */

	int (*broadcast)(struct afb_eventid *eventid, struct json_object *obj);
	int (*push)(struct afb_eventid *eventid, struct json_object *obj);
	void (*unref)(struct afb_eventid *eventid); /* aka drop */
	const char *(*name)(struct afb_eventid *eventid);
	struct afb_eventid *(*addref)(struct afb_eventid *eventid);
};

/*
 * Describes the request of afb-daemon for bindings
 */
struct afb_eventid
{
	const struct afb_eventid_itf *itf;	/* the interface to use */
};

