#include <string.h>
#include <json-c/json.h>

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

afb_event event_login, event_logout;

void login(afb_req req)
{
        json_object *args, *user, *passwd;
        char *usr;

        args = afb_req_json(req);
        if (!json_object_object_get_ex(args, "user", &user)
         || !json_object_object_get_ex(args, "password", &passwd)) {
                AFB_REQ_ERROR(req, "login, bad request: %s", json_object_get_string(args));
                afb_req_fail(req, "bad-request", NULL);
        } else if (afb_req_context_get(req)) {
                AFB_REQ_ERROR(req, "login, bad state, logout first");
                afb_req_fail(req, "bad-state", NULL);
        } else if (strcmp(json_object_get_string(passwd), "please")) {
                AFB_REQ_ERROR(req, "login, unauthorized: %s", json_object_get_string(args));
                afb_req_fail(req, "unauthorized", NULL);
        } else {
                usr = strdup(json_object_get_string(user));
                AFB_REQ_NOTICE(req, "login user: %s", usr);
                afb_req_session_set_LOA(req, 1);
                afb_req_context_set(req, usr, free);
                afb_req_success(req, NULL, NULL);
                afb_event_push(event_login, json_object_new_string(usr));
        }
}

void action(afb_req req)
{
        json_object *args, *val;
        char *usr;

        args = afb_req_json(req);
        usr = afb_req_context_get(req);
        AFB_REQ_NOTICE(req, "action for user %s: %s", usr, json_object_get_string(args));
        if (json_object_object_get_ex(args, "subscribe", &val)) {
                if (json_object_get_boolean(val)) {
                        AFB_REQ_NOTICE(req, "user %s subscribes to events", usr);
                        afb_req_subscribe(req, event_login);
                        afb_req_subscribe(req, event_logout);
                } else {
                        AFB_REQ_NOTICE(req, "user %s unsubscribes to events", usr);
                        afb_req_unsubscribe(req, event_login);
                        afb_req_unsubscribe(req, event_logout);
                }
        }
        afb_req_success(req, json_object_get(args), NULL);
}

void logout(afb_req req)
{
        char *usr;

        usr = afb_req_context_get(req);
        AFB_REQ_NOTICE(req, "login user %s out", usr);
        afb_event_push(event_logout, json_object_new_string(usr));
        afb_req_session_set_LOA(req, 0);
        afb_req_context_clear(req);
        afb_req_success(req, NULL, NULL);
}

int preinit()
{
        AFB_NOTICE("preinit");
        return 0;
}

int init()
{
        AFB_NOTICE("init");
        event_login = afb_daemon_make_event("login");
        event_logout = afb_daemon_make_event("logout");
        if (afb_event_is_valid(event_login) && afb_event_is_valid(event_logout))
                return 0;
        AFB_ERROR("Can't create events");
        return -1;
}

const afb_verb_v2 verbs[] = {
        { .verb="login", .callback=login },
        { .verb="action", .callback=action, .session=AFB_SESSION_LOA_1 },
        { .verb="logout", .callback=logout, .session=AFB_SESSION_LOA_1 },
        { .verb=NULL }
};

const afb_binding_v2 afbBindingV2 = {
        .api = "tuto-2",
        .specification = NULL,
        .verbs = verbs,
        .preinit = preinit,
        .init = init,
        .noconcurrency = 0
};