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

#define AFB_VERBOSITY_LEVEL_ERROR	0
#define AFB_VERBOSITY_LEVEL_WARNING	1
#define AFB_VERBOSITY_LEVEL_NOTICE	2
#define AFB_VERBOSITY_LEVEL_INFO	3
#define AFB_VERBOSITY_LEVEL_DEBUG	4

#define AFB_SYSLOG_LEVEL_EMERGENCY	0
#define AFB_SYSLOG_LEVEL_ALERT		1
#define AFB_SYSLOG_LEVEL_CRITICAL	2
#define AFB_SYSLOG_LEVEL_ERROR		3
#define AFB_SYSLOG_LEVEL_WARNING	4
#define AFB_SYSLOG_LEVEL_NOTICE		5
#define AFB_SYSLOG_LEVEL_INFO		6
#define AFB_SYSLOG_LEVEL_DEBUG		7

#define AFB_VERBOSITY_LEVEL_WANT(verbosity,level)	((verbosity) >= (level))

#define AFB_VERBOSITY_LEVEL_WANT_ERROR(x)	AFB_VERBOSITY_LEVEL_WANT(x,AFB_VERBOSITY_LEVEL_ERROR)
#define AFB_VERBOSITY_LEVEL_WANT_WARNING(x)	AFB_VERBOSITY_LEVEL_WANT(x,AFB_VERBOSITY_LEVEL_WARNING)
#define AFB_VERBOSITY_LEVEL_WANT_NOTICE(x)	AFB_VERBOSITY_LEVEL_WANT(x,AFB_VERBOSITY_LEVEL_NOTICE)
#define AFB_VERBOSITY_LEVEL_WANT_INFO(x)	AFB_VERBOSITY_LEVEL_WANT(x,AFB_VERBOSITY_LEVEL_INFO)
#define AFB_VERBOSITY_LEVEL_WANT_DEBUG(x)	AFB_VERBOSITY_LEVEL_WANT(x,AFB_VERBOSITY_LEVEL_DEBUG)

#define AFB_SYSLOG_MASK_WANT(verbomask,level)	((verbomask) & (1 << (level)))

#define AFB_SYSLOG_MASK_WANT_EMERGENCY(x)	AFB_SYSLOG_MASK_WANT(x,AFB_SYSLOG_LEVEL_EMERGENCY)
#define AFB_SYSLOG_MASK_WANT_ALERT(x)		AFB_SYSLOG_MASK_WANT(x,AFB_SYSLOG_LEVEL_ALERT)
#define AFB_SYSLOG_MASK_WANT_CRITICAL(x)	AFB_SYSLOG_MASK_WANT(x,AFB_SYSLOG_LEVEL_CRITICAL)
#define AFB_SYSLOG_MASK_WANT_ERROR(x)		AFB_SYSLOG_MASK_WANT(x,AFB_SYSLOG_LEVEL_ERROR)
#define AFB_SYSLOG_MASK_WANT_WARNING(x)		AFB_SYSLOG_MASK_WANT(x,AFB_SYSLOG_LEVEL_WARNING)
#define AFB_SYSLOG_MASK_WANT_NOTICE(x)		AFB_SYSLOG_MASK_WANT(x,AFB_SYSLOG_LEVEL_NOTICE)
#define AFB_SYSLOG_MASK_WANT_INFO(x)		AFB_SYSLOG_MASK_WANT(x,AFB_SYSLOG_LEVEL_INFO)
#define AFB_SYSLOG_MASK_WANT_DEBUG(x)		AFB_SYSLOG_MASK_WANT(x,AFB_SYSLOG_LEVEL_DEBUG)

#define AFB_SYSLOG_LEVEL_FROM_VERBOSITY(x)	((x) + (AFB_SYSLOG_LEVEL_ERROR - AFB_VERBOSITY_LEVEL_ERROR))
#define AFB_SYSLOG_LEVEL_TO_VERBOSITY(x)	((x) + (AFB_VERBOSITY_LEVEL_ERROR - AFB_SYSLOG_LEVEL_ERROR))

static inline int _afb_verbomask_to_upper_level_(int verbomask)
{
	int result = 0;
	while ((verbomask >>= 1) && result < AFB_SYSLOG_LEVEL_DEBUG)
		result++;
	return result;
}

