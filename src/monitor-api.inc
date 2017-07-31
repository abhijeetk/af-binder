
static const char _afb_description_v2_monitor[] =
    "{\"openapi\":\"3.0.0\",\"info\":{\"description\":\"\",\"title\":\"monito"
    "r\",\"version\":\"1.0\",\"x-binding-c-generator\":{\"api\":\"monitor\",\""
    "version\":2,\"prefix\":\"f_\",\"postfix\":\"\",\"preinit\":null,\"init\""
    ":null,\"onevent\":null,\"scope\":\"static\",\"private\":true}},\"servers"
    "\":[{\"url\":\"ws://{host}:{port}/api/monitor\",\"description\":\"The AP"
    "I server.\",\"variables\":{\"host\":{\"default\":\"localhost\"},\"port\""
    ":{\"default\":\"1234\"}},\"x-afb-events\":[{\"$ref\":\"#/components/sche"
    "mas/afb-event\"}]}],\"components\":{\"schemas\":{\"afb-reply\":{\"$ref\""
    ":\"#/components/schemas/afb-reply-v1\"},\"afb-event\":{\"$ref\":\"#/comp"
    "onents/schemas/afb-event-v1\"},\"afb-reply-v1\":{\"title\":\"Generic res"
    "ponse.\",\"type\":\"object\",\"required\":[\"jtype\",\"request\"],\"prop"
    "erties\":{\"jtype\":{\"type\":\"string\",\"const\":\"afb-reply\"},\"requ"
    "est\":{\"type\":\"object\",\"required\":[\"status\"],\"properties\":{\"s"
    "tatus\":{\"type\":\"string\"},\"info\":{\"type\":\"string\"},\"token\":{"
    "\"type\":\"string\"},\"uuid\":{\"type\":\"string\"},\"reqid\":{\"type\":"
    "\"string\"}}},\"response\":{\"type\":\"object\"}}},\"afb-event-v1\":{\"t"
    "ype\":\"object\",\"required\":[\"jtype\",\"event\"],\"properties\":{\"jt"
    "ype\":{\"type\":\"string\",\"const\":\"afb-event\"},\"event\":{\"type\":"
    "\"string\"},\"data\":{\"type\":\"object\"}}},\"set-verbosity\":{\"anyOf\""
    ":[{\"$ref\":\"#/components/schemas/verbosity-map\"},{\"$ref\":\"#/compon"
    "ents/schemas/verbosity-level\"}]},\"get-request\":{\"type\":\"object\",\""
    "properties\":{\"verbosity\":{\"$ref\":\"#/components/schemas/get-verbosi"
    "ty\"},\"apis\":{\"$ref\":\"#/components/schemas/get-apis\"}}},\"get-resp"
    "onse\":{\"type\":\"object\",\"properties\":{\"verbosity\":{\"$ref\":\"#/"
    "components/schemas/verbosity-map\"},\"apis\":{\"type\":\"object\"}}},\"g"
    "et-verbosity\":{\"anyOf\":[{\"type\":\"boolean\"},{\"type\":\"array\",\""
    "items\":{\"type\":\"string\"}},{\"type\":\"object\"}]},\"get-apis\":{\"a"
    "nyOf\":[{\"type\":\"boolean\"},{\"type\":\"array\",\"items\":{\"type\":\""
    "string\"}},{\"type\":\"object\"}]},\"verbosity-map\":{\"type\":\"object\""
    ",\"patternProperties\":{\"^.*$\":{\"$ref\":\"#/components/schemas/verbos"
    "ity-level\"}}},\"verbosity-level\":{\"enum\":[\"debug\",3,\"info\",2,\"n"
    "otice\",\"warning\",1,\"error\",0]},\"trace-add\":{\"anyOf\":[{\"type\":"
    "\"array\",\"items\":{\"$ref\":\"#/components/schemas/trace-add-object\"}"
    "},{\"$ref\":\"#/components/schemas/trace-add-any\"}]},\"trace-add-any\":"
    "{\"anyOf\":[{\"$ref\":\"#/components/schemas/trace-add-request\"},{\"$re"
    "f\":\"#/components/schemas/trace-add-object\"}]},\"trace-add-object\":{\""
    "type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\",\"descri"
    "ption\":\"name of the generated event\",\"default\":\"trace\"},\"tag\":{"
    "\"type\":\"string\",\"description\":\"tag for grouping traces\",\"defaul"
    "t\":\"trace\"},\"api\":{\"type\":\"string\",\"description\":\"api for re"
    "quests, daemons and services\"},\"verb\":{\"type\":\"string\",\"descript"
    "ion\":\"verb for requests\"},\"session\":{\"type\":\"string\",\"descript"
    "ion\":\"session for requests\"},\"pattern\":{\"type\":\"string\",\"descr"
    "iption\":\"pattern for events\"},\"request\":{\"$ref\":\"#/components/sc"
    "hemas/trace-add-request\"},\"daemon\":{\"$ref\":\"#/components/schemas/t"
    "race-add-daemon\"},\"service\":{\"$ref\":\"#/components/schemas/trace-ad"
    "d-service\"},\"event\":{\"$ref\":\"#/components/schemas/trace-add-event\""
    "},\"for\":{\"$ref\":\"#/components/schemas/trace-add\"}},\"examples\":[{"
    "\"tag\":\"1\",\"for\":[\"common\",{\"api\":\"xxx\",\"request\":\"*\",\"d"
    "aemon\":\"*\",\"service\":\"*\"}]}]},\"trace-add-request\":{\"anyOf\":[{"
    "\"type\":\"array\",\"items\":{\"$ref\":\"#/components/schemas/trace-requ"
    "est-names\"}},{\"$ref\":\"#/components/schemas/trace-request-names\"}]},"
    "\"trace-request-names\":{\"title\":\"name of traceable items of requests"
    "\",\"enum\":[\"*\",\"addref\",\"all\",\"args\",\"begin\",\"common\",\"co"
    "ntext\",\"context_get\",\"context_set\",\"end\",\"event\",\"extra\",\"fa"
    "il\",\"get\",\"json\",\"life\",\"ref\",\"result\",\"session\",\"session_"
    "close\",\"session_set_LOA\",\"simple\",\"store\",\"stores\",\"subcall\","
    "\"subcall_result\",\"subcalls\",\"subcallsync\",\"subcallsync_result\",\""
    "subscribe\",\"success\",\"unref\",\"unstore\",\"unsubscribe\",\"vverbose"
    "\"]},\"trace-add-daemon\":{\"anyOf\":[{\"type\":\"array\",\"items\":{\"$"
    "ref\":\"#/components/schemas/trace-daemon-names\"}},{\"$ref\":\"#/compon"
    "ents/schemas/trace-daemon-names\"}]},\"trace-daemon-names\":{\"title\":\""
    "name of traceable items of daemons\",\"enum\":[\"*\",\"all\",\"common\","
    "\"event_broadcast_after\",\"event_broadcast_before\",\"event_make\",\"ex"
    "tra\",\"get_event_loop\",\"get_system_bus\",\"get_user_bus\",\"queue_job"
    "\",\"require_api\",\"require_api_result\",\"rootdir_get_fd\",\"rootdir_o"
    "pen_locale\",\"unstore_req\",\"vverbose\"]},\"trace-add-service\":{\"any"
    "Of\":[{\"type\":\"array\",\"items\":{\"$ref\":\"#/components/schemas/tra"
    "ce-service-names\"}},{\"$ref\":\"#/components/schemas/trace-service-name"
    "s\"}]},\"trace-service-names\":{\"title\":\"name of traceable items of s"
    "ervices\",\"enum\":[\"*\",\"all\",\"call\",\"call_result\",\"callsync\","
    "\"callsync_result\",\"on_event_after\",\"on_event_before\",\"start_after"
    "\",\"start_before\"]},\"trace-add-event\":{\"anyOf\":[{\"type\":\"array\""
    ",\"items\":{\"$ref\":\"#/components/schemas/trace-event-names\"}},{\"$re"
    "f\":\"#/components/schemas/trace-event-names\"}]},\"trace-event-names\":"
    "{\"title\":\"name of traceable items of events\",\"enum\":[\"*\",\"all\""
    ",\"broadcast_after\",\"broadcast_before\",\"common\",\"create\",\"drop\""
    ",\"extra\",\"name\",\"push_after\",\"push_before\"]},\"trace-drop\":{\"a"
    "nyOf\":[{\"type\":\"boolean\"},{\"type\":\"object\",\"properties\":{\"ev"
    "ent\":{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"array\",\"items\":\""
    "string\"}]},\"tag\":{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"array\""
    ",\"items\":\"string\"}]},\"session\":{\"anyOf\":[{\"type\":\"string\"},{"
    "\"type\":\"array\",\"items\":\"string\"}]}}}]}},\"x-permissions\":{\"set"
    "\":{\"permission\":\"urn:AGL:permission:monitor:public:set\"},\"get\":{\""
    "permission\":\"urn:AGL:permission:monitor:public:get\"},\"get-or-set\":{"
    "\"anyOf\":[{\"$ref\":\"#/components/x-permissions/get\"},{\"$ref\":\"#/c"
    "omponents/x-permissions/set\"}]}}},\"paths\":{\"/get\":{\"description\":"
    "\"Get monitoring data.\",\"get\":{\"x-permissions\":{\"$ref\":\"#/compon"
    "ents/x-permissions/get-or-set\"},\"parameters\":[{\"in\":\"query\",\"nam"
    "e\":\"verbosity\",\"required\":false,\"schema\":{\"$ref\":\"#/components"
    "/schemas/get-verbosity\"}},{\"in\":\"query\",\"name\":\"apis\",\"require"
    "d\":false,\"schema\":{\"$ref\":\"#/components/schemas/get-apis\"}}],\"re"
    "sponses\":{\"200\":{\"description\":\"A complex object array response\","
    "\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/s"
    "chemas/afb-reply\"}}}}}}},\"/set\":{\"description\":\"Set monitoring act"
    "ions.\",\"get\":{\"x-permissions\":{\"$ref\":\"#/components/x-permission"
    "s/set\"},\"parameters\":[{\"in\":\"query\",\"name\":\"verbosity\",\"requ"
    "ired\":false,\"schema\":{\"$ref\":\"#/components/schemas/set-verbosity\""
    "}}],\"responses\":{\"200\":{\"description\":\"A complex object array res"
    "ponse\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/comp"
    "onents/schemas/afb-reply\"}}}}}}},\"/trace\":{\"description\":\"Set moni"
    "toring actions.\",\"get\":{\"x-permissions\":{\"$ref\":\"#/components/x-"
    "permissions/set\"},\"parameters\":[{\"in\":\"query\",\"name\":\"add\",\""
    "required\":false,\"schema\":{\"$ref\":\"#/components/schemas/trace-add\""
    "}},{\"in\":\"query\",\"name\":\"drop\",\"required\":false,\"schema\":{\""
    "$ref\":\"#/components/schemas/trace-drop\"}}],\"responses\":{\"200\":{\""
    "description\":\"A complex object array response\",\"content\":{\"applica"
    "tion/json\":{\"schema\":{\"$ref\":\"#/components/schemas/afb-reply\"}}}}"
    "}}}}}"
;

static const struct afb_auth _afb_auths_v2_monitor[] = {
	{ .type = afb_auth_Permission, .text = "urn:AGL:permission:monitor:public:set" },
	{ .type = afb_auth_Permission, .text = "urn:AGL:permission:monitor:public:get" },
	{ .type = afb_auth_Or, .first = &_afb_auths_v2_monitor[1], .next = &_afb_auths_v2_monitor[0] }
};

static void f_get(struct afb_req req);
static void f_set(struct afb_req req);
static void f_trace(struct afb_req req);

static const struct afb_verb_v2 _afb_verbs_v2_monitor[] = {
    {
        .verb = "get",
        .callback = f_get,
        .auth = &_afb_auths_v2_monitor[2],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "set",
        .callback = f_set,
        .auth = &_afb_auths_v2_monitor[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "trace",
        .callback = f_trace,
        .auth = &_afb_auths_v2_monitor[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    { .verb = NULL }
};

static const struct afb_binding_v2 _afb_binding_v2_monitor = {
    .api = "monitor",
    .specification = _afb_description_v2_monitor,
    .info = NULL,
    .verbs = _afb_verbs_v2_monitor,
    .preinit = NULL,
    .init = NULL,
    .onevent = NULL,
    .noconcurrency = 0
};

