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
 * Broadcasts widely an event of 'eventid' with the data 'object'.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
static inline int afb_eventid_broadcast(struct afb_eventid *eventid, struct json_object *object)
{
	return eventid->itf->broadcast(eventid, object);
}

/*
 * Pushes an event of 'eventid' with the data 'object' to its observers.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
static inline int afb_eventid_push(struct afb_eventid *eventid, struct json_object *object)
{
	return eventid->itf->push(eventid, object);
}

/*
 * Gets the name associated to 'eventid'.
 */
static inline const char *afb_eventid_name(struct afb_eventid *eventid)
{
	return eventid->itf->name(eventid);
}

/*
 * Decrease the count of reference to 'eventid' and
 * destroys the eventid when the reference count falls to zero.
 */
static inline void afb_eventid_unref(struct afb_eventid *eventid)
{
	eventid->itf->unref(eventid);
}

/*
 * Increases the count of reference to 'eventid'
 */
static inline struct afb_eventid *afb_eventid_addref(struct afb_eventid *eventid)
{
	return eventid->itf->addref(eventid);
}

