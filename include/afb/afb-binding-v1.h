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

#include "afb-req-common.h"
#include "afb-event-itf.h"
#include "afb-service-common.h"
#include "afb-daemon-common.h"

#include "afb-req-v1.h"
#include "afb-session-v1.h"
#include "afb-service-v1.h"
#include "afb-daemon-v1.h"

struct afb_binding_v1;
struct afb_binding_interface_v1;

/*
 * Function for registering the binding
 *
 * A binding V1 MUST have an exported function of name
 *
 *              afbBindingV1Register
 *
 * This function is called during loading of the binding. It
 * receives an 'interface' that should be recorded for later access to
 * functions provided by the framework.
 *
 * This function MUST return the address of a structure that describes
 * the binding and its implemented verbs.
 *
 * In case of initialisation error, NULL must be returned.
 *
 * Be aware that the given 'interface' is not fully functionnal
 * because no provision is given to the name and description
 * of the binding. Check the function 'afbBindingV1ServiceInit'
 * defined in the file <afb/afb-service-v1.h> because when
 * the function 'afbBindingV1ServiceInit' is called, the 'interface'
 * is fully functionnal.
 */
extern const struct afb_binding_v1 *afbBindingV1Register (const struct afb_binding_interface_v1 *interface);

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


/*
 * Description of one verb of the API provided by the binding
 * This enumeration is valid for bindings of type version 1
 */
struct afb_verb_desc_v1
{
       const char *name;                       /* name of the verb */
       enum afb_session_flags_v1 session;      /* authorisation and session requirements of the verb */
       void (*callback)(struct afb_req req);   /* callback function implementing the verb */
       const char *info;                       /* textual description of the verb */
};

/*
 * Description of the bindings of type version 1
 */
struct afb_binding_desc_v1
{
       const char *info;                       /* textual information about the binding */
       const char *prefix;                     /* required prefix name for the binding */
       const struct afb_verb_desc_v1 *verbs;   /* array of descriptions of verbs terminated by a NULL name */
};

/*
 * Definition of the type+versions of the binding.
 * The definition uses hashes.
 */
enum  afb_binding_type_v1
{
       AFB_BINDING_VERSION_1 = 123456789
};

/*
 * Description of a binding
 */
struct afb_binding_v1
{
       enum afb_binding_type_v1 type; /* type of the binding */
       union {
               struct afb_binding_desc_v1 v1;   /* description of the binding of type 1 */
       };
};

/*
 * config mode
 */
enum afb_mode_v1
{
	AFB_MODE_LOCAL = 0,     /* run locally */
	AFB_MODE_REMOTE,        /* run remotely */
	AFB_MODE_GLOBAL         /* run either remotely or locally (DONT USE! reserved for future) */
};

/*
 * Interface between the daemon and the binding.
 */
struct afb_binding_interface_v1
{
	struct afb_daemon daemon;       /* access to the daemon facilies */
	int verbosity;                  /* level of verbosity */
	enum afb_mode_v1 mode;          /* run mode (local or remote) */
};

/*
 * Macros for logging messages
 */
#if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO)
# if !defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DETAILS)
#  define _AFB_LOGGING_V1_(itf,vlevel,llevel,...) \
	do{ \
		if(itf->verbosity>=vlevel) \
			afb_daemon_verbose2_v1(itf->daemon,llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
	}while(0)
#  define _AFB_REQ_LOGGING_V1_(itf,vlevel,llevel,req,...) \
	do{ \
		if(itf->verbosity>=vlevel) \
			afb_req_verbose(req,llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
	}while(0)
# else
#  define _AFB_LOGGING_V1_(itf,vlevel,llevel,...) \
	do{ \
		if(itf->verbosity>=vlevel) \
			afb_daemon_verbose_v1(itf->daemon,llevel,NULL,0,NULL,__VA_ARGS__); \
	}while(0)
#  define _AFB_REQ_LOGGING_V1_(itf,vlevel,llevel,req,...) \
	do{ \
		if(itf->verbosity>=vlevel) \
			afb_req_verbose(req,llevel,NULL,0,NULL,__VA_ARGS__); \
	}while(0)
# endif
# include "afb-verbosity.h"
# define AFB_ERROR_V1(itf,...)       _AFB_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_ERROR,_AFB_SYSLOG_LEVEL_ERROR_,__VA_ARGS__)
# define AFB_WARNING_V1(itf,...)     _AFB_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_WARNING,_AFB_SYSLOG_LEVEL_WARNING_,__VA_ARGS__)
# define AFB_NOTICE_V1(itf,...)      _AFB_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_NOTICE,_AFB_SYSLOG_LEVEL_NOTICE_,__VA_ARGS__)
# define AFB_INFO_V1(itf,...)        _AFB_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_INFO,_AFB_SYSLOG_LEVEL_INFO_,__VA_ARGS__)
# define AFB_DEBUG_V1(itf,...)       _AFB_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_DEBUG,_AFB_SYSLOG_LEVEL_DEBUG_,__VA_ARGS__)
# define AFB_REQ_ERROR_V1(itf,...)   _AFB_REQ_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_ERROR,_AFB_SYSLOG_LEVEL_ERROR_,__VA_ARGS__)
# define AFB_REQ_WARNING_V1(itf,...) _AFB_REQ_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_WARNING,_AFB_SYSLOG_LEVEL_WARNING_,__VA_ARGS__)
# define AFB_REQ_NOTICE_V1(itf,...)  _AFB_REQ_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_NOTICE,_AFB_SYSLOG_LEVEL_NOTICE_,__VA_ARGS__)
# define AFB_REQ_INFO_V1(itf,...)    _AFB_REQ_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_INFO,_AFB_SYSLOG_LEVEL_INFO_,__VA_ARGS__)
# define AFB_REQ_DEBUG_V1(itf,...)   _AFB_REQ_LOGGING_V1_(itf,AFB_VERBOSITY_LEVEL_DEBUG,_AFB_SYSLOG_LEVEL_DEBUG_,__VA_ARGS__)
#endif

