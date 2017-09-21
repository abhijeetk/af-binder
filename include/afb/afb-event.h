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

#include "afb-eventid-itf.h"

/*
 * Describes the request of afb-daemon for bindings
 */
struct afb_event
{
	const struct afb_eventid_itf *itf;	/* the interface to use */
	struct afb_eventid *closure;		/* the closure argument for functions of 'itf' */
};

/*
 * Checks wether the 'event' is valid or not.
 *
 * Returns 0 if not valid or 1 if valid.
 */
static inline int afb_event_is_valid(struct afb_event event)
{
	return !!event.itf;
}

/*
 * Broadcasts widely the 'event' with the data 'object'.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
static inline int afb_event_broadcast(struct afb_event event, struct json_object *object)
{
	return event.itf->broadcast(event.closure, object);
}

/*
 * Pushes the 'event' with the data 'object' to its observers.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
static inline int afb_event_push(struct afb_event event, struct json_object *object)
{
	return event.itf->push(event.closure, object);
}

/* OBSOLETE */
#define afb_event_drop afb_event_unref

/*
 * Gets the name associated to the 'event'.
 */
static inline const char *afb_event_name(struct afb_event event)
{
	return event.itf->name(event.closure);
}

/*
 * Decreases the count of reference to 'event' and
 * destroys the event when the reference count falls to zero.
 */
static inline void afb_event_unref(struct afb_event event)
{
	event.itf->unref(event.closure);
}

/*
 * Increases the count of reference to 'event'
 */
static inline void afb_event_addref(struct afb_event event)
{
	event.itf->addref(event.closure);
}

