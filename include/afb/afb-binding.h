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

#if AFB_BINDING_VERSION != 0
# if AFB_BINDING_VERSION < AFB_BINDING_LOWER_VERSION || AFB_BINDING_VERSION > AFB_BINDING_UPPER_VERSION
#  error "Unsupported binding version AFB_BINDING_VERSION " #AFB_BINDING_VERSION
# endif
#endif

/*
 * Some function of the library are exported to afb-daemon.
 */

#include "afb-binding-v1.h"
#include "afb-binding-v2.h"

/***************************************************************************************************/

#if AFB_BINDING_VERSION == 1

# define afb_binding		afb_binding_v1
# define afb_binding_interface	afb_binding_interface_v1

# define AFB_SESSION_NONE	AFB_SESSION_NONE_V1
# define AFB_SESSION_CREATE	AFB_SESSION_CREATE_V1
# define AFB_SESSION_CLOSE	AFB_SESSION_CLOSE_V1
# define AFB_SESSION_RENEW	AFB_SESSION_RENEW_V1
# define AFB_SESSION_CHECK	AFB_SESSION_CHECK_V1

# define AFB_SESSION_LOA_GE	AFB_SESSION_LOA_GE_V1
# define AFB_SESSION_LOA_LE	AFB_SESSION_LOA_LE_V1
# define AFB_SESSION_LOA_EQ	AFB_SESSION_LOA_EQ_V1

# define AFB_SESSION_LOA_SHIFT	AFB_SESSION_LOA_SHIFT_V1
# define AFB_SESSION_LOA_MASK	AFB_SESSION_LOA_MASK_V1

# define AFB_SESSION_LOA_0	AFB_SESSION_LOA_0_V1
# define AFB_SESSION_LOA_1	AFB_SESSION_LOA_1_V1
# define AFB_SESSION_LOA_2	AFB_SESSION_LOA_2_V1
# define AFB_SESSION_LOA_3	AFB_SESSION_LOA_3_V1
# define AFB_SESSION_LOA_4	AFB_SESSION_LOA_4_V1

# define AFB_SESSION_LOA_LE_0	AFB_SESSION_LOA_LE_0_V1
# define AFB_SESSION_LOA_LE_1	AFB_SESSION_LOA_LE_1_V1
# define AFB_SESSION_LOA_LE_2	AFB_SESSION_LOA_LE_2_V1
# define AFB_SESSION_LOA_LE_3	AFB_SESSION_LOA_LE_3_V1

# define AFB_SESSION_LOA_EQ_0	AFB_SESSION_LOA_EQ_0_V1
# define AFB_SESSION_LOA_EQ_1	AFB_SESSION_LOA_EQ_1_V1
# define AFB_SESSION_LOA_EQ_2	AFB_SESSION_LOA_EQ_2_V1
# define AFB_SESSION_LOA_EQ_3	AFB_SESSION_LOA_EQ_3_V1

# define AFB_SESSION_LOA_GE_0	AFB_SESSION_LOA_GE_0_V1
# define AFB_SESSION_LOA_GE_1	AFB_SESSION_LOA_GE_1_V1
# define AFB_SESSION_LOA_GE_2	AFB_SESSION_LOA_GE_2_V1
# define AFB_SESSION_LOA_GE_3	AFB_SESSION_LOA_GE_3_V1

# if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO)

#  define ERROR			AFB_ERROR_V1
#  define WARNING		AFB_WARNING_V1
#  define NOTICE		AFB_NOTICE_V1
#  define INFO			AFB_INFO_V1
#  define DEBUG			AFB_DEBUG_V1

# endif

#define afb_daemon_get_event_loop	afb_daemon_get_event_loop_v1
#define afb_daemon_get_user_bus		afb_daemon_get_user_bus_v1
#define afb_daemon_get_system_bus	afb_daemon_get_system_bus_v1
#define afb_daemon_broadcast_event	afb_daemon_broadcast_event_v1
#define afb_daemon_make_event		afb_daemon_make_event_v1
#define afb_daemon_verbose		afb_daemon_verbose_v1
#define afb_daemon_rootdir_get_fd	afb_daemon_rootdir_get_fd_v1
#define afb_daemon_rootdir_open_locale	afb_daemon_rootdir_open_locale_v1
#define afb_daemon_queue_job		afb_daemon_queue_job_v1

#define afb_service_call		afb_service_call_v1
#define afb_service_call_sync		afb_service_call_sync_v1

#endif

/***************************************************************************************************/

#if AFB_BINDING_VERSION == 2

# define afb_binding		afb_binding_v2
# define afb_binding_interface	afb_binding_interface_v2

# define AFB_SESSION_NONE	AFB_SESSION_NONE_V2
# define AFB_SESSION_CLOSE	AFB_SESSION_CLOSE_V2
# define AFB_SESSION_RENEW	AFB_SESSION_REFRESH_V2
# define AFB_SESSION_REFRESH	AFB_SESSION_REFRESH_V2
# define AFB_SESSION_CHECK	AFB_SESSION_CHECK_V2

# define AFB_SESSION_LOA_MASK	AFB_SESSION_LOA_MASK_V2

# define AFB_SESSION_LOA_0	AFB_SESSION_LOA_0_V2
# define AFB_SESSION_LOA_1	AFB_SESSION_LOA_1_V2
# define AFB_SESSION_LOA_2	AFB_SESSION_LOA_2_V2
# define AFB_SESSION_LOA_3	AFB_SESSION_LOA_3_V2

# if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO)

#  define ERROR			AFB_ERROR_V2
#  define WARNING		AFB_WARNING_V2
#  define NOTICE		AFB_NOTICE_V2
#  define INFO			AFB_INFO_V2
#  define DEBUG			AFB_DEBUG_V2

# endif

#define afb_daemon_get_event_loop	afb_daemon_get_event_loop_v2
#define afb_daemon_get_user_bus		afb_daemon_get_user_bus_v2
#define afb_daemon_get_system_bus	afb_daemon_get_system_bus_v2
#define afb_daemon_broadcast_event	afb_daemon_broadcast_event_v2
#define afb_daemon_make_event		afb_daemon_make_event_v2
#define afb_daemon_verbose		afb_daemon_verbose_v2
#define afb_daemon_rootdir_get_fd	afb_daemon_rootdir_get_fd_v2
#define afb_daemon_rootdir_open_locale	afb_daemon_rootdir_open_locale_v2
#define afb_daemon_queue_job		afb_daemon_queue_job_v2

#define afb_service_call		afb_service_call_v2
#define afb_service_call_sync		afb_service_call_sync_v2

#endif
