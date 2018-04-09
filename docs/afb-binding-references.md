# Binding Reference

## Structure for declaring binding

### afb_binding_t

The main structure, of type **afb_binding_t**, for describing the binding
must be exported under the name **afbBindingExport**.

This structure is defined as below.

```C
typedef struct afb_binding_v3 afb_binding_t;
```

Where:

```C
/**
 * Description of the bindings of type version 3
 */
struct afb_binding_v3
{
	/** api name for the binding, can't be NULL */
	const char *api;

	/** textual specification of the binding, can be NULL */
	const char *specification;

	/** some info about the api, can be NULL */
	const char *info;

	/** array of descriptions of verbs terminated by a NULL name, can be NULL */
	const struct afb_verb_v3 *verbs;

	/** callback at load of the binding */
	int (*preinit)(struct afb_api_x3 *api);

	/** callback for starting the service */
	int (*init)(struct afb_api_x3 *api);

	/** callback for handling events */
	void (*onevent)(struct afb_api_x3 *api, const char *event, struct json_object *object);

	/** userdata for afb_api_x3 */
	void *userdata;

	/** space separated list of provided class(es) */
	const char *provide_class;

	/** space separated list of required class(es) */
	const char *require_class;

	/** space separated list of required API(es) */
	const char *require_api;

	/** avoids concurrent requests to verbs */
	unsigned noconcurrency: 1;
};
```

### struct afb_verb_t

Each verb is described with a structure of type **afb_verb_t**
defined below:

```C
typedef struct afb_verb_v3 afb_verb_t;
```

```C
/**
 * Description of one verb as provided for binding API version 3
 */
struct afb_verb_v3
{
	/** name of the verb, NULL only at end of the array */
	const char *verb;

	/** callback function implementing the verb */
	void (*callback)(afb_req_t_x2 *req);

	/** required authorization, can be NULL */
	const struct afb_auth *auth;

	/** some info about the verb, can be NULL */
	const char *info;

	/**< data for the verb callback */
	void *vcbdata;

	/** authorization and session requirements of the verb */
	uint16_t session;

	/** is the verb glob name */
	uint16_t glob: 1;
};
```

The **session** flags is one of the constant defined below:

- AFB_SESSION_NONE : no flag, synonym to 0
- AFB_SESSION_LOA_0 : Requires the LOA to be 0 or more, synonym to 0 or AFB_SESSION_NONE
- AFB_SESSION_LOA_1 : Requires the LOA to be 1 or more
- AFB_SESSION_LOA_2 : Requires the LOA to be 2 or more
- AFB_SESSION_LOA_3 : Requires the LOA to be 3 or more
- AFB_SESSION_CHECK : Requires the token to be set and valid
- AFB_SESSION_REFRESH : Implies a token refresh
- AFB_SESSION_CLOSE : Implies closing the session after request processed

The LOA (Level Of Assurance) is set, by binding api, using the function **afb_req_session_set_LOA**.

The session can be closed, by binding api, using the function **afb_req_session_close**.

### afb_auth_t and afb_auth_type_t

The structure **afb_auth_t** is used within verb description to
set security requirements.  
The interpretation of the structure depends on the value of the field **type**.

```C
typedef struct afb_auth afb_auth_t;

/**
 * Definition of an authorization entry
 */
struct afb_auth
{
	/** type of entry @see afb_auth_type */
	enum afb_auth_type type;
	
	union {
		/** text when @ref type == @ref afb_auth_Permission */
		const char *text;
		
		/** level of assurancy when @ref type ==  @ref afb_auth_LOA */
		unsigned loa;
		
		/** first child when @ref type in { @ref afb_auth_Or, @ref afb_auth_And, @ref afb_auth_Not } */
		const struct afb_auth *first;
	};
	
	/** second child when @ref type in { @ref afb_auth_Or, @ref afb_auth_And } */
	const struct afb_auth *next;
};

```

The possible values for **type** is defined here:

```C
typedef enum afb_auth_type afb_auth_type_t;

/**
 * Enumeration  for authority (Session/Token/Assurance) definitions.
 *
 * @see afb_auth
 */
enum afb_auth_type
{
	/** never authorized, no data */
	afb_auth_No = 0,

	/** authorized if token valid, no data */
	afb_auth_Token,

	/** authorized if LOA greater than data 'loa' */
	afb_auth_LOA,

	/** authorized if permission 'text' is granted */
	afb_auth_Permission,

	/** authorized if 'first' or 'next' is authorized */
	afb_auth_Or,

	/** authorized if 'first' and 'next' are authorized */
	afb_auth_And,

	/** authorized if 'first' is not authorized */
	afb_auth_Not,

	/** always authorized, no data */
	afb_auth_Yes
};
```

Example:

```C
static const afb_auth_t myauth[] = {
    { .type = afb_auth_Permission, .text = "urn:AGL:permission:me:public:set" },
    { .type = afb_auth_Permission, .text = "urn:AGL:permission:me:public:get" },
    { .type = afb_auth_Or, .first = &myauth[1], .next = &myauth[0] }
};
```

## Functions of class afb_daemon

The 3 following functions are linked to libsystemd.  
They allow use of **sd_event** features and access
to **sd_bus** features.

```C
/*
 * Retrieves the common systemd's event loop of AFB 
 * 
 */
struct sd_event *afb_daemon_get_event_loop();

/*
 * Retrieves the common systemd's user/session d-bus of AFB if active
 */
struct sd_bus *afb_daemon_get_user_bus();

/*
 * Retrieves the common systemd's system d-bus of AFB if active or NULL
 */
struct sd_bus *afb_daemon_get_system_bus();
```

The 2 following functions are linked to event management.  
Broadcasting an event send it to any possible listener.

```C
/*
 * Broadcasts widely the event of 'name' with the data 'object'.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Calling this function is only forbidden during preinit.
 *
 * Returns the count of clients that received the event.
 */
int afb_daemon_broadcast_event(const char *name, struct json_object *object);

/*
 * Creates an event of 'name' and returns it.
 *
 * Calling this function is only forbidden during preinit.
 *
 * See afb_event_is_valid to check if there is an error.
 */
afb_event_t afb_daemon_make_event(const char *name);
```

The following function is used by logging macros and should normally
not be used.  
Instead, you should use the macros:

- **AFB\_ERROR**
- **AFB\_WARNING**
- **AFB\_NOTICE**
- **AFB\_INFO**
- **AFB\_DEBUG**

```C
/*
 * Send a message described by 'fmt' and following parameters
 * to the journal for the verbosity 'level'.
 *
 * 'file', 'line' and 'func' are indicators of position of the code in source files
 * (see macros __FILE__, __LINE__ and __func__).
 *
 * 'level' is defined by syslog standard:
 *      EMERGENCY         0        System is unusable
 *      ALERT             1        Action must be taken immediately
 *      CRITICAL          2        Critical conditions
 *      ERROR             3        Error conditions
 *      WARNING           4        Warning conditions
 *      NOTICE            5        Normal but significant condition
 *      INFO              6        Informational
 *      DEBUG             7        Debug-level messages
 */
void afb_daemon_verbose(int level, const char *file, int line, const char * func, const char *fmt, ...);
```

The 2 following functions MUST be used to access data of the bindings.

```C
/*
 * Get the root directory file descriptor. This file descriptor can
 * be used with functions 'openat', 'fstatat', ...
 */
int afb_daemon_rootdir_get_fd();

/*
 * Opens 'filename' within the root directory with 'flags' (see function openat)
 * using the 'locale' definition (example: "jp,en-US") that can be NULL.
 * Returns the file descriptor or -1 in case of error.
 */
int afb_daemon_rootdir_open_locale(const char *filename, int flags, const char *locale);
```

The following function is used to queue jobs.

```C
/*
 * Queue the job defined by 'callback' and 'argument' for being executed asynchronously
 * in this thread (later) or in an other thread.
 * If 'group' is not NUL, the jobs queued with a same value (as the pointer value 'group')
 * are executed in sequence in the order of there submission.
 * If 'timeout' is not 0, it represent the maximum execution time for the job in seconds.
 * At first, the job is called with 0 as signum and the given argument.
 * The job is executed with the monitoring of its time and some signals like SIGSEGV and
 * SIGFPE. When a such signal is catched, the job is terminated and re-executed but with
 * signum being the signal number (SIGALRM when timeout expired).
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int afb_daemon_queue_job(void (*callback)(int signum, void *arg), void *argument, void *group, int timeout)
```

The following function must be used when a binding depends on other
bindings at its initialization.

```C
/*
 * Tells that it requires the API of "name" to exist
 * and if 'initialized' is not null to be initialized.
 * Calling this function is only allowed within init.
 * Returns 0 in case of success or -1 in case of error.
 */
int afb_daemon_require_api(const char *name, int initialized)
```

This function allows to give a different name to the binding.
It can be called during pre-init.

```C
/*
 * Set the name of the API to 'name'.
 * Calling this function is only allowed within preinit.
 * Returns 0 in case of success or -1 in case of error.
 */
int afb_daemon_rename_api(const char *name);
```

## Functions of class afb_service

The following functions allow services to call verbs of other
bindings for themselves.

```C
/**
 * Calls the 'verb' of the 'api' with the arguments 'args' and 'verb' in the name of the binding.
 * The result of the call is delivered to the 'callback' function with the 'callback_closure'.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * The 'callback' receives 3 arguments:
 *  1. 'closure' the user defined closure pointer 'callback_closure',
 *  2. 'status' a status being 0 on success or negative when an error occured,
 *  2. 'result' the resulting data as a JSON object.
 *
 * @param api      The api name of the method to call
 * @param verb     The verb name of the method to call
 * @param args     The arguments to pass to the method
 * @param callback The to call on completion
 * @param callback_closure The closure to pass to the callback
 *
 * @see also 'afb_req_subcall'
 */
void afb_service_call(
    const char *api,
    const char *verb,
    struct json_object *args,
    void (*callback)(void*closure, int status, struct json_object *result),
    void *callback_closure);

/**
 * Calls the 'verb' of the 'api' with the arguments 'args' and 'verb' in the name of the binding.
 * 'result' will receive the response.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * @param api      The api name of the method to call
 * @param verb     The verb name of the method to call
 * @param args     The arguments to pass to the method
 * @param result   Where to store the result - should call json_object_put on it -
 *
 * @returns 0 in case of success or a negative value in case of error.
 *
 * @see also 'afb_req_subcall'
 */
int afb_service_call_sync(
    const char *api,
    const char *verb,
    struct json_object *args,
    struct json_object **result);
```

## Functions of class afb_event

This function checks whether the event is valid.  
It must be used when creating events.

```C
/*
 * Checks wether the 'event' is valid or not.
 *
 * Returns 0 if not valid or 1 if valid.
 */
int afb_event_is_valid(afb_event_t event);
```

The two following functions are used to broadcast or push
event with its data.

```C
/*
 * Broadcasts widely the 'event' with the data 'object'.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
int afb_event_broadcast(afb_event_t event, struct json_object *object);

/*
 * Pushes the 'event' with the data 'object' to its observers.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
int afb_event_push(afb_event_t event, struct json_object *object);
```

The following function remove one reference to the event.

```C
/*
 * Decrease the reference count of the event.
 * After calling this function, the event
 * MUST NOT BE USED ANYMORE.
 */
void afb_event_unref(afb_event_t event);
```

The following function add one reference to the event.

```C
/*
 * Decrease the reference count of the event.
 * After calling this function, the event
 * MUST NOT BE USED ANYMORE.
 */
void afb_event_unref(afb_event_t event);
```

This function allows to retrieve the exact name of the event.

```C
/*
 * Gets the name associated to the 'event'.
 */
const char *afb_event_name(afb_event_t event);
```

## Functions of class afb_req

This function checks the validity of the **req**.

```C
/*
 * Checks wether the request 'req' is valid or not.
 *
 * Returns 0 if not valid or 1 if valid.
 */
int afb_req_is_valid(afb_req_t req);
```

The following functions retrieves parameters of the request.

```C
/*
 * Gets from the request 'req' the argument of 'name'.
 * Returns a PLAIN structure of type 'struct afb_arg'.
 * When the argument of 'name' is not found, all fields of result are set to NULL.
 * When the argument of 'name' is found, the fields are filled,
 * in particular, the field 'result.name' is set to 'name'.
 *
 * There is a special name value: the empty string.
 * The argument of name "" is defined only if the request was made using
 * an HTTP POST of Content-Type "application/json". In that case, the
 * argument of name "" receives the value of the body of the HTTP request.
 */
afb_arg_t afb_req_get(afb_req_t req, const char *name);

/*
 * Gets from the request 'req' the string value of the argument of 'name'.
 * Returns NULL if when there is no argument of 'name'.
 * Returns the value of the argument of 'name' otherwise.
 *
 * Shortcut for: afb_req_get(req, name).value
 */
const char *afb_req_value(afb_req_t req, const char *name);

/*
 * Gets from the request 'req' the path for file attached to the argument of 'name'.
 * Returns NULL if when there is no argument of 'name' or when there is no file.
 * Returns the path of the argument of 'name' otherwise.
 *
 * Shortcut for: afb_req_get(req, name).path
 */
const char *afb_req_path(afb_req_t req, const char *name);

/*
 * Gets from the request 'req' the json object hashing the arguments.
 * The returned object must not be released using 'json_object_put'.
 */
struct json_object *afb_req_json(afb_req_t req);
```

The following functions emit the reply to the request.

```C
/*
 * Sends a reply of kind success to the request 'req'.
 * The status of the reply is automatically set to "success".
 * Its send the object 'obj' (can be NULL) with an
 * informationnal comment 'info (can also be NULL).
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
void afb_req_success(afb_req_t req, struct json_object *obj, const char *info);

/*
 * Same as 'afb_req_success' but the 'info' is a formatting
 * string followed by arguments.
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
void afb_req_success_f(afb_req_t req, struct json_object *obj, const char *info, ...);

/*
 * Same as 'afb_req_success_f' but the arguments to the format 'info'
 * are given as a variable argument list instance.
 *
 * For convenience, the function calls 'json_object_put' for 'obj'.
 * Thus, in the case where 'obj' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 */
void afb_req_success_v(afb_req_t req, struct json_object *obj, const char *info, va_list args);

/*
 * Sends a reply of kind failure to the request 'req'.
 * The status of the reply is set to 'status' and an
 * informational comment 'info' (can also be NULL) can be added.
 *
 * Note that calling afb_req_fail("success", info) is equivalent
 * to call afb_req_success(NULL, info). Thus even if possible it
 * is strongly recommended to NEVER use "success" for status.
 */
void afb_req_fail(afb_req_t req, const char *status, const char *info);

/*
 * Same as 'afb_req_fail' but the 'info' is a formatting
 * string followed by arguments.
 */
void afb_req_fail_f(afb_req_t req, const char *status, const char *info, ...);

/*
 * Same as 'afb_req_fail_f' but the arguments to the format 'info'
 * are given as a variable argument list instance.
 */
void afb_req_fail_v(afb_req_t req, const char *status, const char *info, va_list args);
```

The following functions handle the session data.

```C
/*
 * Gets the pointer stored by the binding for the session of 'req'.
 * When the binding has not yet recorded a pointer, NULL is returned.
 */
void *afb_req_context_get(afb_req_t req);

/*
 * Stores for the binding the pointer 'context' to the session of 'req'.
 * The function 'free_context' will be called when the session is closed
 * or if binding stores an other pointer.
 */
void afb_req_context_set(afb_req_t req, void *context, void (*free_context)(void*));

/*
 * Gets the pointer stored by the binding for the session of 'req'.
 * If the stored pointer is NULL, indicating that no pointer was
 * already stored, afb_req_context creates a new context by calling
 * the function 'create_context' and stores it with the freeing function
 * 'free_context'.
 */
void *afb_req_context(afb_req_t req, void *(*create_context)(), void (*free_context)(void*));

/*
 * Frees the pointer stored by the binding for the session of 'req'
 * and sets it to NULL.
 *
 * Shortcut for: afb_req_context_set(req, NULL, NULL)
 */
void afb_req_context_clear(afb_req_t req);

/*
 * Closes the session associated with 'req'
 * and delete all associated contexts.
 */
void afb_req_session_close(afb_req_t req);

/*
 * Sets the level of assurance of the session of 'req'
 * to 'level'. The effect of this function is subject of
 * security policies.
 * Returns 1 on success or 0 if failed.
 */
int afb_req_session_set_LOA(afb_req_t req, unsigned level);
```

The 4 following functions must be used for asynchronous handling requests.

```C
/*
 * Adds one to the count of references of 'req'.
 * This function MUST be called by asynchronous implementations
 * of verbs if no reply was sent before returning.
 */
void afb_req_addref(afb_req_t req);

/*
 * Substracts one to the count of references of 'req'.
 * This function MUST be called by asynchronous implementations
 * of verbs after sending the asynchronous reply.
 */
void afb_req_unref(afb_req_t req);

/*
 * Stores 'req' on heap for asynchronous use.
 * Returns a handler to the stored 'req' or NULL on memory depletion.
 * The count of reference to 'req' is incremented on success
 * (see afb_req_addref).
 */
struct afb_stored_req *afb_req_store(afb_req_t req);

/*
 * Retrieves the afb_req stored at 'sreq'.
 * Returns the stored request.
 * The count of reference is UNCHANGED, thus, the
 * function 'afb_req_unref' should be called on the result
 * after that the asynchronous reply if sent.
 */
afb_req_t afb_req_unstore(struct afb_stored_req *sreq);
```

The two following functions are used to associate client with events
(subscription).

```C
/*
 * Establishes for the client link identified by 'req' a subscription
 * to the 'event'.
 * Returns 0 in case of successful subscription or -1 in case of error.
 */
int afb_req_subscribe(afb_req_t req, afb_event_t event);

/*
 * Revokes the subscription established to the 'event' for the client
 * link identified by 'req'.
 * Returns 0 in case of successful subscription or -1 in case of error.
 */
int afb_req_unsubscribe(afb_req_t req, afb_event_t event);
```

The following functions must be used to make request in the name of the
client (with its permissions).

```C
/*
 * Makes a call to the method of name 'api' / 'verb' with the object 'args'.
 * This call is made in the context of the request 'req'.
 * On completion, the function 'callback' is invoked with the
 * 'closure' given at call and two other parameters: 'iserror' and 'result'.
 * 'status' is 0 on success or negative when on an error reply.
 * 'result' is the json object of the reply, you must not call json_object_put
 * on the result.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * See also:
 *  - 'afb_req_subcall_req' that is convenient to keep request alive automatically.
 *  - 'afb_req_subcall_sync' the synchronous version
 */
void afb_req_subcall(
                afb_req_t req,
                const char *api,
                const char *verb,
                struct json_object *args,
                void (*callback)(void *closure, int status, struct json_object *result),
                void *closure);

/*
 * Makes a call to the method of name 'api' / 'verb' with the object 'args'.
 * This call is made in the context of the request 'req'.
 * On completion, the function 'callback' is invoked with the
 * original request 'req', the 'closure' given at call and two
 * other parameters: 'iserror' and 'result'.
 * 'status' is 0 on success or negative when on an error reply.
 * 'result' is the json object of the reply, you must not call json_object_put
 * on the result.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * See also:
 *  - 'afb_req_subcall' that doesn't keep request alive automatically.
 *  - 'afb_req_subcall_sync' the synchronous version
 */
static inline void afb_req_subcall_req(afb_req_t req, const char *api, const char *verb, struct json_object *args, void (*callback)(void *closure, int iserror, struct json_object *result, afb_req_t req), void *closure)
{
	req.itf->subcall_req(req.closure, api, verb, args, callback, closure);
}

/*
 * Makes a call to the method of name 'api' / 'verb' with the object 'args'.
 * This call is made in the context of the request 'req'.
 * This call is synchronous, it waits untill completion of the request.
 * It returns 0 on success or a negative value on error answer.
 * The object pointed by 'result' is filled and must be released by the caller
 * after its use by calling 'json_object_put'.
 *
 * For convenience, the function calls 'json_object_put' for 'args'.
 * Thus, in the case where 'args' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * See also:
 *  - 'afb_req_subcall_req' that is convenient to keep request alive automatically.
 *  - 'afb_req_subcall' that doesn't keep request alive automatically.
 */
int afb_req_subcall_sync(
                afb_req_t req,
                const char *api,
                const char *verb,
                struct json_object *args,
                struct json_object **result);
```

The following function is used by logging macros and should normally
not be used.  
Instead, you should use the macros:

- **AFB_REQ_ERROR**
- **AFB_REQ_WARNING**
- **AFB_REQ_NOTICE**
- **AFB_REQ_INFO**
- **AFB_REQ_DEBUG**

```C
/*
 * Send associated to 'req' a message described by 'fmt' and following parameters
 * to the journal for the verbosity 'level'.
 *
 * 'file', 'line' and 'func' are indicators of position of the code in source files
 * (see macros __FILE__, __LINE__ and __func__).
 *
 * 'level' is defined by syslog standard:
 *      EMERGENCY         0        System is unusable
 *      ALERT             1        Action must be taken immediately
 *      CRITICAL          2        Critical conditions
 *      ERROR             3        Error conditions
 *      WARNING           4        Warning conditions
 *      NOTICE            5        Normal but significant condition
 *      INFO              6        Informational
 *      DEBUG             7        Debug-level messages
 */
void afb_req_verbose(afb_req_t req, int level, const char *file, int line, const char * func, const char *fmt, ...);
```

The functions below allow a binding involved in the platform security
to explicitely check a permission of a client or to get the calling
application identity.

```C
/*
 * Check whether the 'permission' is granted or not to the client
 * identified by 'req'.
 *
 * Returns 1 if the permission is granted or 0 otherwise.
 */
int afb_req_has_permission(afb_req_t req, const char *permission);

/*
 * Get the application identifier of the client application for the
 * request 'req'.
 *
 * Returns the application identifier or NULL when the application
 * can not be identified.
 *
 * The returned value if not NULL must be freed by the caller
 */
char *afb_req_get_application_id(afb_req_t req);

/*
 * Get the user identifier (UID) of the client application for the
 * request 'req'.
 *
 * Returns -1 when the application can not be identified.
 */
int afb_req_get_uid(afb_req_t req);
```

## Logging macros

The following macros must be used for logging:

```C
AFB_ERROR(fmt,...)
AFB_WARNING(fmt,...)
AFB_NOTICE(fmt,...)
AFB_INFO(fmt,...)
AFB_DEBUG(fmt,...)
```

The following macros can be used for logging in the context
of a request **req** of type **afb_req**:

```C
AFB_REQ_ERROR(req,fmt,...)
AFB_REQ_WARNING(req,fmt,...)
AFB_REQ_NOTICE(req,fmt,...)
AFB_REQ_INFO(req,fmt,...)
AFB_REQ_DEBUG(req,fmt,...)
```

By default, the logging macros add file, line and function
indication.
