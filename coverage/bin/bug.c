#include <errno.h>
#include <stdint.h>
static int ok()
{
	return 0;
}
static int bug()
{
	errno = 0;
	return ((int(*)())(intptr_t)0)();
}
static int err()
{
	errno = EAGAIN;
	return -1;
}
/**************************************************************************/
/**************************************************************************/
/***           BINDINGS V2                                              ***/
/**************************************************************************/
/**************************************************************************/
#if defined(BUG1)  /* incomplete exports: afbBindingV2data miss */

#define AFB_BINDING_VERSION 0
#include <afb/afb-binding.h>
const struct afb_binding_v2 afbBindingV2;

#endif
/**************************************************************************/
#if defined(BUG2)  /* incomplete exports: afbBindingV2 miss */

#define AFB_BINDING_VERSION 0
#include <afb/afb-binding.h>
struct afb_binding_data_v2 afbBindingV2data;

#endif
/**************************************************************************/
#if defined(BUG3)  /* zero filled structure */

#define AFB_BINDING_VERSION 0
#include <afb/afb-binding.h>
const struct afb_binding_v2 afbBindingV2;
struct afb_binding_data_v2 afbBindingV2data;

#endif
/**************************************************************************/
#if defined(BUG4)  /* no verb definition */

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

const struct afb_binding_v2 afbBindingV2 = {
	.api = "bug4",
        .preinit = (void*)ok,
        .init = (void*)ok
};
#endif
/**************************************************************************/
#if defined(BUG5)  /* preinit buggy */

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

struct afb_verb_v2 verbs[] = {
	{ NULL }
};
const struct afb_binding_v2 afbBindingV2 = {
	.api = "bug5",
	.verbs = verbs,
        .preinit = (void*)bug,
        .init = (void*)ok
};
#endif
/**************************************************************************/
#if defined(BUG6)  /* buggy init */

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

struct afb_verb_v2 verbs[] = {
	{ NULL }
};
const struct afb_binding_v2 afbBindingV2 = {
	.api = "bug6",
	.verbs = verbs,
        .preinit = (void*)ok,
        .init = (void*)bug
};
#endif
/**************************************************************************/
#if defined(BUG7)  /* error in preinit */

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

struct afb_verb_v2 verbs[] = {
	{ NULL }
};
const struct afb_binding_v2 afbBindingV2 = {
	.api = "bug7",
	.verbs = verbs,
        .preinit = (void*)err,
        .init = (void*)ok
};
#endif
/**************************************************************************/
#if defined(BUG8)  /* error in init */

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

struct afb_verb_v2 verbs[] = {
	{ NULL }
};
const struct afb_binding_v2 afbBindingV2 = {
	.api = "bug8",
	.verbs = verbs,
        .preinit = (void*)ok,
        .init = (void*)err
};
#endif
/**************************************************************************/
#if defined(BUG9)  /* no api name */

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

struct afb_verb_v2 verbs[] = {
	{ NULL }
};
const struct afb_binding_v2 afbBindingV2 = {
	.verbs = verbs,
        .preinit = (void*)ok,
        .init = (void*)ok
};
#endif
/**************************************************************************/
#if defined(BUG10)  /* bad api name */

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

struct afb_verb_v2 verbs[] = {
	{ NULL }
};
const struct afb_binding_v2 afbBindingV2 = {
	.api = "bug 10",
	.verbs = verbs,
        .preinit = (void*)ok,
        .init = (void*)err
};
#endif
/**************************************************************************/
/**************************************************************************/
/***           BINDINGS V3                                              ***/
/**************************************************************************/
/**************************************************************************/
#if defined(BUG11) /* make a SEGV */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>
int afbBindingEntry(afb_api_t api)
{
	return ((int(*)())(intptr_t)0)();
}
#endif
/**************************************************************************/
#if defined(BUG12) /* no afbBindingV3 nor afbBindingV3entry */

#define AFB_BINDING_VERSION 0
#include <afb/afb-binding.h>
struct afb_api_x3 *afbBindingV3root;

#endif
/**************************************************************************/
#if defined(BUG13) /* no afbBindingV3root nor afbBindingV3entry */

#define AFB_BINDING_VERSION 0
#include <afb/afb-binding.h>
const struct afb_binding_v3 afbBindingV3;
int afbBindingV3entry(struct afb_api_x3 *rootapi) { return 0; }

#endif
/**************************************************************************/
#if defined(BUG14) /* no api name */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

const struct afb_binding_v3 afbBindingV3;

#endif
/**************************************************************************/
#if defined(BUG15) /* bad api name */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

const struct afb_binding_v3 afbBindingV3 = {
	.api = "bug 15"
};

#endif
/**************************************************************************/
#if defined(BUG16) /* both entry and preinit */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

int afbBindingV3entry(struct afb_api_x3 *rootapi) { return 0; }
const struct afb_binding_v3 afbBindingV3 = {
	.api = "bug16",
	.preinit = afbBindingV3entry
};

#endif
/**************************************************************************/
#if defined(BUG17) /* entry fails */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

int afbBindingV3entry(struct afb_api_x3 *rootapi) { errno = EAGAIN; return -1; }
#endif
/**************************************************************************/
#if defined(BUG18) /* preinit fails */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

const struct afb_binding_v3 afbBindingV3 = {
	.api = "bug18",
	.preinit = (void*)err
};

#endif
/**************************************************************************/
#if defined(BUG19) /* preinit SEGV */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

const struct afb_binding_v3 afbBindingV3 = {
	.api = "bug19",
	.preinit = (void*)bug
};

#endif
/**************************************************************************/
#if defined(BUG20) /* init fails */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

const struct afb_binding_v3 afbBindingV3 = {
	.api = "bug20",
	.init = (void*)err
};

#endif
/**************************************************************************/
#if defined(BUG21) /* init SEGV */

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

const struct afb_binding_v3 afbBindingV3 = {
	.api = "bug21",
	.init = (void*)bug,
	.provide_class = "a b c",
	.require_class = "x y z",
	.require_api = "bug4 bug5",
};

#endif
/**************************************************************************/
