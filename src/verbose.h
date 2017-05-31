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

extern void verbose_set_name(const char *name, int authority);

/*
 Level is defined by syslog standard:
       KERN_EMERG             0        System is unusable
       KERN_ALERT             1        Action must be taken immediately
       KERN_CRIT              2        Critical conditions
       KERN_ERR               3        Error conditions
       KERN_WARNING           4        Warning conditions
       KERN_NOTICE            5        Normal but significant condition
       KERN_INFO              6        Informational
       KERN_DEBUG             7        Debug-level messages
*/
extern void verbose(int level, const char *file, int line, const char *function, const char *fmt, ...) __attribute__((format(printf, 5, 6)));
extern void vverbose(int level, const char *file, int line, const char *function, const char *fmt, va_list args);

# define ERROR(...)   do{if(verbosity>=0)verbose(3,__FILE__,__LINE__,__func__,__VA_ARGS__);}while(0)
# define WARNING(...) do{if(verbosity>=1)verbose(4,__FILE__,__LINE__,__func__,__VA_ARGS__);}while(0)
# define NOTICE(...)  do{if(verbosity>=1)verbose(5,__FILE__,__LINE__,__func__,__VA_ARGS__);}while(0)
# define INFO(...)    do{if(verbosity>=2)verbose(6,__FILE__,__LINE__,__func__,__VA_ARGS__);}while(0)
# define DEBUG(...)   do{if(verbosity>=3)verbose(7,__FILE__,__LINE__,__func__,__VA_ARGS__);}while(0)
# define LOGUSER(app) verbose_set_name(app,0)
# define LOGAUTH(app) verbose_set_name(app,1)

