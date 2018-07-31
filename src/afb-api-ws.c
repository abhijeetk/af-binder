/*
 * Copyright (C) 2015-2018 "IoT.bzh"
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
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-api-ws.h"
#include "afb-fdev.h"
#include "afb-socket.h"
#include "afb-stub-ws.h"
#include "verbose.h"
#include "fdev.h"

struct api_ws
{
	char *uri;		/* uri of the object for the API */
	char *api;		/* api name of the interface */
	struct fdev *fdev;	/* fdev handler */
	struct afb_apiset *apiset;
};

/******************************************************************************/

/*
 * create a structure api_ws not connected to the 'uri'.
 */
static struct api_ws *api_ws_make(const char *uri)
{
	struct api_ws *api;
	size_t length;

	/* allocates the structure */
	length = strlen(uri);
	api = calloc(1, sizeof *api + 1 + length);
	if (api == NULL) {
		errno = ENOMEM;
		goto error;
	}

	/* uri is copied after the struct */
	api->uri = (char*)(api+1);
	memcpy(api->uri, uri, length + 1);

	/* api name is at the end of the uri */
	while (length && uri[length - 1] != '/' && uri[length - 1] != ':')
		length = length - 1;
	api->api = &api->uri[length];
	if (api->api == NULL || !afb_api_is_valid_name(api->api)) {
		errno = EINVAL;
		goto error2;
	}

	return api;

error2:
	free(api);
error:
	return NULL;
}

/**********************************************************************************/

int afb_api_ws_add_client(const char *uri, struct afb_apiset *declare_set, struct afb_apiset *call_set, int strong)
{
	struct api_ws *apiws;
	struct afb_stub_ws *stubws;

	/* create the ws client api */
	apiws = api_ws_make(uri);
	if (apiws == NULL)
		goto error;

	/* connect to the service */
	apiws->fdev = afb_socket_open(apiws->uri, 0);
	if (!apiws->fdev)
		goto error2;

	stubws = afb_stub_ws_create_client(apiws->fdev, apiws->api, call_set);
	if (!stubws) {
		ERROR("can't setup client ws service to %s", apiws->uri);
		goto error3;
	}
	if (afb_stub_ws_client_add(stubws, declare_set) < 0) {
		ERROR("can't add the client to the apiset for service %s", apiws->uri);
		goto error3;
	}
	free(apiws);
	return 0;
error3:
	afb_stub_ws_unref(stubws);
error2:
	free(apiws);
error:
	return -!!strong;
}

int afb_api_ws_add_client_strong(const char *uri, struct afb_apiset *declare_set, struct afb_apiset *call_set)
{
	return afb_api_ws_add_client(uri, declare_set, call_set, 1);
}

int afb_api_ws_add_client_weak(const char *uri, struct afb_apiset *declare_set, struct afb_apiset *call_set)
{
	return afb_api_ws_add_client(uri, declare_set, call_set, 0);
}

static int api_ws_server_accept_client(struct api_ws *apiws, struct fdev *fdev)
{
	return -!afb_stub_ws_create_server(fdev, apiws->api, apiws->apiset);
}

static void api_ws_server_accept(struct api_ws *apiws)
{
	int rc, fd;
	struct sockaddr addr;
	socklen_t lenaddr;
	struct fdev *fdev;

	lenaddr = (socklen_t)sizeof addr;
	fd = accept(fdev_fd(apiws->fdev), &addr, &lenaddr);
	if (fd < 0) {
		ERROR("can't accept connection to %s: %m", apiws->uri);
	} else {
		fdev = afb_fdev_create(fd);
		if (!fdev) {
			ERROR("can't hold accepted connection to %s: %m", apiws->uri);
			close(fd);
		} else {
			rc = api_ws_server_accept_client(apiws, fdev);
			if (rc < 0)
				ERROR("can't serve accepted connection to %s: %m", apiws->uri);
		}
	}
}

static int api_ws_server_connect(struct api_ws *apiws);

static void api_ws_server_listen_callback(void *closure, uint32_t revents, struct fdev *fdev)
{
	struct api_ws *apiws = closure;

	if ((revents & EPOLLIN) != 0)
		api_ws_server_accept(apiws);
	if ((revents & EPOLLHUP) != 0)
		api_ws_server_connect(apiws);
}

static void api_ws_server_disconnect(struct api_ws *apiws)
{
	fdev_unref(apiws->fdev);
	apiws->fdev = 0;
}

static int api_ws_server_connect(struct api_ws *apiws)
{
	/* ensure disconnected */
	api_ws_server_disconnect(apiws);

	/* request the service object name */
	apiws->fdev = afb_socket_open(apiws->uri, 1);
	if (!apiws->fdev)
		ERROR("can't create socket %s", apiws->uri);
	else {
		/* listen for service */
		fdev_set_events(apiws->fdev, EPOLLIN);
		fdev_set_callback(apiws->fdev, api_ws_server_listen_callback, apiws);
		return 0;
	}
	return -1;
}

/* create the service */
int afb_api_ws_add_server(const char *uri, struct afb_apiset *declare_set, struct afb_apiset *call_set)
{
	int rc;
	struct api_ws *apiws;

	/* creates the ws api object */
	apiws = api_ws_make(uri);
	if (apiws == NULL)
		goto error;

	/* check api name */
	if (!afb_apiset_lookup(call_set, apiws->api, 1)) {
		ERROR("Can't provide ws-server for %s: API %s doesn't exist", uri, apiws->api);
		goto error2;
	}

	/* connect for serving */
	rc = api_ws_server_connect(apiws);
	if (rc < 0)
		goto error2;

	apiws->apiset = afb_apiset_addref(call_set);
	return 0;

error2:
	free(apiws);
error:
	return -1;
}
