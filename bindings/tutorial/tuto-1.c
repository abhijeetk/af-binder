#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

void hello(afb_req req)
{
	AFB_REQ_DEBUG(req, "hello world");
	afb_req_success(req, NULL, "hello world");
}

const afb_verb_v2 verbs[] = {
	{ .verb="hello", .callback=hello },
	{ .verb=NULL }
};

const afb_binding_v2 afbBindingV2 = {
	.api = "tuto-1",
	.verbs = verbs
};

