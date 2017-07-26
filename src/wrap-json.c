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

#include <string.h>

#include "wrap-json.h"

#define STACKCOUNT 32
#define STRCOUNT   8

enum {
	wrap_json_pack_error_none,
	wrap_json_pack_error_null_object,
	wrap_json_pack_error_truncated,
	wrap_json_pack_error_internal_error,
	wrap_json_pack_error_out_of_memory,
	wrap_json_pack_error_invalid_character,
	wrap_json_pack_error_too_long,
	wrap_json_pack_error_too_deep,
	wrap_json_pack_error_null_spec,
	wrap_json_pack_error_null_key,
	wrap_json_pack_error_null_string,
	_wrap_json_pack_error_count_
};

static const char ignore_all[] = " \t\n\r,:";
static const char accept_arr[] = "][{snbiIfoO";
static const char accept_key[] = "s}";
#define accept_any (&accept_arr[1])

static const char *pack_errors[_wrap_json_pack_error_count_] =
{
	[wrap_json_pack_error_none] = "unknown error",
	[wrap_json_pack_error_null_object] = "null object",
	[wrap_json_pack_error_truncated] = "truncated",
	[wrap_json_pack_error_internal_error] = "internal error",
	[wrap_json_pack_error_out_of_memory] = "out of memory",
	[wrap_json_pack_error_invalid_character] = "invalid character",
	[wrap_json_pack_error_too_long] = "too long",
	[wrap_json_pack_error_too_deep] = "too deep",
	[wrap_json_pack_error_null_spec] = "spec is NULL",
	[wrap_json_pack_error_null_key] = "key is NULL",
	[wrap_json_pack_error_null_string] = "string is NULL"
};

int wrap_json_pack_error_position(int rc)
{
	if (rc < 0)
		rc = -rc;
	return (rc >> 4) + 1;
}

int wrap_json_pack_error_code(int rc)
{
	if (rc < 0)
		rc = -rc;
	return rc & 15;
}

const char *wrap_json_pack_error_string(int rc)
{
	rc = wrap_json_pack_error_code(rc);
	if (rc >= sizeof pack_errors / sizeof *pack_errors)
		rc = 0;
	return pack_errors[rc];
}



static inline const char *skip(const char *d)
{
	while (*d && strchr(ignore_all, *d))
		d++;
	return d;
}

int wrap_json_vpack(struct json_object **result, const char *desc, va_list args)
{
	int nstr, notnull, nullable, rc;
	size_t sz, dsz, ssz;
	char *s;
	char c;
	const char *d;
	char buffer[256];
	struct { const char *str; size_t sz; } strs[STRCOUNT];
	struct { struct json_object *cont, *key; const char *acc; char type; } stack[STACKCOUNT], *top;
	struct json_object *obj;

	ssz = sizeof buffer;
	s = buffer;
	top = stack;
	top->key = NULL;
	top->cont = NULL;
	top->acc = accept_any;
	top->type = 0;
	if (!desc)
		goto null_spec;
	d = skip(desc);
	for(;;) {
		c = *d;
		if (!c)
			goto truncated;
		if (!strchr(top->acc, c))
			goto invalid_character;
		d = skip(++d);
		switch(c) {
		case 's':
			nullable = 0;
			notnull = 0;
			nstr = 0;
			sz = 0;
			for (;;) {
				strs[nstr].str = va_arg(args, const char*);
				if (strs[nstr].str)
					notnull = 1;
				if (*d == '?') {
					d = skip(++d);
					nullable = 1;
				}
				switch(*d) {
				case '%': strs[nstr].sz = va_arg(args, size_t); d = skip(++d); break;
				case '#': strs[nstr].sz = (size_t)va_arg(args, int); d = skip(++d); break;
				default: strs[nstr].sz = strs[nstr].str ? strlen(strs[nstr].str) : 0; break;
				}
				sz += strs[nstr++].sz;
				if (*d == '?') {
					d = skip(++d);
					nullable = 1;
				}
				if (*d != '+')
					break;
				if (nstr >= STRCOUNT)
					goto too_long;
				d = skip(++d);
			}
			if (*d == '*')
				nullable = 1;
			if (notnull) {
				if (sz > ssz) {
					ssz += ssz;
					if (ssz < sz)
						ssz = sz;
					s = alloca(sz);
				}
				dsz = sz;
				while (nstr) {
					nstr--;
					dsz -= strs[nstr].sz;
					memcpy(&s[dsz], strs[nstr].str, strs[nstr].sz);
				}
				obj = json_object_new_string_len(s, sz);
				if (!obj)
					goto out_of_memory;
			} else if (nullable)
				obj = NULL;
			else
				goto null_string;
			break;
		case 'n':
			obj = NULL;
			break;
		case 'b':
			obj = json_object_new_boolean(va_arg(args, int));
			if (!obj)
				goto out_of_memory;
			break;
		case 'i':
			obj = json_object_new_int(va_arg(args, int));
			if (!obj)
				goto out_of_memory;
			break;
		case 'I':
			obj = json_object_new_int64(va_arg(args, int64_t));
			if (!obj)
				goto out_of_memory;
			break;
		case 'f':
			obj = json_object_new_double(va_arg(args, double));
			if (!obj)
				goto out_of_memory;
			break;
		case 'o':
		case 'O':
			obj = va_arg(args, struct json_object*);
			if (*d == '?')
				d = skip(++d);
			else if (*d != '*' && !obj)
				goto null_object;
			if (c == 'O')
				json_object_get(obj);
			break;
		case '[':
		case '{':
			if (++top >= &stack[STACKCOUNT])
				goto too_deep;
			top->key = NULL;
			if (c == '[') {
				top->type = ']';
				top->acc = accept_arr;
				top->cont = json_object_new_array();
			} else {
				top->type = '}';
				top->acc = accept_key;
				top->cont = json_object_new_object();
			}
			if (!top->cont)
				goto out_of_memory;
			continue;
		case '}':
		case ']':
			if (c != top->type || top <= stack)
				goto invalid_character;
			obj = (top--)->cont;
			if (*d == '*' && !(c == '}' ? json_object_object_length(obj) : json_object_array_length(obj))) {
				json_object_put(obj);
				obj = NULL;
			}
			break;
		default:
			goto internal_error;
		}
		switch (top->type) {
		case 0:
			if (top != stack)
				goto internal_error;
			if (*d)
				goto invalid_character;
			*result = obj;
			return 0;
		case ']':
			if (obj || *d != '*')
				json_object_array_add(top->cont, obj);
			if (*d == '*')
				d = skip(++d);
			break;
		case '}':
			if (!obj)
				goto null_key;
			top->key = obj;
			top->acc = accept_any;
			top->type = ':';
			break;
		case ':':
			if (obj || *d != '*')
				json_object_object_add(top->cont, json_object_get_string(top->key), obj);
			if (*d == '*')
				d = skip(++d);
			json_object_put(top->key);
			top->key = NULL;
			top->acc = accept_key;
			top->type = '}';
			break;
		}
	}

null_object:
	rc = wrap_json_pack_error_null_object;
	goto error;
truncated:
	rc = wrap_json_pack_error_truncated;
	goto error;
internal_error:
	rc = wrap_json_pack_error_internal_error;
	goto error;
out_of_memory:
	rc = wrap_json_pack_error_out_of_memory;
	goto error;
invalid_character:
	rc = wrap_json_pack_error_invalid_character;
	goto error;
too_long:
	rc = wrap_json_pack_error_too_long;
	goto error;
too_deep:
	rc = wrap_json_pack_error_too_deep;
	goto error;
null_spec:
	rc = wrap_json_pack_error_null_spec;
	goto error;
null_key:
	rc = wrap_json_pack_error_null_key;
	goto error;
null_string:
	rc = wrap_json_pack_error_null_string;
	goto error;
error:
	do {
		json_object_put(top->key);
		json_object_put(top->cont);
	} while (--top >= stack);
	*result = NULL;
	rc = rc | (int)((d - desc) << 4);
	return -rc;
}

int wrap_json_pack(struct json_object **result, const char *desc, ...)
{
	int rc;
	va_list args;

	va_start(args, desc);
	rc = wrap_json_vpack(result, desc, args);
	va_end(args);
	return rc;
}

#if 1
#include <stdio.h>

void T(const char *desc, ...)
{
	int rc;
	va_list args;
	struct json_object *result;

	va_start(args, desc);
	rc = wrap_json_vpack(&result, desc, args);
	va_end(args);
	if (!rc) 
		printf("  SUCCESS %s\n\n", json_object_to_json_string(result));
	else
		printf("  ERROR[char %d err %d] %s\n\n", wrap_json_pack_error_position(rc), wrap_json_pack_error_code(rc), wrap_json_pack_error_string(rc));
	json_object_put(result);
}

#define t(...) printf("testing(%s)\n",#__VA_ARGS__); T(__VA_ARGS__);

int main()
{
	char buffer[4] = {'t', 'e', 's', 't'};

	t("n");
	t("b", 1);
	t("b", 0);
	t("i", 1);
	t("I", (uint64_t)0x123456789abcdef);
	t("f", 3.14);
	t("s", "test");
	t("s?", "test");
	t("s?", NULL);
	t("s#", "test asdf", 4);
	t("s%", "test asdf", (size_t)4);
	t("s#", buffer, 4);
	t("s%", buffer, (size_t)4);
	t("s++", "te", "st", "ing");
	t("s#+#+", "test", 1, "test", 2, "test");
	t("s%+%+", "test", (size_t)1, "test", (size_t)2, "test");
	t("{}", 1.0);
	t("[]", 1.0);
	t("o", json_object_new_int(1));
	t("o?", json_object_new_int(1));
	t("o?", NULL);
	t("O", json_object_new_int(1));
	t("O?", json_object_new_int(1));
	t("O?", NULL);
	t("{s:[]}", "foo");
	t("{s+#+: []}", "foo", "barbar", 3, "baz");
	t("{s:s,s:o,s:O}", "a", NULL, "b", NULL, "c", NULL);
	t("{s:**}", "a", NULL);
	t("{s:s*,s:o*,s:O*}", "a", NULL, "b", NULL, "c", NULL);
	t("[i,i,i]", 0, 1, 2);
	t("[s,o,O]", NULL, NULL, NULL);
	t("[**]", NULL);
	t("[s*,o*,O*]", NULL, NULL, NULL);
	t(" s ", "test");
	t("[ ]");
	t("[ i , i,  i ] ", 1, 2, 3);
	t("{\n\n1");
	t("[}");
	t("{]");
	t("[");
	t("{");
	t("[i]a", 42);
	t("ia", 42);
	t("s", NULL);
	t("+", NULL);
	t(NULL);
	t("{s:i}", NULL, 1);
	t("{ {}: s }", "foo");
	t("{ s: {},  s:[ii{} }", "foo", "bar", 12, 13);
	t("[[[[[   [[[[[  [[[[ }]]]] ]]]] ]]]]]");
	return 0;
}

#endif


