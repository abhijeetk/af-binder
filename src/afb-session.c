/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
 * Author "Fulup Ar Foll"
 * Author: José Bollo <jose.bollo@iot.bzh>
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
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>
#include <assert.h>
#include <errno.h>

#include <json-c/json.h>

#include "afb-session.h"
#include "verbose.h"

#define HEADCOUNT    16
#define COOKEYCOUNT  8
#define COOKEYMASK   (COOKEYCOUNT - 1)

#define NOW (time(NULL))

struct cookie
{
	struct cookie *next;
	const void *key;
	void *value;
	void (*freecb)(void*);
};

struct afb_session
{
	struct afb_session *next; /* link to the next */
	unsigned refcount;
	int timeout;
	time_t expiration;    // expiration time of the token
	pthread_mutex_t mutex;
	char idx;
	char uuid[37];        // long term authentication of remote client
	char token[37];       // short term authentication of remote client
	struct cookie *cookies[COOKEYCOUNT];
};

// Session UUID are store in a simple array [for 10 sessions this should be enough]
static struct {
	pthread_mutex_t mutex;          // declare a mutex to protect hash table
	struct afb_session *heads[HEADCOUNT]; // sessions
	int count;                      // current number of sessions
	int max;
	int timeout;
	char initok[37];
} sessions;

/* generate a uuid */
static void new_uuid(char uuid[37])
{
	uuid_t newuuid;
	uuid_generate(newuuid);
	uuid_unparse_lower(newuuid, uuid);
}

static inline void lock(struct afb_session *session)
{
	pthread_mutex_lock(&session->mutex);
}

static inline void unlock(struct afb_session *session)
{
	pthread_mutex_unlock(&session->mutex);
}

// Free context [XXXX Should be protected again memory abort XXXX]
static void remove_all_cookies(struct afb_session *session)
{
	int idx;
	struct cookie *cookie, *next;

	// free cookies
	for (idx = 0 ; idx < COOKEYCOUNT ; idx++) {
		cookie = session->cookies[idx];
		session->cookies[idx] = NULL;
		while (cookie != NULL) {
			next = cookie->next;
			if (cookie->freecb != NULL)
				cookie->freecb(cookie->value);
			free(cookie);
			cookie = next;
		}
	}
}

/* tiny hash function inspired from pearson */
static int pearson4(const char *text)
{
	static uint8_t T[16] = {
		 4,  1,  6,  0,  9, 14, 11,  5,
		 2,  3, 12, 15, 10,  7,  8, 13
	};
	uint8_t r, c;

	for (r = 0; (c = (uint8_t)*text) ; text++) {
		r = T[r ^ (15 & c)];
		r = T[r ^ (c >> 4)];
	}
	return r; // % HEADCOUNT;
}

// Create a new store in RAM, not that is too small it will be automatically extended
void afb_session_init (int max_session_count, int timeout, const char *initok)
{
	pthread_mutex_init(&sessions.mutex, NULL);
	sessions.max = max_session_count;
	sessions.timeout = timeout;
	if (initok == NULL)
		/* without token, a secret is made to forbid creation of sessions */
		new_uuid(sessions.initok);
	else if (strlen(initok) < sizeof sessions.initok)
		strcpy(sessions.initok, initok);
	else {
		ERROR("initial token '%s' too long (max length %d)", initok, ((int)(sizeof sessions.initok)) - 1);
		exit(1);
	}
}

const char *afb_session_initial_token()
{
	return sessions.initok;
}

static struct afb_session *search (const char* uuid, int idx)
{
	struct afb_session *session;

	session = sessions.heads[idx];
	while (session && strcmp(uuid, session->uuid))
		session = session->next;

	return session;
}

static void destroy (struct afb_session *session)
{
	struct afb_session **prv;

	assert (session != NULL);

	remove_all_cookies(session);
	pthread_mutex_lock(&sessions.mutex);
	prv = &sessions.heads[(int)session->idx];
	while (*prv)
		if (*prv != session)
			prv = &((*prv)->next);
		else {
			*prv = session->next;
			sessions.count--;
			pthread_mutex_destroy(&session->mutex);
			free(session);
			break;
		}
	pthread_mutex_unlock(&sessions.mutex);
}

// Check if context timeout or not
static int is_expired (struct afb_session *ctx, time_t now)
{
	assert (ctx != NULL);
	return ctx->expiration < now;
}

// Check if context is active or not
static int is_active (struct afb_session *ctx, time_t now)
{
	assert (ctx != NULL);
	return ctx->uuid[0] != 0 && ctx->expiration >= now;
}

// Loop on every entry and remove old context sessions.hash
static time_t cleanup ()
{
	struct afb_session *session, *next;
	int idx;
	time_t now;

	// Loop on Sessions Table and remove anything that is older than timeout
	now = NOW;
	for (idx = 0 ; idx < HEADCOUNT; idx++) {
		session = sessions.heads[idx];
		while (session) {
			next = session->next;
			if (is_expired(session, now))
				afb_session_close(session);
			session = next;
		}
	}
	return now;
}

static struct afb_session *add_session (const char *uuid, int timeout, time_t now, int idx)
{
	struct afb_session *session;

	if (!AFB_SESSION_TIMEOUT_IS_VALID(timeout)
	 || (uuid && strlen(uuid) >= sizeof session->uuid)) {
		errno = EINVAL;
		return NULL;
	}

	if (sessions.count >= sessions.max) {
		errno = EBUSY;
		return NULL;
	}

	/* allocates a new one */
	session = calloc(1, sizeof *session);
	if (session == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	pthread_mutex_init(&session->mutex, NULL);
	session->refcount = 1;
	strcpy(session->uuid, uuid);
	strcpy(session->token, sessions.initok);

	/* init timeout */
	if (timeout == AFB_SESSION_TIMEOUT_DEFAULT)
		timeout = sessions.timeout;
	session->timeout = timeout;
	session->expiration = now + timeout;
	if (timeout == AFB_SESSION_TIMEOUT_INFINITE || session->expiration < 0) {
		session->expiration = (time_t)(~(time_t)0);
		if (session->expiration < 0)
			session->expiration = (time_t)(((unsigned long long)session->expiration) >> 1);
	}

	session->idx = (char)idx;
	session->next = sessions.heads[idx];
	sessions.heads[idx] = session;
	sessions.count++;

	return session;
}

static struct afb_session *new_session (int timeout, time_t now)
{
	int idx;
	char uuid[37];

	do {
		new_uuid(uuid);
		idx = pearson4(uuid);
	} while(search(uuid, idx));
	return add_session(uuid, timeout, now, idx);
}

/* Creates a new session with 'timeout' */
struct afb_session *afb_session_create (int timeout)
{
	time_t now;
	struct afb_session *session;

	/* cleaning */
	pthread_mutex_lock(&sessions.mutex);
	now = cleanup();
	session = new_session(timeout, now);
	pthread_mutex_unlock(&sessions.mutex);

	return session;
}

/* Searchs the session of 'uuid' */
struct afb_session *afb_session_search (const char *uuid)
{
	struct afb_session *session;

	/* cleaning */
	pthread_mutex_lock(&sessions.mutex);
	cleanup();
	session = search(uuid, pearson4(uuid));
	if (session)
		__atomic_add_fetch(&session->refcount, 1, __ATOMIC_RELAXED);
	pthread_mutex_unlock(&sessions.mutex);
	return session;

}

/* This function will return exiting session or newly created session */
struct afb_session *afb_session_get (const char *uuid, int timeout, int *created)
{
	int idx;
	struct afb_session *session;
	time_t now;

	/* cleaning */
	pthread_mutex_lock(&sessions.mutex);
	now = cleanup();

	/* search for an existing one not too old */
	if (!uuid)
		session = new_session(timeout, now);
	else {
		idx = pearson4(uuid);
		session = search(uuid, idx);
		if (session) {
			__atomic_add_fetch(&session->refcount, 1, __ATOMIC_RELAXED);
			pthread_mutex_unlock(&sessions.mutex);
			if (created)
				*created = 0;
			return session;
		}
		session = add_session (uuid, timeout, now, idx);
	}
	pthread_mutex_unlock(&sessions.mutex);

	if (created)
		*created = !!session;

	return session;
}

/* increase the use count on the session */
struct afb_session *afb_session_addref(struct afb_session *session)
{
	if (session != NULL)
		__atomic_add_fetch(&session->refcount, 1, __ATOMIC_RELAXED);
	return session;
}

/* decrease the use count of the session */
void afb_session_unref(struct afb_session *session)
{
	if (session != NULL) {
		assert(session->refcount != 0);
		if (!__atomic_sub_fetch(&session->refcount, 1, __ATOMIC_RELAXED)) {
			pthread_mutex_lock(&session->mutex);
			if (session->uuid[0] == 0)
				destroy (session);
			else
				pthread_mutex_unlock(&session->mutex);
		}
	}
}

// Free Client Session Context
void afb_session_close (struct afb_session *session)
{
	assert(session != NULL);
	pthread_mutex_lock(&session->mutex);
	if (session->uuid[0] != 0) {
		session->uuid[0] = 0;
		remove_all_cookies(session);
		if (session->refcount == 0) {
			destroy (session);
			return;
		}
	}
	pthread_mutex_unlock(&session->mutex);
}

// Sample Generic Ping Debug API
int afb_session_check_token (struct afb_session *session, const char *token)
{
	assert(session != NULL);
	assert(token != NULL);

	// compare current token with previous one
	if (!is_active (session, NOW))
		return 0;

	if (session->token[0] && strcmp (token, session->token) != 0)
		return 0;

	return 1;
}

// generate a new token and update client context
void afb_session_new_token (struct afb_session *session)
{
	assert(session != NULL);

	// Old token was valid let's regenerate a new one
	new_uuid(session->token);

	// keep track of time for session timeout and further clean up
	if (session->timeout != 0)
		session->expiration = NOW + session->timeout;
}

/* Returns the uuid of 'session' */
const char *afb_session_uuid (struct afb_session *session)
{
	assert(session != NULL);
	return session->uuid;
}

/* Returns the token of 'session' */
const char *afb_session_token (struct afb_session *session)
{
	assert(session != NULL);
	return session->token;
}

/**
 * Get the index of the 'key' in the cookies array.
 * @param key the key to scan
 * @return the index of the list for key within cookies
 */
static int cookeyidx(const void *key)
{
	intptr_t x = (intptr_t)key;
	unsigned r = (unsigned)((x >> 5) ^ (x >> 15));
	return r & COOKEYMASK;
}

/**
 * Set, get, replace, remove a cookie of 'key' for the 'session'
 *
 * The behaviour of this function depends on its parameters:
 *
 * @param session the session
 * @param key     the key of the cookie
 * @param makecb  the creation function
 * @param freecb  the release function
 * @param closure an argument
 * @param replace a boolean enforcing replecement of the previous value
 *
 * @return the value of the cookie
 *
 * The 'key' is a pointer and compared as pointers.
 *
 * For getting the current value of the cookie:
 *
 *   afb_session_cookie(session, key, NULL, NULL, NULL, 0)
 *
 * For storing the value of the cookie
 *
 *   afb_session_cookie(session, key, NULL, NULL, value, 1)
 */
void *afb_session_cookie(struct afb_session *session, const void *key, void *(*makecb)(void *closure), void (*freecb)(void *item), void *closure, int replace)
{
	int idx;
	void *value;
	struct cookie *cookie, **prv;

	/* get key hashed index */
	idx = cookeyidx(key);

	/* lock session and search for the cookie of 'key' */
	lock(session);
	prv = &session->cookies[idx];
	for (;;) {
		cookie = *prv;
		if (!cookie) {
			/* 'key' not found, create value using 'closure' and 'makecb' */
			value = makecb ? makecb(closure) : closure;
			/* store the the only if it has some meaning */
			if (replace || makecb || freecb) {
				cookie = malloc(sizeof *cookie);
				if (!cookie) {
					errno = ENOMEM;
					/* calling freecb if there is no makecb may have issue */
					if (makecb && freecb)
						freecb(value);
					value = NULL;
				} else {
					cookie->key = key;
					cookie->value = value;
					cookie->freecb = freecb;
					cookie->next = NULL;
					*prv = cookie;
				}
			}
			break;
		} else if (cookie->key == key) {
			/* cookie of key found */
			if (!replace)
				/* not replacing, get the value */
				value = cookie->value;
			else {
				/* create value using 'closure' and 'makecb' */
				value = makecb ? makecb(closure) : closure;

				/* free previous value is needed */
				if (cookie->value != value && cookie->freecb)
					cookie->freecb(cookie->value);

				/* store the value and its releaser */
				cookie->value = value;
				cookie->freecb = freecb;

				/* but if both are NULL drop the cookie */
				if (!value && !freecb) {
					*prv = cookie->next;
					free(cookie);
				}
			}
			break;
		} else {
			prv = &(cookie->next);
		}
	}

	/* unlock the session and return the value */
	unlock(session);
	return value;
}

void *afb_session_get_cookie(struct afb_session *session, const void *key)
{
	return afb_session_cookie(session, key, NULL, NULL, NULL, 0);
}

int afb_session_set_cookie(struct afb_session *session, const void *key, void *value, void (*freecb)(void*))
{
	return -(value != afb_session_cookie(session, key, NULL, freecb, value, 1));
}

