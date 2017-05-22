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

#include <stdint.h>

#include "afb-auth.h"
#include "afb-req-itf.h"
#include "afb-event-itf.h"
#include "afb-service-common.h"
#include "afb-daemon-common.h"

#include "afb-session-v2.h"

struct json_object;

/*
 * Description of one verb of the API provided by the binding
 * This enumeration is valid for bindings of type version 2
 */
struct afb_verb_v2
{
	const char *verb;                       /* name of the verb */
	void (*callback)(struct afb_req req);   /* callback function implementing the verb */
	const struct afb_auth *auth;		/* required authorisation */
	uint32_t session;                       /* authorisation and session requirements of the verb */
};

/*
 * Description of the bindings of type version 2
 */
struct afb_binding_v2
{
	const char *api;			/* api name for the binding */
	const char *specification;		/* textual specification of the binding */
	const struct afb_verb_v2 *verbs;	/* array of descriptions of verbs terminated by a NULL name */
	int (*preinit)();
	int (*init)();
	void (*onevent)(const char *event, struct json_object *object);
	unsigned noconcurrency: 1;		/* avoids concurrent requests to verbs */
};

struct afb_binding_data_v2
{
	int verbosity;			/* level of verbosity */
	struct afb_daemon daemon;	/* access to daemon APIs */
	struct afb_service service;	/* access to service APIs */
};

/*
 * A binding V2 MUST have two exported symbols of name:
 *
 *            -  afbBindingV2
 *            -  afbBindingV2data
 *
 */
#if !defined(AFB_BINDING_MAIN_NAME_V2)
extern const struct afb_binding_v2 afbBindingV2;
#endif

#if !defined(AFB_BINDING_DATA_NAME_V2)
#define AFB_BINDING_DATA_NAME_V2 afbBindingV2data
#endif

#if AFB_BINDING_VERSION == 2
struct afb_binding_data_v2 AFB_BINDING_DATA_NAME_V2  __attribute__ ((weak));
#else
extern struct afb_binding_data_v2 AFB_BINDING_DATA_NAME_V2;
#endif

#define afb_get_verbosity_v2()	(AFB_BINDING_DATA_NAME_V2.verbosity)
#define afb_get_daemon_v2()	(AFB_BINDING_DATA_NAME_V2.daemon)
#define afb_get_service_v2()	(AFB_BINDING_DATA_NAME_V2.service)

/*
 * Macros for logging messages
 */
#if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO)
# if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DETAILS)
#  define _AFB_LOGGING_V2_(vlevel,llevel,...) \
	do{ \
		if(AFB_BINDING_DATA_NAME_V2.verbosity>=vlevel) \
			afb_daemon_verbose_v2(llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
	}while(0)
# else
#  define _AFB_LOGGING_V2_(vlevel,llevel,...) \
	do{ \
		if(afbBindingV2data.verbosity>=vlevel) \
			afb_daemon_verbose_v2(llevel,NULL,0,NULL,__VA_ARGS__); \
	}while(0)
# endif
# define AFB_ERROR_V2(...)   _AFB_LOGGING_V2_(0,3,__VA_ARGS__)
# define AFB_WARNING_V2(...) _AFB_LOGGING_V2_(1,4,__VA_ARGS__)
# define AFB_NOTICE_V2(...)  _AFB_LOGGING_V2_(1,5,__VA_ARGS__)
# define AFB_INFO_V2(...)    _AFB_LOGGING_V2_(2,6,__VA_ARGS__)
# define AFB_DEBUG_V2(...)   _AFB_LOGGING_V2_(3,7,__VA_ARGS__)
#endif

#include "afb-daemon-v2.h"
#include "afb-service-v2.h"

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

#endif
