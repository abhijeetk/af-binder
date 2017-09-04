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
 */

#define AFB_BINDING_LOWER_VERSION     1
#define AFB_BINDING_UPPER_VERSION     2

#ifndef AFB_BINDING_VERSION
#define AFB_BINDING_VERSION   1
#pragma GCC warning "\
\n\
\n\
  AFB_BINDING_VERSION should be defined before including <afb/afb-binding.h>\n\
  AFB_BINDING_VERSION defines the version of binding that you use.\n\
  Currently, known versions are 1 or 2.\n\
  Setting now AFB_BINDING_VERSION to 1 (version 1 by default)\n\
  NOTE THAT VERSION 2 IS NOW RECOMMENDED!\n\
  Consider to add one of the following define before including <afb/afb-binding.h>:\n\
\n\
    #define AFB_BINDING_VERSION 1\n\
    #define AFB_BINDING_VERSION 2\n\
\n\
  Note that in some case it will enforce you to include <stdio.h>\n\
"
#include <stdio.h> /* old version side effect */
#else
#  if AFB_BINDING_VERSION == 1
#    pragma GCC warning "Using binding version 1, consider to switch to version 2"
#  endif
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

typedef struct afb_verb_desc_v1         afb_verb_desc_v1;
typedef struct afb_binding_desc_v1      afb_binding_desc_v1;
typedef struct afb_binding_v1           afb_binding_v1;
typedef struct afb_binding_interface_v1 afb_binding_interface_v1;

typedef struct afb_verb_v2              afb_verb_v2;
typedef struct afb_binding_v2           afb_binding_v2;

typedef enum   afb_auth_type            afb_auth_type;
typedef struct afb_auth                 afb_auth;
typedef struct afb_daemon               afb_daemon;
typedef struct afb_event                afb_event;
typedef struct afb_arg                  afb_arg;
typedef struct afb_req                  afb_req;
typedef struct afb_stored_req           afb_stored_req;
typedef struct afb_service              afb_service;

#if 0
/* these typedef's shouldn't be needed */
typedef enum   afb_binding_type_v1      afb_binding_type_v1;
typedef enum   afb_mode_v1              afb_mode_v1;
typedef enum   afb_session_flags_v1     afb_session_flags_v1;
typedef enum   afb_session_flags_v2     afb_session_flags_v2;
typedef struct afb_binding_data_v2      afb_binding_data_v2;
typedef struct afb_daemon_itf           afb_daemon_itf;
typedef struct afb_event_itf            afb_event_itf;
typedef struct afb_req_itf              afb_req_itf;
typedef struct afb_service_itf          afb_service_itf;
#endif

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

# define AFB_ERROR		AFB_ERROR_V1
# define AFB_WARNING		AFB_WARNING_V1
# define AFB_NOTICE		AFB_NOTICE_V1
# define AFB_INFO		AFB_INFO_V1
# define AFB_DEBUG		AFB_DEBUG_V1

# define AFB_REQ_ERROR		AFB_REQ_ERROR_V1
# define AFB_REQ_WARNING	AFB_REQ_WARNING_V1
# define AFB_REQ_NOTICE		AFB_REQ_NOTICE_V1
# define AFB_REQ_INFO		AFB_REQ_INFO_V1
# define AFB_REQ_DEBUG		AFB_REQ_DEBUG_V1

#define afb_daemon_get_event_loop	afb_daemon_get_event_loop_v1
#define afb_daemon_get_user_bus		afb_daemon_get_user_bus_v1
#define afb_daemon_get_system_bus	afb_daemon_get_system_bus_v1
#define afb_daemon_broadcast_event	afb_daemon_broadcast_event_v1
#define afb_daemon_make_event		afb_daemon_make_event_v1
#define afb_daemon_verbose		afb_daemon_verbose_v1
#define afb_daemon_rootdir_get_fd	afb_daemon_rootdir_get_fd_v1
#define afb_daemon_rootdir_open_locale	afb_daemon_rootdir_open_locale_v1
#define afb_daemon_queue_job		afb_daemon_queue_job_v1
#define afb_daemon_require_api		afb_daemon_require_api_v1

#define afb_service_call		afb_service_call_v1
#define afb_service_call_sync		afb_service_call_sync_v1

#define afb_req_store			afb_req_store_v1
#define afb_req_unstore			afb_req_unstore_v1

#endif

/***************************************************************************************************/

#if AFB_BINDING_VERSION == 2

# define afb_binding		afb_binding_v2
# define afb_get_verbosity	afb_get_verbosity_v2
# define afb_get_daemon		afb_get_daemon_v2
# define afb_get_service	afb_get_service_v2


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

# define AFB_ERROR		AFB_ERROR_V2
# define AFB_WARNING		AFB_WARNING_V2
# define AFB_NOTICE		AFB_NOTICE_V2
# define AFB_INFO		AFB_INFO_V2
# define AFB_DEBUG		AFB_DEBUG_V2

# define AFB_REQ_ERROR		AFB_REQ_ERROR_V2
# define AFB_REQ_WARNING	AFB_REQ_WARNING_V2
# define AFB_REQ_NOTICE		AFB_REQ_NOTICE_V2
# define AFB_REQ_INFO		AFB_REQ_INFO_V2
# define AFB_REQ_DEBUG		AFB_REQ_DEBUG_V2

#define afb_daemon_get_event_loop	afb_daemon_get_event_loop_v2
#define afb_daemon_get_user_bus		afb_daemon_get_user_bus_v2
#define afb_daemon_get_system_bus	afb_daemon_get_system_bus_v2
#define afb_daemon_broadcast_event	afb_daemon_broadcast_event_v2
#define afb_daemon_make_event		afb_daemon_make_event_v2
#define afb_daemon_verbose		afb_daemon_verbose_v2
#define afb_daemon_rootdir_get_fd	afb_daemon_rootdir_get_fd_v2
#define afb_daemon_rootdir_open_locale	afb_daemon_rootdir_open_locale_v2
#define afb_daemon_queue_job		afb_daemon_queue_job_v2
#define afb_daemon_unstore_req		afb_daemon_unstore_req_v2
#define afb_daemon_require_api		afb_daemon_require_api_v2

#define afb_service_call		afb_service_call_v2
#define afb_service_call_sync		afb_service_call_sync_v2

#define afb_req_store			afb_req_store_v2
#define afb_req_unstore			afb_daemon_unstore_req_v2

#endif

/***************************************************************************************************/

#if AFB_BINDING_VERSION >= 2

# define afb_verbose_error()	(afb_get_verbosity() >= AFB_VERBOSITY_LEVEL_ERROR)
# define afb_verbose_warning()	(afb_get_verbosity() >= AFB_VERBOSITY_LEVEL_WARNING)
# define afb_verbose_notice()	(afb_get_verbosity() >= AFB_VERBOSITY_LEVEL_NOTICE)
# define afb_verbose_info()	(afb_get_verbosity() >= AFB_VERBOSITY_LEVEL_INFO)
# define afb_verbose_debug()	(afb_get_verbosity() >= AFB_VERBOSITY_LEVEL_DEBUG)

#endif

/***************************************************************************************************/

#if defined(AFB_BINDING_PRAGMA_KEEP_VERBOSE_UNPREFIX)
# define ERROR			AFB_ERROR
# define WARNING		AFB_WARNING
# define NOTICE			AFB_NOTICE
# define INFO			AFB_INFO
# define DEBUG			AFB_DEBUG

# define REQ_ERROR		AFB_REQ_ERROR
# define REQ_WARNING		AFB_REQ_WARNING
# define REQ_NOTICE		AFB_REQ_NOTICE
# define REQ_INFO		AFB_REQ_INFO
# define REQ_DEBUG		AFB_REQ_DEBUG
#endif

