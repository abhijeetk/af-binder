Migration to binding V3
=======================

The ***binding*** interface evolved from version 1 to version 2
for the following reasons:

- integration of the security requirements within the bindings
- simplification of the API (after developer feedbacks)
- removal of obscure features, cleanup

The ***binder*** can run ***bindings*** v1, v2 and/or v3 in any combination.  
Thus moving from v1 or v2 to v3 is not enforced at this time. But ...

In the face to face meeting of Karlsruhe it was decided to remove support
of bindings v1 and to deprecate the use of bindings v2.

So at the end, **IT IS HIGHLY NEEDED TO SWITCH TO VERSION 3**

This guide covers the migration of bindings from version 2 to version 3.

The migration from version 1 is not treated here because bindings version 1
are very old and probably does not exist anymore. If needed you can refer
to the old [guide to migrate bindings from v1 to v2](afb-migration-v1-to-v2.md).


Differences between version 2 and version 3
-------------------------------------------

### in v3 all is api

The version 3 introduce the concept of "API" that gather what was called before
the daemon and the service. This is the new concept that predates the 2 others.

The concept of API is intended to allow the definition of multiple APIs
by a same "binding" (a dynamically loaded library).

Because there is potentially several "API", the functions that were without
context in bindings version 2 need now to tell what API is consumer.

To be compatible with version 2, the bindings v3 still have a default hidden
context: the default API named **afbBindingV3root**.

To summarize, the functions of class **daemon** and **service** use the default
hidden API.

It is encouraged to avoid use of functions of class **daemon** and **service**.
You should replace these implicit calls to explicit **api** calls that 
reference **afbBindingV3root**.

Same thing for the logging macros: **AFB_ERROR**, **AFB_WARNING**,
**AFB_NOTICE**, **AFB_INFO**, **AFB_DEBUG** that becomes respectively
**AFB_API_ERROR**, **AFB_API_WARNING**, **AFB_API_NOTICE**, **AFB_API_INFO**,
**AFB_API_DEBUG**.

Example of 2 equivalent writes:

```C
	AFB_NOTICE("send stress event");
        afb_daemon_broadcast_event(stressed_event, NULL);
```

or 

```C
	AFB_API_NOTICE(afbBindingV3root, "send stress event");
        afb_api_broadcast_event(afbBindingV3root, stressed_event, NULL);
```

### the reply mechanism predates success and fail

### subcall has more power

Task list for the migration
---------------------------

This task list is:

1. Use the automatic migration procedure described below
2. Adapt the init and preinit functions
3. Consider to change to use the new reply
4. Consider to change to use the new (sub)call

The remaining chapters explain these task with more details.

Automatic migration!
--------------------

A tiny **sed** script is intended to perform a first pass on the code that
you want to upgrade. It can be down using **curl** and applied using **sed**
as below.

```bash
BASE=https://git.automotivelinux.org/src/app-framework-binder/tree
SED=migration-to-binding-v3.sed
curl -o $SED $BASE/docs/$SED
sed -i -f $SED file1 file2 file3...
```

This automatic action does most of the boring job nut not all the job.
The remaining of this guide explains the missing part.

Adapt the init and preinit functions
------------------------------------

Consider to change to use the new reply
---------------------------------------

Consider to change to use the new (sub)call
-------------------------------------------

