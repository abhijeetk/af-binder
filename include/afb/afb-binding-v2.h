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

#include "afb-session-v2.h"

struct afb_service;
struct afb_daemon;
struct afb_binding_v2;

struct json_object;

/*
 * A binding V2 MUST have two exported symbols of name:
 *
 *            -  afbBindingV2
 *            -  afbBindingV2verbosity
 *
 */
extern const struct afb_binding_v2 afbBindingV2;
extern int afbBindingV2verbosity;

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
	int (*preinit)(struct afb_daemon daemon);
	int (*init)(struct afb_service service);
	void (*onevent)(struct afb_service service, const char *event, struct json_object *object);
	unsigned concurrent: 1;			/* allows concurrent requests to verbs */
};

/*
 * Macros for logging messages
 */
#if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO)
# if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DETAILS)
#  define AFB_ERROR_V2(daemon,...)   do{if(afbBindingV2verbosity>=0)afb_daemon_verbose(daemon,3,__FILE__,__LINE__,__VA_ARGS__);}while(0)
#  define AFB_WARNING_V2(daemon,...) do{if(afbBindingV2verbosity>=1)afb_daemon_verbose(daemon,4,__FILE__,__LINE__,__VA_ARGS__);}while(0)
#  define AFB_NOTICE_V2(daemon,...)  do{if(afbBindingV2verbosity>=1)afb_daemon_verbose(daemon,5,__FILE__,__LINE__,__VA_ARGS__);}while(0)
#  define AFB_INFO_V2(daemon,...)    do{if(afbBindingV2verbosity>=2)afb_daemon_verbose(daemon,6,__FILE__,__LINE__,__VA_ARGS__);}while(0)
#  define AFB_DEBUG_V2(daemon,...)   do{if(afbBindingV2verbosity>=3)afb_daemon_verbose(daemon,7,__FILE__,__LINE__,__VA_ARGS__);}while(0)
# else
#  define AFB_ERROR_V2(daemon,...)   do{if(afbBindingV2verbosity>=0)afb_daemon_verbose(daemon,3,NULL,0,__VA_ARGS__);}while(0)
#  define AFB_WARNING_V2(daemon,...) do{if(afbBindingV2verbosity>=1)afb_daemon_verbose(daemon,4,NULL,0,__VA_ARGS__);}while(0)
#  define AFB_NOTICE_V2(daemon,...)  do{if(afbBindingV2verbosity>=1)afb_daemon_verbose(daemon,5,NULL,0,__VA_ARGS__);}while(0)
#  define AFB_INFO_V2(daemon,...)    do{if(afbBindingV2verbosity>=2)afb_daemon_verbose(daemon,6,NULL,0,__VA_ARGS__);}while(0)
#  define AFB_DEBUG_V2(daemon,...)   do{if(afbBindingV2verbosity>=3)afb_daemon_verbose(daemon,7,NULL,0,__VA_ARGS__);}while(0)
# endif
#endif

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

#endif
