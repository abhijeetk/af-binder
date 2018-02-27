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

#include <stdint.h>

#include "afb-auth.h"
#include "afb-event.h"
#include "afb-req.h"
#include "afb-service-itf.h"
#include "afb-daemon-itf.h"

#include "afb-req-v2.h"
#include "afb-session-v2.h"

struct json_object;

/*
 * Description of one verb of the API provided by the binding
 * This enumeration is valid for bindings of type version 2
 */
struct afb_verb_v2
{
	const char *verb;                       /* name of the verb, NULL only at end of the array */
	void (*callback)(struct afb_req req);   /* callback function implementing the verb */
	const struct afb_auth *auth;		/* required authorisation, can be NULL */
	const char *info;			/* some info about the verb, can be NULL */
	uint32_t session;                       /* authorisation and session requirements of the verb */
};

/*
 * Description of the bindings of type version 2
 */
struct afb_binding_v2
{
	const char *api;			/* api name for the binding */
	const char *specification;		/* textual specification of the binding, can be NULL */
	const char *info;			/* some info about the api, can be NULL */
	const struct afb_verb_v2 *verbs;	/* array of descriptions of verbs terminated by a NULL name */
	int (*preinit)();                       /* callback at load of the binding */
	int (*init)();                          /* callback for starting the service */
	void (*onevent)(const char *event, struct json_object *object); /* callback for handling events */
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

#if AFB_BINDING_VERSION != 2
extern
#endif
struct afb_binding_data_v2 AFB_BINDING_DATA_NAME_V2  __attribute__ ((weak));

#define afb_get_verbosity_v2()	(AFB_BINDING_DATA_NAME_V2.verbosity)
#define afb_get_daemon_v2()	(AFB_BINDING_DATA_NAME_V2.daemon)
#define afb_get_service_v2()	(AFB_BINDING_DATA_NAME_V2.service)

/*
 * Macros for logging messages
 */
#if defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DATA)

# define _AFB_LOGGING_V2_(vlevel,llevel,...) \
	do{ \
		if(AFB_BINDING_DATA_NAME_V2.verbosity>=vlevel) {\
			if (llevel <= AFB_VERBOSITY_LEVEL_ERROR) \
				afb_daemon_verbose_v2(llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
			else \
				afb_daemon_verbose_v2(llevel,__FILE__,__LINE__,NULL,NULL); \
		} \
	}while(0)
# define _AFB_REQ_LOGGING_V2_(vlevel,llevel,req,...) \
	do{ \
		if(AFB_BINDING_DATA_NAME_V2.verbosity>=vlevel) \
			afb_req_verbose(req,llevel,__FILE__,__LINE__,NULL,NULL); \
	}while(0)

#elif defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DETAILS)

# define _AFB_LOGGING_V2_(vlevel,llevel,...) \
	do{ \
		if(AFB_BINDING_DATA_NAME_V2.verbosity>=vlevel) \
			afb_daemon_verbose_v2(llevel,NULL,0,NULL,__VA_ARGS__); \
	}while(0)
# define _AFB_REQ_LOGGING_V2_(vlevel,llevel,req,...) \
	do{ \
		if(AFB_BINDING_DATA_NAME_V2.verbosity>=vlevel) \
			afb_req_verbose(req,llevel,NULL,0,NULL,__VA_ARGS__); \
	}while(0)

#else

# define _AFB_LOGGING_V2_(vlevel,llevel,...) \
	do{ \
		if(AFB_BINDING_DATA_NAME_V2.verbosity>=vlevel) \
			afb_daemon_verbose_v2(llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
	}while(0)
# define _AFB_REQ_LOGGING_V2_(vlevel,llevel,req,...) \
	do{ \
		if(AFB_BINDING_DATA_NAME_V2.verbosity>=vlevel) \
			afb_req_verbose(req,llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
	}while(0)

#endif

#include "afb-verbosity.h"
#define AFB_ERROR_V2(...)       _AFB_LOGGING_V2_(AFB_VERBOSITY_LEVEL_ERROR,_AFB_SYSLOG_LEVEL_ERROR_,__VA_ARGS__)
#define AFB_WARNING_V2(...)     _AFB_LOGGING_V2_(AFB_VERBOSITY_LEVEL_WARNING,_AFB_SYSLOG_LEVEL_WARNING_,__VA_ARGS__)
#define AFB_NOTICE_V2(...)      _AFB_LOGGING_V2_(AFB_VERBOSITY_LEVEL_NOTICE,_AFB_SYSLOG_LEVEL_NOTICE_,__VA_ARGS__)
#define AFB_INFO_V2(...)        _AFB_LOGGING_V2_(AFB_VERBOSITY_LEVEL_INFO,_AFB_SYSLOG_LEVEL_INFO_,__VA_ARGS__)
#define AFB_DEBUG_V2(...)       _AFB_LOGGING_V2_(AFB_VERBOSITY_LEVEL_DEBUG,_AFB_SYSLOG_LEVEL_DEBUG_,__VA_ARGS__)
#define AFB_REQ_ERROR_V2(...)   _AFB_REQ_LOGGING_V2_(AFB_VERBOSITY_LEVEL_ERROR,_AFB_SYSLOG_LEVEL_ERROR_,__VA_ARGS__)
#define AFB_REQ_WARNING_V2(...) _AFB_REQ_LOGGING_V2_(AFB_VERBOSITY_LEVEL_WARNING,_AFB_SYSLOG_LEVEL_WARNING_,__VA_ARGS__)
#define AFB_REQ_NOTICE_V2(...)  _AFB_REQ_LOGGING_V2_(AFB_VERBOSITY_LEVEL_NOTICE,_AFB_SYSLOG_LEVEL_NOTICE_,__VA_ARGS__)
#define AFB_REQ_INFO_V2(...)    _AFB_REQ_LOGGING_V2_(AFB_VERBOSITY_LEVEL_INFO,_AFB_SYSLOG_LEVEL_INFO_,__VA_ARGS__)
#define AFB_REQ_DEBUG_V2(...)   _AFB_REQ_LOGGING_V2_(AFB_VERBOSITY_LEVEL_DEBUG,_AFB_SYSLOG_LEVEL_DEBUG_,__VA_ARGS__)

#include "afb-daemon-v2.h"
#include "afb-service-v2.h"

