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
	unsigned refcount;
	unsigned loa;
	int timeout;
	time_t expiration;    // expiration time of the token
	time_t access;
	char uuid[37];        // long term authentication of remote client
	char token[37];       // short term authentication of remote client
	struct cookie *cookies[COOKEYCOUNT];
};

// Session UUID are store in a simple array [for 10 sessions this should be enough]
static struct {
	pthread_mutex_t mutex;          // declare a mutex to protect hash table
	struct afb_session **store;          // sessions store
	int count;                      // current number of sessions
	int max;
	int timeout;
	char initok[37];
} sessions;

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

/* generate a uuid */
static void new_uuid(char uuid[37])
{
	uuid_t newuuid;
	uuid_generate(newuuid);
	uuid_unparse_lower(newuuid, uuid);
}

// Free context [XXXX Should be protected again memory abort XXXX]
static void free_data (struct afb_session *session)
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

// Create a new store in RAM, not that is too small it will be automatically extended
void afb_session_init (int max_session_count, int timeout, const char *initok)
{
	// let's create as store as hashtable does not have any
	sessions.store = calloc (1 + (unsigned)max_session_count, sizeof(struct afb_session));
	sessions.max = max_session_count;
	sessions.timeout = timeout;
	if (initok == NULL)
		/* without token, a secret is made to forbid creation of sessions */
		new_uuid(sessions.initok);
	else if (strlen(initok) < sizeof(sessions.store[0]->token))
		strcpy(sessions.initok, initok);
	else {
		ERROR("initial token '%s' too long (max length 36)", initok);
		exit(1);
	}
}

static struct afb_session *search (const char* uuid)
{
	int  idx;
	struct afb_session *session;

	assert (uuid != NULL);

	pthread_mutex_lock(&sessions.mutex);

	for (idx=0; idx < sessions.max; idx++) {
		session = sessions.store[idx];
		if (session && (0 == strcmp (uuid, session->uuid)))
			goto found;
	}
	session = NULL;

found:
	pthread_mutex_unlock(&sessions.mutex);
	return session;
}

static int destroy (struct afb_session *session)
{
	int idx;
	int status;

	assert (session != NULL);

	pthread_mutex_lock(&sessions.mutex);

	for (idx=0; idx < sessions.max; idx++) {
		if (sessions.store[idx] == session) {
			sessions.store[idx] = NULL;
			sessions.count--;
			status = 1;
			goto deleted;
		}
	}
	status = 0;
deleted:
	pthread_mutex_unlock(&sessions.mutex);
	return status;
}

static int add (struct afb_session *session)
{
	int idx;
	int status;

	assert (session != NULL);

	pthread_mutex_lock(&sessions.mutex);

	for (idx=0; idx < sessions.max; idx++) {
		if (NULL == sessions.store[idx]) {
			sessions.store[idx] = session;
			sessions.count++;
			status = 1;
			goto added;
		}
	}
	status = 0;
added:
	pthread_mutex_unlock(&sessions.mutex);
	return status;
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
static void cleanup (time_t now)
{
	struct afb_session *ctx;
	long idx;

	// Loop on Sessions Table and remove anything that is older than timeout
	for (idx=0; idx < sessions.max; idx++) {
		ctx = sessions.store[idx];
		if (ctx != NULL && is_expired(ctx, now)) {
			afb_session_close (ctx);
		}
	}
}

static struct afb_session *make_session (const char *uuid, int timeout, time_t now)
{
	struct afb_session *session;

	/* allocates a new one */
	session = calloc(1, sizeof(struct afb_session));
	if (session == NULL) {
		errno = ENOMEM;
		goto error;
	}

	/* generate the uuid */
	if (uuid == NULL) {
		new_uuid(session->uuid);
	} else {
		if (strlen(uuid) >= sizeof session->uuid) {
			errno = EINVAL;
			goto error2;
		}
		strcpy(session->uuid, uuid);
	}

	/* init the token */
	strcpy(session->token, sessions.initok);
	session->timeout = timeout;
	if (timeout != 0)
		session->expiration = now + timeout;
	else {
		session->expiration = (time_t)(~(time_t)0);
		if (session->expiration < 0)
			session->expiration = (time_t)(((unsigned long long)session->expiration) >> 1);
	}
	if (!add (session)) {
		errno = ENOMEM;
		goto error2;
	}

	session->access = now;
	session->refcount = 1;
	return session;

error2:
	free(session);
error:
	return NULL;
}

struct afb_session *afb_session_create (const char *uuid, int timeout)
{
	time_t now;

	/* cleaning */
	now = NOW;
	cleanup (now);

	/* search for an existing one not too old */
	if (uuid != NULL && search(uuid) != NULL) {
		errno = EEXIST;
		return NULL;
	}

	return make_session(uuid, timeout, now);
}

// This function will return exiting session or newly created session
struct afb_session *afb_session_get (const char *uuid, int *created)
{
	struct afb_session *session;
	time_t now;

	/* cleaning */
	now = NOW;
	cleanup (now);

	/* search for an existing one not too old */
	if (uuid != NULL) {
		session = search(uuid);
		if (session != NULL) {
			*created = 0;
			session->access = now;
			session->refcount++;
			return session;
		}
	}

	*created = 1;
	return make_session(uuid, sessions.timeout, now);
}

struct afb_session *afb_session_addref(struct afb_session *session)
{
	if (session != NULL)
		session->refcount++;
	return session;
}

void afb_session_unref(struct afb_session *session)
{
	if (session != NULL) {
		assert(session->refcount != 0);
		--session->refcount;
		if (session->refcount == 0 && session->uuid[0] == 0) {
			destroy (session);
			free(session);
		}
	}
}

// Free Client Session Context
void afb_session_close (struct afb_session *session)
{
	assert(session != NULL);
	if (session->uuid[0] != 0) {
		session->uuid[0] = 0;
	        free_data (session);
		if (session->refcount == 0) {
			destroy (session);
			free(session);
		}
	}
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

const char *afb_session_uuid (struct afb_session *session)
{
	assert(session != NULL);
	return session->uuid;
}

const char *afb_session_token (struct afb_session *session)
{
	assert(session != NULL);
	return session->token;
}

unsigned afb_session_get_LOA (struct afb_session *session)
{
	assert(session != NULL);
	return session->loa;
}

void afb_session_set_LOA (struct afb_session *session, unsigned loa)
{
	assert(session != NULL);
	session->loa = loa;
}

void *afb_session_get_cookie(struct afb_session *session, const void *key)
{
	struct cookie *cookie;
	int idx;

	idx = cookeyidx(key);
	cookie = session->cookies[idx];
	while(cookie != NULL) {
		if (cookie->key == key)
			return cookie->value;
		cookie = cookie->next;
	}
	return NULL;
}

int afb_session_set_cookie(struct afb_session *session, const void *key, void *value, void (*freecb)(void*))
{
	struct cookie *cookie;
	int idx;

	/* search for a replacement */
	idx = cookeyidx(key);
	cookie = session->cookies[idx];
	while(cookie != NULL) {
		if (cookie->key == key) {
			if (cookie->value != value && cookie->freecb)
				cookie->freecb(cookie->value);
			cookie->value = value;
			cookie->freecb = freecb;
			return 0;
		}
		cookie = cookie->next;
	}

	/* allocates */
	cookie = malloc(sizeof *cookie);
	if (cookie == NULL) {
		errno = ENOMEM;
		return -1;
	}

	cookie->key = key;
	cookie->value = value;
	cookie->freecb = freecb;
	cookie->next = session->cookies[idx];
	session->cookies[idx] = cookie;
	return 0;
}

