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
#define AFB_BINDING_PRAGMA_NO_VERBOSE_MACRO

#include <stdlib.h>

#include <afb/afb-auth.h>

#include "afb-auth.h"
#include "afb-context.h"
#include "afb-xreq.h"
#include "afb-cred.h"
#include "verbose.h"

int afb_auth_check(struct afb_xreq *xreq, const struct afb_auth *auth)
{
	switch (auth->type) {
	default:
	case afb_auth_No:
		return 0;

	case afb_auth_Token:
		return afb_context_check(&xreq->context);

	case afb_auth_LOA:
		return afb_context_check_loa(&xreq->context, auth->loa);

	case afb_auth_Permission:
		return afb_auth_has_permission(xreq, auth->text);

	case afb_auth_Or:
		return afb_auth_check(xreq, auth->first) || afb_auth_check(xreq, auth->next);

	case afb_auth_And:
		return afb_auth_check(xreq, auth->first) && afb_auth_check(xreq, auth->next);

	case afb_auth_Not:
		return !afb_auth_check(xreq, auth->first);

	case afb_auth_Yes:
		return 1;
	}
}

/*********************************************************************************/
#ifdef BACKEND_PERMISSION_IS_CYNARA

#include <pthread.h>
#include <cynara-client.h>

static cynara *handle;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int afb_auth_has_permission(struct afb_xreq *xreq, const char *permission)
{
	int rc;

	if (!xreq->cred) {
		/* case of permission for self */
		return 1;
	}
	if (!permission) {
		ERROR("Got a null permission!");
		return 0;
	}

	/* cynara isn't reentrant */
	pthread_mutex_lock(&mutex);

	/* lazy initialisation */
	if (!handle) {
		rc = cynara_initialize(&handle, NULL);
		if (rc != CYNARA_API_SUCCESS) {
			handle = NULL;
			ERROR("cynara initialisation failed with code %d", rc);
			return 0;
		}
	}

	/* query cynara permission */
	rc = cynara_check(handle, xreq->cred->label, afb_context_uuid(&xreq->context), xreq->cred->user, permission);

	pthread_mutex_unlock(&mutex);
	return rc == CYNARA_API_ACCESS_ALLOWED;
}

/*********************************************************************************/
#else
int afb_auth_has_permission(struct afb_xreq *xreq, const char *permission)
{
	WARNING("Granting permission %s by default of backend", permission ?: "(null)");
	return !!permission;
}
#endif

