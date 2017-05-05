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

/*
 * Enum for Session/Token/Assurance middleware.
 */
enum afb_auth_type
{
	afb_auth_No = 0,	/** never authorized, no data */
	afb_auth_Token,		/** authorized if token valid, no data */
	afb_auth_LOA,		/** authorized if LOA greater than data 'loa' */
	afb_auth_Permission,	/** authorized if permission 'text' is granted */
	afb_auth_Or,		/** authorized if 'first' or 'next' is authorized */
	afb_auth_And,		/** authorized if 'first' and 'next' are authorized */
	afb_auth_Not,		/** authorized if 'first' is not authorized */
	afb_auth_Yes		/** always authorized, no data */
};

struct afb_auth
{
	const enum afb_auth_type type;
	union {
		const char *text;
		const unsigned loa;
		const struct afb_auth *first;
	};
	const struct afb_auth *next;
};

