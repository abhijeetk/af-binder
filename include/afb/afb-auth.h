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
	afb_auth_No = 0,
	afb_auth_Permission,
	afb_auth_Or,
	afb_auth_And,
	afb_auth_Yes
};

struct afb_auth_desc
{
	enum afb_auth_type type;
	union {
		const char *text;
		struct afb_auth_desc *child[2];
	};
};

