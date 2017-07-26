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
#include <json-c/json.h>

extern int wrap_json_pack_error_position(int rc);
extern int wrap_json_pack_error_code(int rc);
extern const char *wrap_json_pack_error_string(int rc);
extern int wrap_json_vpack(struct json_object **result, const char *desc, va_list args);
extern int wrap_json_pack(struct json_object **result, const char *desc, ...);

