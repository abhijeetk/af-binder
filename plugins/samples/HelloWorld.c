/*
 * Copyright (C) 2015 "IoT.bzh"
 * Author "Fulup Ar Foll"
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <json.h>

#include "afb-plugin.h"
#include "afb-req-itf.h"

static int fillargs(json_object *args, struct afb_arg arg)
{
    json_object *obj;

    obj = json_object_new_object();
    json_object_object_add (obj, "value", json_object_new_string(arg.value));
    if (arg.path != NULL)
	json_object_object_add (obj, "path", json_object_new_string(arg.path));
    json_object_object_add (obj, "size", json_object_new_int64((int64_t)arg.size));
    json_object_object_add (args, arg.name && *arg.name ? arg.name : "<empty-string>", obj);
    return 1; /* continue to iterate */
}

// Sample Generic Ping Debug API
static void ping(struct afb_req request, json_object *jresp)
{
    static int pingcount = 0;
    json_object *query;

    query = json_object_new_object();
    afb_req_iterate(request, (void*)fillargs, query);

    afb_req_success_f(request, jresp, "Ping Binder Daemon count=%d query=%s", ++pingcount, json_object_to_json_string(query));
}

static void pingSample (struct afb_req request)
{
	ping(request, json_object_new_string ("Some String"));
}

static void pingFail (struct afb_req request)
{
	afb_req_fail(request, "failed", "Ping Binder Daemon fails");
}

static void pingNull (struct afb_req request)
{
	ping(request, NULL);
}

static void pingBug (struct afb_req request)
{
	pingNull((struct afb_req){NULL,NULL,NULL});
}


// For samples https://linuxprograms.wordpress.com/2010/05/20/json-c-libjson-tutorial/
static void pingJson (struct afb_req request) {
    json_object *jresp, *embed;    
    
    jresp = json_object_new_object();
    json_object_object_add(jresp, "myString", json_object_new_string ("Some String"));
    json_object_object_add(jresp, "myInt", json_object_new_int (1234));
     
    embed  = json_object_new_object();
    json_object_object_add(embed, "subObjString", json_object_new_string ("Some String"));
    json_object_object_add(embed, "subObjInt", json_object_new_int (5678));
    
    json_object_object_add(jresp,"eobj", embed);

    ping(request, jresp);
}

// NOTE: this sample does not use session to keep test a basic as possible
//       in real application most APIs should be protected with AFB_SESSION_CHECK
static const struct AFB_restapi pluginApis[]= {
  {"ping"     , AFB_SESSION_NONE, pingSample  , "Ping Application Framework"},
  {"pingfail" , AFB_SESSION_NONE, pingFail    , "Fails"},
  {"pingnull" , AFB_SESSION_NONE, pingNull    , "Return NULL"},
  {"pingbug"  , AFB_SESSION_NONE, pingBug     , "Do a Memory Violation"},
  {"pingJson" , AFB_SESSION_NONE, pingJson    , "Return a JSON object"},
  {NULL}
};

static const struct AFB_plugin plugin_desc = {
	.type = AFB_PLUGIN_JSON,
	.info = "Minimal Hello World Sample",
	.prefix = "hello",
	.apis = pluginApis
};

const struct AFB_plugin *pluginRegister (const struct AFB_interface *itf)
{
	return &plugin_desc;
}
