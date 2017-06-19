
Overview of the bindings
========================

The ***binder*** serves files through HTTP protocol and offers to
developers the capability to offer application API methods through HTTP or
WebSocket protocol.

The ***bindings*** are used to add **API** to ***binders***.
This part describes how to write a ***binding*** for ***binder***
or in other words how to add a new **API** to the system.

Excepting this summary, this section target developers.

This section shortly explain how to write a binding
using the C programming language.

It is convenient to install the ***binder*** on the
desktop used for writing the binding. It allows easy
debug and test.

## Nature of a binding

A ***binding*** is an independent piece of software compiled as a shared
library and dynamically loaded by a ***binder***.
It is intended to provide one **API** (**A**pplication **P**rogramming
**I**nterface).

The **API** is designated and accessed through its name.
It contains several **verbs** that implement the ***binding***
functionalities.  Each of these **verbs** is a **method** that
processes requests of applications and sends result.

The ***binding***'s methods are invoked by HTTP or websocket
requests.

The **methods** of the ***bindings*** are noted **api/verb**
where **api** is the **API** name of the binding and **verb** is
the **method**'s name within the **API**.
This notation comes from HTTP invocations that rely on URL path terminated
with **api/verb**.

The name of an **API** can be made of any characters except:

 - the control characters (\u0000 .. \u001f)
 - the characters of the set { ' ', '"', '#', '%', '&',
   '\'', '/', '?', '`', '\x7f' }

The names if the **verbs** can be any character.

The binder makes no distinctions between upper case and lower case
latin letters. So **API/VERB** matches **Api/Verb** or **api/verb**.

Actually it exists 2 ways of writing ***bindings***.
You can either write:

 - a binding version 1 (not recommended);
 - a binding version 2 (RECOMMENDED).

A ***binder*** loads and runs any of these version in any combination.
This document explain how to write bindings version 2.

<!-- pagebreak -->
Sample binding: tuto-1
======================

This is the code of the binding **tuto-1.c**:

```C
  1 #define AFB_BINDING_VERSION 2
  2 #include <afb/afb-binding.h>
  3
  4 void hello(afb_req req)
  5 {
  6         AFB_REQ_DEBUG(req, "hello world");
  7         afb_req_success(req, NULL, "hello world");
  8 }
  9
 10 const afb_verb_v2 verbs[] = {
 11         { .verb="hello", .callback=hello },
 12         { .verb=NULL }
 13 };
 14
 15 const afb_binding_v2 afbBindingV2 = {
 16         .api = "tuto-1",
 17         .verbs = verbs
 18 };
```

Compiling:

```bash
$ gcc -fPIC -shared tuto-1.c -o tuto-1.so $(pkg-config --cflags-only-I afb-daemon)
```

Running:

```bash
$ afb-daemon --binding tuto-1.so --port 3333 --token ''
```

Testing using **curl**:

```bash
$ curl http://localhost:3333/api/tuto-1/hello
{"jtype":"afb-reply","request":{"status":"success","info":"hello world","uuid":"1e587b54-900b-49ab-9940-46141bc2e1d6"}}
```

Testing using **afb-client-demo** (with option -H for
getting a human readable output):

```bash
$ afb-client-demo -H ws://localhost:3333/api?token=x tuto-1 hello
ON-REPLY 1:tuto-1/hello: OK
{
  "jtype":"afb-reply",
  "request":{
    "status":"success",
    "info":"hello world",
    "uuid":"03a84ad1-458a-4ace-af74-b1da917391b9"
  }
}
```

This shows basic things:

 - The include to get for creating a binding
 - How to declare the API offered by the binding
 - How to handle request made to the binding


### Getting declarations for the binding

The lines 1 and 2 show how to get the include file **afb-binding.h**.

```C
  1 #define AFB_BINDING_VERSION 2
  2 #include <afb/afb-binding.h>
```

You must define the version of ***binding*** that you are using.
This is done line 1 where we define that this is the version 2.

If you don't define it, a warning message is prompted by the compiler
and the version is switched to version 1. This behaviour is
temporarily and enables to continue to use previously written
***binding*** without change but it will change in some future when
***bindings*** V1 will become obsoletes.

To include **afb-binding.h** successfully, the include search path
should be set correctly if needed (not needed only if installed in
/usr/include/afb directory that is the default).

Setting the include path is easy using **pkg-config**:

```bash
$ pkg-config --cflags-only-I afb-daemon
```

Note for **C++** developers: The ***binder*** currently expose
only **C** language **API**. The file **afb/afb-binding.h**
isn't **C++** ready. You should use the construct **extern "C"**
as below:

```C
  #define AFB_BINDING_VERSION 2
  extern "C" {
  #include <afb/afb-binding.h>
  }
```

Future version of the ***binder*** will include a **C++**
interface. Until it is available, please, use the above
construct.

### Declaring the API of the binding

Lines 10 to 18 show the declaration of the ***binding***.

The ***binder*** knows that this is a ***binding*** version 2 because
it finds the exported symbol **afbBindingV2** that is expected to be
a structure of type **afb_binding_v2**.

```C
 10 const afb_verb_v2 verbs[] = {
 11         { .verb="hello", .callback=hello },
 12         { .verb=NULL }
 13 };
 14
 15 const afb_binding_v2 afbBindingV2 = {
 16         .api = "tuto-1",
 17         .verbs = verbs
 18 };
```

The structure **afbBindingV2** actually tells that:

 - the exported **API** name is **tuto-1** (line 16)
 - the array of verbs is the above defined one

The exported list of verb is specified by an array of structures,
each describing a verb, ended with a verb NULL (line 12).

The only defined verb here (line 11) is named **hello** (field **.verb**)
and the function that handle the related request is **hello**
(field **.callback**).

Note that you can explicitly mark the fact that these are
struct by typing the **struct** as below:

```C
 10 const struct afb_verb_v2 verbs[] = {
 11         { .verb="hello", .callback=hello },
 12         { .verb=NULL }
 13 };
 14
 15 const struct afb_binding_v2 afbBindingV2 = {
 16         .api = "tuto-1",
 17         .verbs = verbs
 18 };
```

### Handling binder's requests

As shown above this is by default the common include directory where
the AGL stuff is installed.

```C
  4 void hello(afb_req req)
  5 {
  6         AFB_REQ_DEBUG(req, "hello world");
  7         afb_req_success(req, NULL, "hello world");
  8 }
```

When the ***binder*** receives a request for the verb **hello** of
of the api **tuto-1**, it invoke the callback **hello** of the **binding**
with the argument **req** that handles the client request.

The callback has to treat synchronously or asynchronously the request and
should at the end emit a reply for the request.

Here, the callback for **tuto-1/hello** replies a successful answer
(line 7) to the request **req**. The second parameter (here NULL)
is a json object that is sent to the client with the reply.
The third parameter is also sent with the reply and is a string
called info that can be used as some meta data.

Here again, you can explicitly mark the fact that
**afb_req** is a structure by declaring **hello** as below:

```C
  4 void hello(struct afb_req req)
```

<!-- pagebreak -->
Sample binding: tuto-2
======================
The second tutorial shows many important feature that can
commonly be used when writing a ***binding***: initialization,
getting arguments, sending replies, pushing events.

This is the code of the binding **tuto-2.c**:

```C
      1 #include <string.h>
      2 #include <json-c/json.h>
      3
      4 #define AFB_BINDING_VERSION 2
      5 #include <afb/afb-binding.h>
      6
      7 afb_event event_login, event_logout;
      8
      9 void login(afb_req req)
     10 {
     11         json_object *args, *user, *passwd;
     12         char *usr;
     13
     14         args = afb_req_json(req);
     15         if (!json_object_object_get_ex(args, "user", &user)
     16          || !json_object_object_get_ex(args, "password", &passwd)) {
     17                 AFB_REQ_ERROR(req, "login, bad request: %s", json_object_get_string(args));
     18                 afb_req_fail(req, "bad-request", NULL);
     19         } else if (afb_req_context_get(req)) {
     20                 AFB_REQ_ERROR(req, "login, bad state, logout first");
     21                 afb_req_fail(req, "bad-state", NULL);
     22         } else if (strcmp(json_object_get_string(passwd), "please")) {
     23                 AFB_REQ_ERROR(req, "login, unauthorized: %s", json_object_get_string(args));
     24                 afb_req_fail(req, "unauthorized", NULL);
     25         } else {
     26                 usr = strdup(json_object_get_string(user));
     27                 AFB_REQ_NOTICE(req, "login user: %s", usr);
     28                 afb_req_session_set_LOA(req, 1);
     29                 afb_req_context_set(req, usr, free);
     30                 afb_req_success(req, NULL, NULL);
     31                 afb_event_push(event_login, json_object_new_string(usr));
     32         }
     33 }
     34
     35 void action(afb_req req)
     36 {
     37         json_object *args, *val;
     38         char *usr;
     39
     40         args = afb_req_json(req);
     41         usr = afb_req_context_get(req);
     42         AFB_REQ_NOTICE(req, "action for user %s: %s", usr, json_object_get_string(args));
     43         if (json_object_object_get_ex(args, "subscribe", &val)) {
     44                 if (json_object_get_boolean(val)) {
     45                         AFB_REQ_NOTICE(req, "user %s subscribes to events", usr);
     46                         afb_req_subscribe(req, event_login);
     47                         afb_req_subscribe(req, event_logout);
     48                 } else {
     49                         AFB_REQ_NOTICE(req, "user %s unsubscribes to events", usr);
     50                         afb_req_unsubscribe(req, event_login);
     51                         afb_req_unsubscribe(req, event_logout);
     52                 }
     53         }
     54         afb_req_success(req, json_object_get(args), NULL);
     55 }
     56
     57 void logout(afb_req req)
     58 {
     59         char *usr;
     60
     61         usr = afb_req_context_get(req);
     62         AFB_REQ_NOTICE(req, "login user %s out", usr);
     63         afb_event_push(event_logout, json_object_new_string(usr));
     64         afb_req_session_set_LOA(req, 0);
     65         afb_req_context_clear(req);
     66         afb_req_success(req, NULL, NULL);
     67 }
     68
     69 int preinit()
     70 {
     71         AFB_NOTICE("preinit");
     72         return 0;
     73 }
     74
     75 int init()
     76 {
     77         AFB_NOTICE("init");
     78         event_login = afb_daemon_make_event("login");
     79         event_logout = afb_daemon_make_event("logout");
     80         if (afb_event_is_valid(event_login) && afb_event_is_valid(event_logout))
     81                 return 0;
     82         AFB_ERROR("Can't create events");
     83         return -1;
     84 }
     85
     86 const afb_verb_v2 verbs[] = {
     87         { .verb="login", .callback=login },
     88         { .verb="action", .callback=action, .session=AFB_SESSION_LOA_1 },
     89         { .verb="logout", .callback=logout, .session=AFB_SESSION_LOA_1 },
     90         { .verb=NULL }
     91 };
     92
     93 const afb_binding_v2 afbBindingV2 = {
     94         .api = "tuto-2",
     95         .specification = NULL,
     96         .verbs = verbs,
     97         .preinit = preinit,
     98         .init = init,
     99         .noconcurrency = 0
    100 };
```

Compiling:

```bash
$ gcc -fPIC -shared tuto-2.c -o tuto-2.so $(pkg-config --cflags --libs afb-daemon)
```

Running:

```bash
$ afb-daemon --binding tuto-2.so --port 3333 --token ''
```

Testing:

```bash
$ afb-client-demo -H localhost:3333/api?token=toto
tuto-2 login {"help":true}
ON-REPLY 1:tuto-2/login: ERROR
{
  "jtype":"afb-reply",
  "request":{
    "status":"bad-request",
    "uuid":"e2b24a13-fc43-487e-a5f4-9266dd1e60a9"
  }
}
tuto-2 login {"user":"jose","password":"please"}
ON-REPLY 2:tuto-2/login: OK
{
  "jtype":"afb-reply",
  "request":{
    "status":"success"
  }
}
tuto-2 login {"user":"jobol","password":"please"}
ON-REPLY 3:tuto-2/login: ERROR
{
  "jtype":"afb-reply",
  "request":{
    "status":"bad-state"
  }
}
tuto-2 action {"subscribe":true}
ON-REPLY 4:tuto-2/action: OK
{
  "response":{
    "subscribe":true
  },
  "jtype":"afb-reply",
  "request":{
    "status":"success"
  }
}
```

In an other terminal:

```bash
$ afb-client-demo -H localhost:3333/api?token=toto
tuto-2 login {"user":"jobol","password":"please"}
ON-REPLY 1:tuto-2/login: OK
{
  "jtype":"afb-reply",
  "request":{
    "status":"success",
    "uuid":"a09f55ff-0e89-4f4e-8415-c6e0e7f439be"
  }
}
tuto-2 logout true
ON-REPLY 2:tuto-2/logout: OK
{
  "jtype":"afb-reply",
  "request":{
    "status":"success"
  }
}
```

It produced in the first terminal:

```bash
ON-EVENT tuto-2/login:
{
  "event":"tuto-2\/login",
  "data":"jobol",
  "jtype":"afb-event"
}
ON-EVENT tuto-2/logout:
{
  "event":"tuto-2\/logout",
  "data":"jobol",
  "jtype":"afb-event"
}
```

