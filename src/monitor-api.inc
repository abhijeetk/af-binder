
static const char _afb_description_v2_monitor[] =
    "{\"openapi\":\"3.0.0\",\"info\":{\"description\":\"\",\"title\":\"monito"
    "r\",\"version\":\"1.0\",\"x-binding\":{\"version\":2,\"prefix\":\"f_\",\""
    "postfix\":\"\",\"start\":null,\"onevent\":null,\"init\":null,\"scope\":\""
    "static\",\"private\":true}},\"servers\":[{\"url\":\"ws://{host}:{port}/a"
    "pi/monitor\",\"description\":\"The API server.\",\"variables\":{\"host\""
    ":{\"default\":\"localhost\"},\"port\":{\"default\":\"1234\"}},\"x-afb-ev"
    "ents\":[{\"$ref\":\"#/components/schemas/afb-event\"}]}],\"components\":"
    "{\"schemas\":{\"afb-reply\":{\"$ref\":\"#/components/schemas/afb-reply-v"
    "1\"},\"afb-event\":{\"$ref\":\"#/components/schemas/afb-event-v1\"},\"af"
    "b-reply-v1\":{\"title\":\"Generic response.\",\"type\":\"object\",\"requ"
    "ired\":[\"jtype\",\"request\"],\"properties\":{\"jtype\":{\"const\":\"af"
    "b-reply\"},\"request\":{\"type\":\"object\",\"required\":[\"status\"],\""
    "properties\":{\"status\":{\"type\":\"string\"},\"info\":{\"type\":\"stri"
    "ng\"},\"token\":{\"type\":\"string\"},\"uuid\":{\"type\":\"string\"},\"r"
    "eqid\":{\"type\":\"string\"}}},\"response\":{\"type\":\"object\"}}},\"af"
    "b-event-v1\":{\"type\":\"object\",\"properties\":{\"jtype\":{\"type\":\""
    "string\"},\"event\":{\"type\":\"string\"},\"data\":{\"type\":\"object\"}"
    "}}},\"x-permissions\":{\"set\":{\"permission\":\"urn:AGL:permission:moni"
    "tor:public:set\"},\"get\":{\"permission\":\"urn:AGL:permission:monitor:p"
    "ublic:get\"},\"get-or-set\":{\"anyOf\":[{\"$ref\":\"#/components/x-permi"
    "ssions/get\"},{\"$ref\":\"#/components/x-permissions/set\"}]}}},\"paths\""
    ":{\"/get\":{\"description\":\"Get monitoring data.\",\"x-permissions\":{"
    "\"$ref\":\"#/components/x-permissions/get-or-set\"},\"get\":{\"parameter"
    "s\":[{\"in\":\"query\",\"name\":\"filter\",\"required\":false,\"schema\""
    ":{\"type\":\"object\",\"properties\":{\"event\":{\"type\":\"string\"}}}}"
    "],\"responses\":{\"200\":{\"description\":\"A complex object array respo"
    "nse\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/compon"
    "ents/schemas/afb-reply\"}}}}}}},\"/set\":{\"description\":\"Set monitori"
    "ng actions.\",\"x-permissions\":{\"$ref\":\"#/components/x-permissions/s"
    "et\"},\"get\":{\"parameters\":[{\"in\":\"query\",\"name\":\"filter\",\"r"
    "equired\":false,\"schema\":{\"type\":\"object\",\"properties\":{\"event\""
    ":{\"type\":\"string\"}}}}],\"responses\":{\"200\":{\"description\":\"A c"
    "omplex object array response\",\"content\":{\"application/json\":{\"sche"
    "ma\":{\"$ref\":\"#/components/schemas/afb-reply\"}}}}}}}}}"
;

static const struct afb_auth _afb_auths_v2_monitor[] = {
	{ .type = afb_auth_Permission, .text = "urn:AGL:permission:monitor:public:set" },
	{ .type = afb_auth_Permission, .text = "urn:AGL:permission:monitor:public:get" },
	{ .type = afb_auth_Or, .first = &_afb_auths_v2_monitor[1], .next = &_afb_auths_v2_monitor[0] }
};

static void f_get(struct afb_req req);
static void f_set(struct afb_req req);

static const struct afb_verb_v2 _afb_verbs_v2_monitor[] = {
    {
        .verb = "get",
        .callback = f_get,
        .auth = &_afb_auths_v2_monitor[2],
        .session = 0,
    },
    {
        .verb = "set",
        .callback = f_set,
        .auth = &_afb_auths_v2_monitor[0],
        .session = 0,
    },
    { .verb = NULL }
};

static const struct afb_binding_v2 _afb_binding_v2_monitor = {
    .api = "monitor",
    .specification = _afb_description_v2_monitor,
    .verbs = _afb_verbs_v2_monitor,
    .init = NULL,
    .start = NULL,
    .onevent = NULL,
};

