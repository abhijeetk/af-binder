/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
 * Author "Fulup Ar Foll"
 * Author José Bollo <jose.bollo@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "afb-session.h"
#include "verbose.h"
#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-context.h"
#include "afb-xreq.h"
#include "jobs.h"

#include <afb/afb-req-itf.h>

#define INCR 8		/* CAUTION: must be a power of 2 */

/**
 * Internal description of an api
 */
struct api_desc {
	const char *name;	/**< name of the api */
	struct afb_api api;	/**< handler of the api */
};

/**
 * Data structure for apiset
 */
struct afb_apiset
{
	struct api_desc *apis;		/**< description of apis */
	struct afb_apiset *subset;	/**< subset if any */
	struct afb_api defapi;		/**< default api if any */
	int count;			/**< count of apis in the set */
	int timeout;			/**< the timeout in second for the apiset */
	int refcount;			/**< reference count for freeing resources */
	char name[1];			/**< name of the apiset */
};

/**
 * Search the api of 'name'.
 * @param set the api set
 * @param name the api name to search
 * @return the descriptor if found or NULL otherwise
 */
static const struct api_desc *search(struct afb_apiset *set, const char *name)
{
	int i, c, up, lo;
	const struct api_desc *a;

	/* dichotomic search of the api */
	/* initial slice */
	lo = 0;
	up = set->count;
	for (;;) {
		/* check remaining slice */
		if (lo >= up) {
			/* not found */
			return NULL;
		}
		/* check the mid of the slice */
		i = (lo + up) >> 1;
		a = &set->apis[i];
		c = strcasecmp(a->name, name);
		if (c == 0) {
			/* found */
			return a;
		}
		/* update the slice */
		if (c < 0)
			lo = i + 1;
		else
			up = i;
	}
}

/**
 * Increases the count of references to the apiset and return its address
 * @param set the set whose reference count is to be increased
 * @return the given apiset
 */
struct afb_apiset *afb_apiset_addref(struct afb_apiset *set)
{
	if (set)
		__atomic_add_fetch(&set->refcount, 1, __ATOMIC_RELAXED);
	return set;
}

/**
 * Decreases the count of references to the apiset and frees its
 * resources when no more references exists.
 * @param set the set to unrefrence
 */
void afb_apiset_unref(struct afb_apiset *set)
{
	if (set && !__atomic_sub_fetch(&set->refcount, 1, __ATOMIC_RELAXED)) {
		afb_apiset_unref(set->subset);
		free(set->apis);
		free(set);
	}
}

/**
 * Create an apiset
 * @param name the name of the apiset
 * @param timeout the default timeout in seconds for the apiset
 * @return the created apiset or NULL in case of error
 */
struct afb_apiset *afb_apiset_create(const char *name, int timeout)
{
	struct afb_apiset *set;

	set = malloc((name ? strlen(name) : 0) + sizeof *set);
	if (set) {
		set->apis = malloc(INCR * sizeof *set->apis);
		set->count = 0;
		set->timeout = timeout;
		set->refcount = 1;
		set->subset = NULL;
		set->defapi.itf = NULL;
		if (name)
			strcpy(set->name, name);
		else
			set->name[0] = 0;
	}
	return set;
}

/**
 * the name of the apiset
 * @param set the api set
 * @return the name of the set
 */
const char *afb_apiset_name(struct afb_apiset *set)
{
	return set->name;
}

/**
 * Get the API timeout of the set
 * @param set the api set
 * @return the timeout in seconds
 */
int afb_apiset_timeout_get(struct afb_apiset *set)
{
	return set->timeout;
}

/**
 * Set the API timeout of the set
 * @param set the api set
 * @param to the timeout in seconds
 */
void afb_apiset_timeout_set(struct afb_apiset *set, int to)
{
	set->timeout = to;
}

/**
 * Get the subset of the set
 * @param set the api set
 * @return the subset of set
 */
struct afb_apiset *afb_apiset_subset_get(struct afb_apiset *set)
{
	return set->subset;
}

/**
 * Set the subset of the set
 * @param set the api set
 * @param subset the subset to set
 */
void afb_apiset_subset_set(struct afb_apiset *set, struct afb_apiset *subset)
{
	struct afb_apiset *tmp;
	if (subset == set) {
		/* avoid infinite loop */
		subset = NULL;
	}
	tmp = set->subset;
	set->subset = afb_apiset_addref(subset);
	afb_apiset_unref(tmp);
}

/**
 * Check if the apiset has a default api
 * @param set the api set
 * @return 1 if the set has a default api or 0 otherwise
 */
int afb_apiset_default_api_exist(struct afb_apiset *set)
{
	return !!set->defapi.itf;
}

/**
 * Get the default api of the api set.
 * @param set the api set
 * @param api where to store the default api
 * @return 0 in case of success or -1 when no default api is set
 */
int afb_apiset_default_api_get(struct afb_apiset *set, struct afb_api *api)
{
	if (set->defapi.itf) {
		*api = set->defapi;
		return 0;
	}
	errno = ENOENT;
	return -1;
}

/**
 * Set the default api of the api set
 * @param set the api set
 * @param subset the subset to set
 */
void afb_apiset_default_api_set(struct afb_apiset *set, struct afb_api api)
{
	set->defapi = api;
}

/**
 * Set the default api of the api set
 * @param set the api set
 */
void afb_apiset_default_api_drop(struct afb_apiset *set)
{
	set->defapi.itf = NULL;
}

/**
 * Adds the api of 'name' described by 'api'.
 * @param set the api set
 * @param name the name of the api to add (have to survive, not copied!)
 * @param api the api
 * @returns 0 in case of success or -1 in case
 * of error with errno set:
 *   - EEXIST if name already registered
 *   - ENOMEM when out of memory
 */
int afb_apiset_add(struct afb_apiset *set, const char *name, struct afb_api api)
{
	struct api_desc *apis;
	int i, c;

	/* check previously existing plugin */
	for (i = 0 ; i < set->count ; i++) {
		c = strcasecmp(set->apis[i].name, name);
		if (c == 0) {
			ERROR("api of name %s already exists", name);
			errno = EEXIST;
			goto error;
		}
		if (c > 0)
			break;
	}

	/* allocates enough memory */
	c = (set->count + INCR) & ~(INCR - 1);
	apis = realloc(set->apis, ((unsigned)c) * sizeof * apis);
	if (apis == NULL) {
		ERROR("out of memory");
		errno = ENOMEM;
		goto error;
	}
	set->apis = apis;

	/* copy higher part of the array */
	apis += i;
	if (i != set->count)
		memmove(apis + 1, apis, ((unsigned)(set->count - i)) * sizeof *apis);

	/* record the plugin */
	apis->api = api;
	apis->name = name;
	set->count++;

	NOTICE("API %s added", name);

	return 0;

error:
	return -1;
}

/**
 * Delete from the 'set' the api of 'name'.
 * @param set the set to be changed
 * @param name the name of the API to remove
 * @return 0 in case of success or -1 in case where the API doesn't exist.
 */
int afb_apiset_del(struct afb_apiset *set, const char *name)
{
	int i, c;

	/* search the api */
	for (i = 0 ; i < set->count ; i++) {
		c = strcasecmp(set->apis[i].name, name);
		if (c == 0) {
			set->count--;
			while(i < set->count) {
				set->apis[i] = set->apis[i + 1];
				i++;
			}
			return 0;
		}
		if (c > 0)
			break;
	}
	errno = ENOENT;
	return -1;
}

/**
 * Get from the 'set' the API of 'name' in 'api'
 * @param set the set of API
 * @param name the name of the API to get
 * @param api the structure where to store data about the API of name
 * @return 0 in case of success or -1 in case of error
 */
int afb_apiset_lookup(struct afb_apiset *set, const char *name, struct afb_api *api)
{
	const struct api_desc *i;

	i = search(set, name);
	if (i) {
		*api = i->api;
		return 0;
	}

	errno = ENOENT;
	return -1;
}

/**
 * Get from the 'set' the API of 'name' in 'api' with fallback to subset or default api
 * @param set the set of API
 * @param name the name of the API to get
 * @param api the structure where to store data about the API of name
 * @return 0 in case of success or -1 in case of error
 */
int afb_apiset_get(struct afb_apiset *set, const char *name, struct afb_api *api)
{
	const struct api_desc *i;

	i = search(set, name);
	if (i) {
		*api = i->api;
		return 0;
	}

	if (set->subset && 0 == afb_apiset_get(set->subset, name, api))
		return 0;

	if (set->defapi.itf) {
		*api = set->defapi;
		return 0;
	}

	errno = ENOENT;
	return -1;
}

/**
 * Starts a service by its 'api' name.
 * @param set the api set
 * @param name name of the service to start
 * @param share_session if true start the servic"e in a shared session
 *                      if false start it in its own session
 * @param onneed if true start the service if possible, if false the api
 *               must be a service
 * @return a positive number on success
 */
int afb_apiset_start_service(struct afb_apiset *set, const char *name, int share_session, int onneed)
{
	const struct api_desc *a;

	a = search(set, name);
	if (!a) {
		ERROR("can't find service %s", name);
		errno = ENOENT;
		return -1;
	}

	if (a->api.itf->service_start)
		return a->api.itf->service_start(a->api.closure, share_session, onneed, set);

	if (onneed)
		return 0;

	/* already started: it is an error */
	ERROR("The api %s is not a startable service", name);
	errno = EINVAL;
	return -1;
}

/**
 * Starts all possible services but stops at first error.
 * @param set the api set
 * @param share_session if true start the servic"e in a shared session
 *                      if false start it in its own session
 * @return 0 on success or a negative number when an error is found
 */
int afb_apiset_start_all_services(struct afb_apiset *set, int share_session)
{
	int rc;
	const struct api_desc *i, *e;

	i = set->apis;
	e = &set->apis[set->count];
	while (i != e) {
		if (i->api.itf->service_start) {
			rc = i->api.itf->service_start(i->api.closure, share_session, 1, set);
			if (rc < 0)
				return rc;
		}
		i++;
	}
	return 0;
}

/**
 * Ask to update the hook flags of the 'api'
 * @param set the api set
 * @param name the api to update (NULL updates all)
 */
void afb_apiset_update_hooks(struct afb_apiset *set, const char *name)
{
	const struct api_desc *i, *e;

	if (!name) {
		i = set->apis;
		e = &set->apis[set->count];
	} else {
		i = search(set, name);
		e = &i[!!i];
	}
	while (i != e) {
		if (i->api.itf->update_hooks)
			i->api.itf->update_hooks(i->api.closure);
		i++;
	}
}

/**
 * Set the verbosity level of the 'api'
 * @param set the api set
 * @param name the api to set (NULL set all)
 */
void afb_apiset_set_verbosity(struct afb_apiset *set, const char *name, int level)
{
	const struct api_desc *i, *e;

	if (!name) {
		i = set->apis;
		e = &set->apis[set->count];
	} else {
		i = search(set, name);
		e = &i[!!i];
	}
	while (i != e) {
		if (i->api.itf->set_verbosity)
			i->api.itf->set_verbosity(i->api.closure, level);
		i++;
	}
}

/**
 * Set the verbosity level of the 'api'
 * @param set the api set
 * @param name the api to set (NULL set all)
 */
int afb_apiset_get_verbosity(struct afb_apiset *set, const char *name)
{
	const struct api_desc *i;

	i = name ? search(set, name) : NULL;
	if (!i) {
		errno = ENOENT;
		return -1;
	}
	if (!i->api.itf->get_verbosity)
		return verbosity;

	return i->api.itf->get_verbosity(i->api.closure);
}

/**
 * Get the list of api names
 * @param set the api set
 * @return a NULL terminated array of api names. Must be freed.
 */
const char **afb_apiset_get_names(struct afb_apiset *set)
{
	size_t size;
	char *dest;
	const char **names;
	int i;

	size = set->count * (1 + sizeof(*names)) + sizeof(*names);
	for (i = 0 ; i < set->count ; i++)
		size += strlen(set->apis[i].name);

	names = malloc(size);
	if (!names)
		errno = ENOMEM;
	else {
		dest = (void*)&names[set->count+1];
		for (i = 0 ; i < set->count ; i++) {
			names[i] = dest;
			dest = stpcpy(dest, set->apis[i].name) + 1;
		}
		names[i] = NULL;
	}
	return names;
}

/**
 * Enumerate the api names to a callback.
 * @param set the api set
 * @param callback the function to call for each name
 * @param closure the closure for the callback
 */
void afb_apiset_enum(struct afb_apiset *set, void (*callback)(struct afb_apiset *set, const char *name, void *closure), void *closure)
{
	int i;

	for (i = 0 ; i < set->count ; i++)
		callback(set, set->apis[i].name, closure);
}

