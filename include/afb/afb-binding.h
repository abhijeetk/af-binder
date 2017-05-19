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

#include <stdarg.h>

/*****************************************************************************
 * This files is the main file to include for writing bindings dedicated to
 *
 *                      AFB-DAEMON
 *
 * Functions of bindings of afb-daemon are accessible by authorized clients
 * through the apis module of afb-daemon.
 *
 * A binding is a shared library. This shared library must have at least one
 * exported symbol for being registered in afb-daemon.
 *
 */

#define AFB_BINDING_LOWER_VERSION     1
#define AFB_BINDING_UPPER_VERSION     2
#define AFB_BINDING_DEFAULT_VERSION   1

#ifndef AFB_BINDING_VERSION
#define AFB_BINDING_VERSION   AFB_BINDING_DEFAULT_VERSION
#endif

/*
 * Some function of the library are exported to afb-daemon.
 */

#include "afb-auth.h"
#include "afb-req-itf.h"
#include "afb-event-itf.h"
#include "afb-service-itf.h"
#include "afb-daemon-itf.h"
#include "afb-binding-v1.h"
#include "afb-binding-v2.h"

