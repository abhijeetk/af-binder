/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
 * Author José Bollo <jose.bollo@iot.bzh>
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

#define _GNU_SOURCE
#define AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO

#include <string.h>

#include <json-c/json.h>
#include <afb/afb-binding.h>

#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-api-so-v2.h"
#include "afb-ditf.h"
#include "afb-xreq.h"
#include "verbose.h"

#include "monitor-api.inc"

extern struct afb_apiset *main_apiset;

int afb_monitor_init()
{
	static int v;
	return afb_api_so_v2_add_binding(&_afb_binding_v2_monitor, NULL, main_apiset, &v);
}

/******************************************************************************
**** Monitoring verbosity
******************************************************************************/

static const char _debug_[] = "debug";
static const char _info_[] = "info";
static const char _notice_[] = "notice";
static const char _warning_[] = "warning";
static const char _error_[] = "error";

/**
 * Translate verbosity indication to an integer value.
 * @param v the verbosity indication
 * @return the verbosity level (0, 1, 2 or 3) or -1 in case of error
 */
static int decode_verbosity(struct json_object *v)
{
	const char *s;
	int level = -1;
	if (json_object_is_type(v, json_type_int)) {
		level = json_object_get_int(v);
		level = level < 0 ? 0 : level > 3 ? 3 : level;
	} else if (json_object_is_type(v, json_type_string)) {
		s = json_object_get_string(v);
		switch(*s&~' ') {
		case 'D':
			if (!strcasecmp(s, _debug_))
				level = 3;
			break;
		case 'I':
			if (!strcasecmp(s, _info_))
				level = 2;
			break;
		case 'N':
			if (!strcasecmp(s, _notice_))
				level = 1;
			break;
		case 'W':
			if (!strcasecmp(s, _warning_))
				level = 1;
			break;
		case 'E':
			if (!strcasecmp(s, _error_))
				level = 0;
			break;
		}
	}
	return level;
}

/**
 * callback for setting verbosity on all apis
 * @param set the apiset
 * @param the name of the api to set
 * @param closure the verbosity to set as an integer casted to a pointer
 */
static void set_verbosity_to_all_cb(struct afb_apiset *set, const char *name, void *closure)
{
	afb_apiset_set_verbosity(set, name, (int)(intptr_t)closure);
}

/**
 * set the verbosity 'level' of the api of 'name'
 * @param name the api name or "*" for any api or NULL or "" for global verbosity
 * @param level the verbosity level to set
 */
static void set_verbosity_to(const char *name, int level)
{
	if (!name || !name[0])
		verbosity = level;
	else if (name[0] == '*' && !name[1])
		afb_apiset_enum(main_apiset, set_verbosity_to_all_cb, (void*)(intptr_t)level);
	else
		afb_apiset_set_verbosity(main_apiset, name, level);
}

/**
 * Set verbosities accordling to specification in 'spec'
 * @param spec specification of the verbosity to set
 */
static void set_verbosity(struct json_object *spec)
{
	int l;
	struct json_object_iterator it, end;

	if (json_object_is_type(spec, json_type_object)) {
		it = json_object_iter_begin(spec);
		end = json_object_iter_end(spec);
		while (!json_object_iter_equal(&it, &end)) {
			l = decode_verbosity(json_object_iter_peek_value(&it));
			if (l >= 0)
				set_verbosity_to(json_object_iter_peek_name(&it), l);
			json_object_iter_next(&it);
		}
	} else {
		l = decode_verbosity(spec);
		if (l >= 0) {
			set_verbosity_to("", l);
			set_verbosity_to("*", l);
		}
	}
}

/**
 * Translate verbosity level to a protocol indication.
 * @param level the verbosity 
 * @return the encoded verbosity
 */
static struct json_object *encode_verbosity(int level)
{
	switch(level) {
	case 0:	return json_object_new_string(_error_);
	case 1:	return json_object_new_string(_notice_);
	case 2:	return json_object_new_string(_info_);
	case 3:	return json_object_new_string(_debug_);
	default: return json_object_new_int(level);
	}
}

/**
 * callback for getting verbosity of all apis
 * @param set the apiset
 * @param the name of the api to set
 * @param closure the json object to build
 */
static void get_verbosity_of_all_cb(struct afb_apiset *set, const char *name, void *closure)
{
	struct json_object *resu = closure;
	int l = afb_apiset_get_verbosity(set, name);
	if (l >= 0)
		json_object_object_add(resu, name, encode_verbosity(l));
}

/**
 * get in resu the verbosity of the api of 'name'
 * @param resu the json object to build
 * @param name the api name or "*" for any api or NULL or "" for global verbosity
 */
static void get_verbosity_of(struct json_object *resu, const char *name)
{
	int l;
	if (!name || !name[0])
		json_object_object_add(resu, "", encode_verbosity(verbosity));
	else if (name[0] == '*' && !name[1])
		afb_apiset_enum(main_apiset, get_verbosity_of_all_cb, resu);
	else {
		l = afb_apiset_get_verbosity(main_apiset, name);
		if (l >= 0)
			json_object_object_add(resu, name, encode_verbosity(l));
	}
}

/**
 * get verbosities accordling to specification in 'spec'
 * @param resu the json object to build
 * @param spec specification of the verbosity to set
 */
static void get_verbosity(struct json_object *resu, struct json_object *spec)
{
	int i, n;
	struct json_object_iterator it, end;

	if (json_object_is_type(spec, json_type_object)) {
		it = json_object_iter_begin(spec);
		end = json_object_iter_end(spec);
		while (!json_object_iter_equal(&it, &end)) {
			get_verbosity_of(resu, json_object_iter_peek_name(&it));
			json_object_iter_next(&it);
		}
	} else if (json_object_is_type(spec, json_type_array)) {
		n = json_object_array_length(spec);
		for (i = 0 ; i < n ; i++)
			get_verbosity_of(resu, json_object_get_string(json_object_array_get_idx(spec, i)));
	} else if (json_object_get_boolean(spec)) {
		get_verbosity_of(resu, "");
		get_verbosity_of(resu, "*");
	}
}

/******************************************************************************
**** Monitoring apis
******************************************************************************/

/**
 * get apis accordling to specification in 'spec'
 * @param resu the json object to build
 * @param spec specification of the verbosity to set
 */
static void get_one_api(struct json_object *resu, const char *name, struct json_object *spec)
{
	struct json_object *o;
	struct afb_api api;
	int rc;

	rc = afb_apiset_lookup(main_apiset, name, &api);
	if (!rc) {
		o = api.itf->describe ? api.itf->describe(api.closure) : NULL;
		json_object_object_add(resu, name, o);
	}
}

/**
 * callback for getting verbosity of all apis
 * @param set the apiset
 * @param the name of the api to set
 * @param closure the json object to build
 */
static void get_apis_of_all_cb(struct afb_apiset *set, const char *name, void *closure)
{
	struct json_object *resu = closure;
	get_one_api(resu, name, NULL);
}

/**
 * get apis accordling to specification in 'spec'
 * @param resu the json object to build
 * @param spec specification of the verbosity to set
 */
static void get_apis(struct json_object *resu, struct json_object *spec)
{
	int i, n;
	struct json_object_iterator it, end;

	if (json_object_is_type(spec, json_type_object)) {
		it = json_object_iter_begin(spec);
		end = json_object_iter_end(spec);
		while (!json_object_iter_equal(&it, &end)) {
			get_one_api(resu, json_object_iter_peek_name(&it), json_object_iter_peek_value(&it));
			json_object_iter_next(&it);
		}
	} else if (json_object_is_type(spec, json_type_array)) {
		n = json_object_array_length(spec);
		for (i = 0 ; i < n ; i++)
			get_one_api(resu, json_object_get_string(json_object_array_get_idx(spec, i)), NULL);
	} else if (json_object_get_boolean(spec)) {
		afb_apiset_enum(main_apiset, get_apis_of_all_cb, resu);
	}
}

/******************************************************************************
**** Implementation monitoring verbs
******************************************************************************/

static const char _verbosity_[] = "verbosity";
static const char _apis_[] = "apis";

static void f_get(struct afb_req req)
{
	struct json_object *o, *v, *r, *x;

	r = json_object_new_object();
	o = afb_req_json(req);

	if (json_object_object_get_ex(o, _verbosity_, &v)) {
		x = json_object_new_object();
		json_object_object_add(r, _verbosity_, x);
		get_verbosity(x, v);
	}

	if (json_object_object_get_ex(o, _apis_, &v)) {
		x = json_object_new_object();
		json_object_object_add(r, _apis_, x);
		get_apis(x, v);
	}

	afb_req_success(req, json_object_get(r), NULL);
	json_object_put(r);
}

static void f_set(struct afb_req req)
{
	struct json_object *o, *v;

	o = afb_req_json(req);
	if (json_object_object_get_ex(o, _verbosity_, &v)) {
		set_verbosity(v);
	}

	afb_req_success(req, NULL, NULL);
}

#if 0
static void f_hook(struct afb_xreq *xreq)
{
	struct json_object *o, *v;

	o = afb_xreq_json(xreq);
	if (json_object_object_get_ex(o, _verbosity_, &v)) {
		set_verbosity(v);
	}

	if (!xreq->replied)
		afb_xreq_success(xreq, NULL, NULL);
}
#endif

