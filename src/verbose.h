/*
 Copyright (C) 2016, 2017 "IoT.bzh"

 author: José Bollo <jose.bollo@iot.bzh>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#pragma once

#include <stdarg.h>

/*
  verbosity tune the count of reported messages

   verbosity value : reported messages
   ----------------+------------------------
    lesser than 0  : no message at all
         0         : ERROR
         1         : ERROR, WARNING, NOTICE
         2         : ERROR, WARNING, NOTICE, INFO
    greater than 2 : ERROR, WARNING, NOTICE, INFO, DEBUG

*/
extern int verbosity;

enum verbosity_levels
{
	Verbosity_Level_Error = 0,
	Verbosity_Level_Warning = 1,
	Verbosity_Level_Notice = 1,
	Verbosity_Level_Info = 2,
	Verbosity_Level_Debug = 3
};

extern void verbose_set_name(const char *name, int authority);

/*
 Log level is defined by syslog standard:
       KERN_EMERG             0        System is unusable
       KERN_ALERT             1        Action must be taken immediately
       KERN_CRIT              2        Critical conditions
       KERN_ERR               3        Error conditions
       KERN_WARNING           4        Warning conditions
       KERN_NOTICE            5        Normal but significant condition
       KERN_INFO              6        Informational
       KERN_DEBUG             7        Debug-level messages
*/

enum log_levels
{
	Log_Level_Emergency = 0,
	Log_Level_Alert = 1,
	Log_Level_Critical = 2,
	Log_Level_Error = 3,
	Log_Level_Warning = 4,
	Log_Level_Notice = 5,
	Log_Level_Info = 6,
	Log_Level_Debug = 7
};

extern void verbose(int loglevel, const char *file, int line, const char *function, const char *fmt, ...) __attribute__((format(printf, 5, 6)));
extern void vverbose(int loglevel, const char *file, int line, const char *function, const char *fmt, va_list args);

# define _VERBOSE_(vlvl,llvl,...)  do{ if (verbosity >= vlvl) verbose(llvl, __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)
# define ERROR(...)                _VERBOSE_(Verbosity_Level_Error, Log_Level_Error, __VA_ARGS__)
# define WARNING(...)              _VERBOSE_(Verbosity_Level_Warning, Log_Level_Warning, __VA_ARGS__)
# define NOTICE(...)               _VERBOSE_(Verbosity_Level_Notice, Log_Level_Notice, __VA_ARGS__)
# define INFO(...)                 _VERBOSE_(Verbosity_Level_Info, Log_Level_Info, __VA_ARGS__)
# define DEBUG(...)                _VERBOSE_(Verbosity_Level_Debug, Log_Level_Debug, __VA_ARGS__)
# define LOGUSER(app)              verbose_set_name(app,0)
# define LOGAUTH(app)              verbose_set_name(app,1)

extern void (*verbose_observer)(int loglevel, const char *file, int line, const char *function, const char *fmt, va_list args);
