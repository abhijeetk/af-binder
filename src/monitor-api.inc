
static const char _afb_description_v2_monitor[] =
    "{\"openapi\":\"3.0.0\",\"info\":{\"description\":\"monitoring of binding"
    "s and internals\",\"title\":\"monitor\",\"version\":\"1.0\",\"x-binding-"
    "c-generator\":{\"api\":\"monitor\",\"version\":2,\"prefix\":\"f_\",\"pos"
    "tfix\":\"\",\"preinit\":null,\"init\":null,\"onevent\":null,\"scope\":\""
    "static\",\"private\":true}},\"servers\":[{\"url\":\"ws://{host}:{port}/a"
    "pi/monitor\",\"description\":\"The API server.\",\"variables\":{\"host\""
    ":{\"default\":\"localhost\"},\"port\":{\"default\":\"1234\"}},\"x-afb-ev"
    "ents\":[{\"$ref\":\"#/components/schemas/afb-event\"}]}],\"components\":"
    "{\"schemas\":{\"afb-reply\":{\"$ref\":\"#/components/schemas/afb-reply-v"
    "1\"},\"afb-event\":{\"$ref\":\"#/components/schemas/afb-event-v1\"},\"af"
    "b-reply-v1\":{\"title\":\"Generic response.\",\"type\":\"object\",\"requ"
    "ired\":[\"jtype\",\"request\"],\"properties\":{\"jtype\":{\"type\":\"str"
    "ing\",\"const\":\"afb-reply\"},\"request\":{\"type\":\"object\",\"requir"
    "ed\":[\"status\"],\"properties\":{\"status\":{\"type\":\"string\"},\"inf"
    "o\":{\"type\":\"string\"},\"token\":{\"type\":\"string\"},\"uuid\":{\"ty"
    "pe\":\"string\"},\"reqid\":{\"type\":\"string\"}}},\"response\":{\"type\""
    ":\"object\"}}},\"afb-event-v1\":{\"type\":\"object\",\"required\":[\"jty"
    "pe\",\"event\"],\"properties\":{\"jtype\":{\"type\":\"string\",\"const\""
    ":\"afb-event\"},\"event\":{\"type\":\"string\"},\"data\":{\"type\":\"obj"
    "ect\"}}},\"set-verbosity\":{\"anyOf\":[{\"$ref\":\"#/components/schemas/"
    "verbosity-map\"},{\"$ref\":\"#/components/schemas/verbosity-level\"}]},\""
    "get-request\":{\"type\":\"object\",\"properties\":{\"verbosity\":{\"$ref"
    "\":\"#/components/schemas/get-verbosity\"},\"apis\":{\"$ref\":\"#/compon"
    "ents/schemas/get-apis\"}}},\"get-response\":{\"type\":\"object\",\"prope"
    "rties\":{\"verbosity\":{\"$ref\":\"#/components/schemas/verbosity-map\"}"
    ",\"apis\":{\"type\":\"object\"}}},\"get-verbosity\":{\"anyOf\":[{\"type\""
    ":\"boolean\"},{\"type\":\"array\",\"items\":{\"type\":\"string\"}},{\"ty"
    "pe\":\"object\"}]},\"get-apis\":{\"anyOf\":[{\"type\":\"boolean\"},{\"ty"
    "pe\":\"array\",\"items\":{\"type\":\"string\"}},{\"type\":\"object\"}]},"
    "\"verbosity-map\":{\"type\":\"object\",\"patternProperties\":{\"^.*$\":{"
    "\"$ref\":\"#/components/schemas/verbosity-level\"}}},\"verbosity-level\""
    ":{\"enum\":[\"debug\",3,\"info\",2,\"notice\",\"warning\",1,\"error\",0]"
    "},\"trace-add\":{\"anyOf\":[{\"type\":\"array\",\"items\":{\"$ref\":\"#/"
    "components/schemas/trace-add-object\"}},{\"$ref\":\"#/components/schemas"
    "/trace-add-any\"}]},\"trace-add-any\":{\"anyOf\":[{\"$ref\":\"#/componen"
    "ts/schemas/trace-add-request\"},{\"$ref\":\"#/components/schemas/trace-a"
    "dd-object\"}]},\"trace-add-object\":{\"type\":\"object\",\"properties\":"
    "{\"name\":{\"type\":\"string\",\"description\":\"name of the generated e"
    "vent\",\"default\":\"trace\"},\"tag\":{\"type\":\"string\",\"description"
    "\":\"tag for grouping traces\",\"default\":\"trace\"},\"api\":{\"type\":"
    "\"string\",\"description\":\"api for requests, daemons and services\"},\""
    "verb\":{\"type\":\"string\",\"description\":\"verb for requests\"},\"uui"
    "d\":{\"type\":\"string\",\"description\":\"uuid of session for requests\""
    "},\"pattern\":{\"type\":\"string\",\"description\":\"pattern for events\""
    "},\"request\":{\"$ref\":\"#/components/schemas/trace-add-request\"},\"da"
    "emon\":{\"$ref\":\"#/components/schemas/trace-add-daemon\"},\"service\":"
    "{\"$ref\":\"#/components/schemas/trace-add-service\"},\"event\":{\"$ref\""
    ":\"#/components/schemas/trace-add-event\"},\"session\":{\"$ref\":\"#/com"
    "ponents/schemas/trace-add-session\"},\"for\":{\"$ref\":\"#/components/sc"
    "hemas/trace-add\"}},\"examples\":[{\"tag\":\"1\",\"for\":[\"common\",{\""
    "api\":\"xxx\",\"request\":\"*\",\"daemon\":\"*\",\"service\":\"*\"}]}]},"
    "\"trace-add-request\":{\"anyOf\":[{\"type\":\"array\",\"items\":{\"$ref\""
    ":\"#/components/schemas/trace-request-names\"}},{\"$ref\":\"#/components"
    "/schemas/trace-request-names\"}]},\"trace-request-names\":{\"title\":\"n"
    "ame of traceable items of requests\",\"enum\":[\"*\",\"addref\",\"all\","
    "\"args\",\"begin\",\"common\",\"context\",\"context_get\",\"context_set\""
    ",\"end\",\"event\",\"extra\",\"fail\",\"get\",\"json\",\"life\",\"ref\","
    "\"result\",\"session\",\"session_close\",\"session_set_LOA\",\"simple\","
    "\"store\",\"stores\",\"subcall\",\"subcall_result\",\"subcalls\",\"subca"
    "llsync\",\"subcallsync_result\",\"subscribe\",\"success\",\"unref\",\"un"
    "store\",\"unsubscribe\",\"vverbose\"]},\"trace-add-daemon\":{\"anyOf\":["
    "{\"type\":\"array\",\"items\":{\"$ref\":\"#/components/schemas/trace-dae"
    "mon-names\"}},{\"$ref\":\"#/components/schemas/trace-daemon-names\"}]},\""
    "trace-daemon-names\":{\"title\":\"name of traceable items of daemons\",\""
    "enum\":[\"*\",\"all\",\"common\",\"event_broadcast_after\",\"event_broad"
    "cast_before\",\"event_make\",\"extra\",\"get_event_loop\",\"get_system_b"
    "us\",\"get_user_bus\",\"queue_job\",\"require_api\",\"require_api_result"
    "\",\"rootdir_get_fd\",\"rootdir_open_locale\",\"unstore_req\",\"vverbose"
    "\"]},\"trace-add-service\":{\"anyOf\":[{\"type\":\"array\",\"items\":{\""
    "$ref\":\"#/components/schemas/trace-service-names\"}},{\"$ref\":\"#/comp"
    "onents/schemas/trace-service-names\"}]},\"trace-service-names\":{\"title"
    "\":\"name of traceable items of services\",\"enum\":[\"*\",\"all\",\"cal"
    "l\",\"call_result\",\"callsync\",\"callsync_result\",\"on_event_after\","
    "\"on_event_before\",\"start_after\",\"start_before\"]},\"trace-add-event"
    "\":{\"anyOf\":[{\"type\":\"array\",\"items\":{\"$ref\":\"#/components/sc"
    "hemas/trace-event-names\"}},{\"$ref\":\"#/components/schemas/trace-event"
    "-names\"}]},\"trace-event-names\":{\"title\":\"name of traceable items o"
    "f events\",\"enum\":[\"*\",\"all\",\"broadcast_after\",\"broadcast_befor"
    "e\",\"common\",\"create\",\"drop\",\"extra\",\"name\",\"push_after\",\"p"
    "ush_before\"]},\"trace-add-session\":{\"anyOf\":[{\"type\":\"array\",\"i"
    "tems\":{\"$ref\":\"#/components/schemas/trace-session-names\"}},{\"$ref\""
    ":\"#/components/schemas/trace-session-names\"}]},\"trace-session-names\""
    ":{\"title\":\"name of traceable items for sessions\",\"enum\":[\"*\",\"a"
    "ddref\",\"all\",\"close\",\"common\",\"create\",\"destroy\",\"renew\",\""
    "unref\"]},\"trace-drop\":{\"anyOf\":[{\"type\":\"boolean\"},{\"type\":\""
    "object\",\"properties\":{\"event\":{\"anyOf\":[{\"type\":\"string\"},{\""
    "type\":\"array\",\"items\":\"string\"}]},\"tag\":{\"anyOf\":[{\"type\":\""
    "string\"},{\"type\":\"array\",\"items\":\"string\"}]},\"uuid\":{\"anyOf\""
    ":[{\"type\":\"string\"},{\"type\":\"array\",\"items\":\"string\"}]}}}]}}"
    ",\"x-permissions\":{\"trace\":{\"permission\":\"urn:AGL:permission:monit"
    "or:public:trace\"},\"set\":{\"permission\":\"urn:AGL:permission:monitor:"
    "public:set\"},\"get\":{\"permission\":\"urn:AGL:permission:monitor:publi"
    "c:get\"},\"get-or-set\":{\"anyOf\":[{\"$ref\":\"#/components/x-permissio"
    "ns/get\"},{\"$ref\":\"#/components/x-permissions/set\"}]}}},\"paths\":{\""
    "/get\":{\"description\":\"Get monitoring data.\",\"get\":{\"x-permission"
    "s\":{\"$ref\":\"#/components/x-permissions/get-or-set\"},\"parameters\":"
    "[{\"in\":\"query\",\"name\":\"verbosity\",\"required\":false,\"schema\":"
    "{\"$ref\":\"#/components/schemas/get-verbosity\"}},{\"in\":\"query\",\"n"
    "ame\":\"apis\",\"required\":false,\"schema\":{\"$ref\":\"#/components/sc"
    "hemas/get-apis\"}}],\"responses\":{\"200\":{\"description\":\"A complex "
    "object array response\",\"content\":{\"application/json\":{\"schema\":{\""
    "$ref\":\"#/components/schemas/afb-reply\"}}}}}}},\"/set\":{\"description"
    "\":\"Set monitoring actions.\",\"get\":{\"x-permissions\":{\"$ref\":\"#/"
    "components/x-permissions/set\"},\"parameters\":[{\"in\":\"query\",\"name"
    "\":\"verbosity\",\"required\":false,\"schema\":{\"$ref\":\"#/components/"
    "schemas/set-verbosity\"}}],\"responses\":{\"200\":{\"description\":\"A c"
    "omplex object array response\",\"content\":{\"application/json\":{\"sche"
    "ma\":{\"$ref\":\"#/components/schemas/afb-reply\"}}}}}}},\"/trace\":{\"d"
    "escription\":\"Set monitoring actions.\",\"get\":{\"x-permissions\":{\"$"
    "ref\":\"#/components/x-permissions/trace\"},\"parameters\":[{\"in\":\"qu"
    "ery\",\"name\":\"add\",\"required\":false,\"schema\":{\"$ref\":\"#/compo"
    "nents/schemas/trace-add\"}},{\"in\":\"query\",\"name\":\"drop\",\"requir"
    "ed\":false,\"schema\":{\"$ref\":\"#/components/schemas/trace-drop\"}}],\""
    "responses\":{\"200\":{\"description\":\"A complex object array response\""
    ",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/"
    "schemas/afb-reply\"}}}}}}}}}"
;

static const struct afb_auth _afb_auths_v2_monitor[] = {
	{ .type = afb_auth_Permission, .text = "urn:AGL:permission:monitor:public:set" },
	{ .type = afb_auth_Permission, .text = "urn:AGL:permission:monitor:public:get" },
	{ .type = afb_auth_Or, .first = &_afb_auths_v2_monitor[1], .next = &_afb_auths_v2_monitor[0] },
	{ .type = afb_auth_Permission, .text = "urn:AGL:permission:monitor:public:trace" }
};

static void f_get(struct afb_req req);
static void f_set(struct afb_req req);
static void f_trace(struct afb_req req);

static const struct afb_verb_v2 _afb_verbs_v2_monitor[] = {
    {
        .verb = "get",
        .callback = f_get,
        .auth = &_afb_auths_v2_monitor[2],
        .info = "Get monitoring data.",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "set",
        .callback = f_set,
        .auth = &_afb_auths_v2_monitor[0],
        .info = "Set monitoring actions.",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "trace",
        .callback = f_trace,
        .auth = &_afb_auths_v2_monitor[3],
        .info = "Set monitoring actions.",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = NULL,
        .callback = NULL,
        .auth = NULL,
        .info = NULL,
        .session = 0
	}
};

static const struct afb_binding_v2 _afb_binding_v2_monitor = {
    .api = "monitor",
    .specification = _afb_description_v2_monitor,
    .info = "monitoring of bindings and internals",
    .verbs = _afb_verbs_v2_monitor,
    .preinit = NULL,
    .init = NULL,
    .onevent = NULL,
    .noconcurrency = 0
};

