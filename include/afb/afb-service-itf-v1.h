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
struct afb_service;

/*
 * When a binding have an exported implementation of the
 * function 'afbBindingV1ServiceInit', defined below,
 * the framework calls it for initialising the service after
 * registration of all bindings.
 *
 * The object 'service' should be recorded. It has functions that
 * allows the binding to call features with its own personality.
 *
 * The function should return 0 in case of success or, else, should return
 * a negative value.
 */
extern int afbBindingV1ServiceInit(struct afb_service service);

/*
 * When a binding have an implementation of the function 'afbBindingV1ServiceEvent',
 * defined below, the framework calls that function for any broadcasted event or for
 * events that the service subscribed to in its name.
 *
 * It receive the 'event' name and its related data in 'object' (be aware that 'object'
 * might be NULL).
 */
extern void afbBindingV1ServiceEvent(const char *event, struct json_object *object);


