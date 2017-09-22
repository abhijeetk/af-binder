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

#include "afb-auth.h"
#include "afb-session-v2.h"
#include "afb-verbosity.h"

#include "afb-eventid.h"
#include "afb-request.h"
#include "afb-dynapi.h"

/*
 * The function afbBindingVdyn if exported allows to create
 * pure dynamic bindings. When the binding is loaded, it receives
 * a virtual dynapi that can be used to create apis. The
 * given API can not be used except for creating dynamic apis.
 */
extern int afbBindingVdyn(struct afb_dynapi *dynapi);

/*
 * Macros for logging messages
 */
#if defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DATA)

# define _AFB_DYNAPI_LOGGING_(vlevel,llevel,dynapi,...) \
	do{ \
		if(dynapi->verbosity>=vlevel) {\
			if (llevel <= AFB_VERBOSITY_LEVEL_ERROR) \
				afb_dynapi_verbose(dynapi,llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
			else \
				afb_dynapi_verbose(dynapi,llevel,__FILE__,__LINE__,NULL,NULL); \
		} \
	}while(0)
# define _AFB_REQUEST_LOGGING_(vlevel,llevel,request,...) \
	do{ \
		if(request->dynapi->verbosity>=vlevel) \
			afb_request_verbose(request,llevel,__FILE__,__LINE__,NULL,NULL); \
	}while(0)

#elif defined(AFB_BINDING_PRAGMA_NO_VERBOSE_DETAILS)

# define _AFB_DYNAPI_LOGGING_(vlevel,llevel,dynapi,...) \
	do{ \
		if(dynapi->verbosity>=vlevel) \
			afb_dynapi_verbose(dynapi,llevel,NULL,0,NULL,__VA_ARGS__); \
	}while(0)
# define _AFB_REQUEST_LOGGING_(vlevel,llevel,request,...) \
	do{ \
		if(request->dynapi->verbosity>=vlevel) \
			afb_request_verbose(request,llevel,NULL,0,NULL,__VA_ARGS__); \
	}while(0)

#else

# define _AFB_DYNAPI_LOGGING_(vlevel,llevel,dynapi,...) \
	do{ \
		if(dynapi->verbosity>=vlevel) \
			afb_dynapi_verbose(dynapi,llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
	}while(0)
# define _AFB_REQUEST_LOGGING_(vlevel,llevel,request,...) \
	do{ \
		if(request->dynapi->verbosity>=vlevel) \
			afb_request_verbose(request,llevel,__FILE__,__LINE__,__func__,__VA_ARGS__); \
	}while(0)

#endif

#define AFB_DYNAPI_ERROR(...)     _AFB_DYNAPI_LOGGING_(AFB_VERBOSITY_LEVEL_ERROR,_AFB_SYSLOG_LEVEL_ERROR_,__VA_ARGS__)
#define AFB_DYNAPI_WARNING(...)   _AFB_DYNAPI_LOGGING_(AFB_VERBOSITY_LEVEL_WARNING,_AFB_SYSLOG_LEVEL_WARNING_,__VA_ARGS__)
#define AFB_DYNAPI_NOTICE(...)    _AFB_DYNAPI_LOGGING_(AFB_VERBOSITY_LEVEL_NOTICE,_AFB_SYSLOG_LEVEL_NOTICE_,__VA_ARGS__)
#define AFB_DYNAPI_INFO(...)      _AFB_DYNAPI_LOGGING_(AFB_VERBOSITY_LEVEL_INFO,_AFB_SYSLOG_LEVEL_INFO_,__VA_ARGS__)
#define AFB_DYNAPI_DEBUG(...)     _AFB_DYNAPI_LOGGING_(AFB_VERBOSITY_LEVEL_DEBUG,_AFB_SYSLOG_LEVEL_DEBUG_,__VA_ARGS__)
#define AFB_REQUEST_ERROR(...)    _AFB_REQUEST_LOGGING_(AFB_VERBOSITY_LEVEL_ERROR,_AFB_SYSLOG_LEVEL_ERROR_,__VA_ARGS__)
#define AFB_REQUEST_WARNING(...)  _AFB_REQUEST_LOGGING_(AFB_VERBOSITY_LEVEL_WARNING,_AFB_SYSLOG_LEVEL_WARNING_,__VA_ARGS__)
#define AFB_REQUEST_NOTICE(...)   _AFB_REQUEST_LOGGING_(AFB_VERBOSITY_LEVEL_NOTICE,_AFB_SYSLOG_LEVEL_NOTICE_,__VA_ARGS__)
#define AFB_REQUEST_INFO(...)     _AFB_REQUEST_LOGGING_(AFB_VERBOSITY_LEVEL_INFO,_AFB_SYSLOG_LEVEL_INFO_,__VA_ARGS__)
#define AFB_REQUEST_DEBUG(...)    _AFB_REQUEST_LOGGING_(AFB_VERBOSITY_LEVEL_DEBUG,_AFB_SYSLOG_LEVEL_DEBUG_,__VA_ARGS__)


