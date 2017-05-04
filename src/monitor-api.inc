
static const char _afb_description_v2_[] =
    "{\"namespace\":\"urn:AGL:binding\",\"api\":\"monitor\",\"doc\":\"monitor"
    " the bindings\",\"verbs\":{\"set\":{\"doc\":\"set various values\",\"per"
    "missions\":{\"$ref\":\"#/permissions/set\"},\"LOA\":{\"minimum\":0},\"re"
    "quest\":{\"$ref\":\"#/types/set-request\"},\"response\":{\"$ref\":\"#/ty"
    "pes/set-response\"}},\"get\":{\"doc\":\"get the verbosity of apis\",\"pe"
    "rmissions\":{\"$ref\":\"#/permissions/get-or-set\"},\"LOA\":{\"minimum\""
    ":0},\"request\":{\"$ref\":\"#/types/get-request\"},\"response\":{\"$ref\""
    ":\"#/types/get-response\"}}},\"permissions\":{\"get\":{\"permission\":\""
    "urn:AGL:permission::platform:monitor:get\"},\"set\":{\"permission\":\"ur"
    "n:AGL:permission::platform:monitor:set\"},\"get-or-set\":{\"anyOf\":[{\""
    "$ref\":\"#/permissions/get\"},{\"$ref\":\"#/permissions/set\"}]}},\"type"
    "s\":{\"set-request\":{\"type\":\"object\",\"properties\":{\"verbosity\":"
    "{\"$ref\":\"#/types/set-verbosity\"}}},\"set-response\":{\"type\":\"obje"
    "ct\"},\"set-verbosity\":{\"anyOf\":[{\"$ref\":\"#/types/verbosity-map\"}"
    ",{\"$ref\":\"#/types/verbosity-level\"}]},\"get-request\":{\"type\":\"ob"
    "ject\",\"properties\":{\"verbosity\":{\"$ref\":\"#/types/get-verbosity\""
    "},\"apis\":{\"$ref\":\"#/types/get-apis\"}}},\"get-response\":{\"type\":"
    "\"object\",\"properties\":{\"verbosity\":{\"$ref\":\"#/types/verbosity-m"
    "ap\"},\"apis\":{\"type\":\"object\"}}},\"get-verbosity\":{\"anyOf\":[{\""
    "type\":\"boolean\"},{\"type\":\"array\",\"items\":{\"type\":\"string\"}}"
    ",{\"type\":\"object\"}]},\"get-apis\":{\"anyOf\":[{\"type\":\"boolean\"}"
    ",{\"type\":\"array\",\"items\":{\"type\":\"string\"}},{\"type\":\"object"
    "\"}]},\"verbosity-map\":{\"type\":\"object\",\"patternProperties\":{\"^."
    "*$\":{\"$ref\":\"#/types/verbosity-level\"}}},\"verbosity-level\":{\"enu"
    "m\":[\"debug\",3,\"info\",2,\"notice\",\"warning\",1,\"error\",0]}},\"me"
    "ta-binding\":{\"prefix\":\"f_\",\"postfix\":\"\",\"start\":null,\"oneven"
    "t\":null,\"init\":null,\"scope\":\"static\",\"private\":true}}"
;

static void f_get(struct afb_req req);
static void f_set(struct afb_req req);

static const struct afb_verb_v2 _afb_verbs_v2_[] = {
    {
        .verb = "get",
        .callback = f_get,
        .auth = NULL,
        .session = AFB_SESSION_LOA_GE_0,
    },
    {
        .verb = "set",
        .callback = f_set,
        .auth = NULL,
        .session = AFB_SESSION_LOA_GE_0,
    },
    { .verb = NULL }
};

static const struct afb_binding_v2 _afb_binding_v2_ = {
    .api = "monitor",
    .specification = _afb_description_v2_,
    .verbs = _afb_verbs_v2_,
    .init = NULL,
    .start = NULL,
    .onevent = NULL,
};

