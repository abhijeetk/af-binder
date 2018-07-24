Functions of class **afb_service**
==============================

All the functions of the class **afb_daemon** use the default api.
These are internally aliased to the corresponding legacy function of
class **afb_api**.

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