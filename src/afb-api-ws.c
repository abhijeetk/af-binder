/*
 * Copyright (C) 2015, 2016, 2017 "IoT.bzh"
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
#define NO_PLUGIN_VERBOSE_MACRO

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

#include <systemd/sd-event.h>
#include "afb-api.h"
#include "afb-apiset.h"
#include "afb-systemd.h"
#include "afb-stub-ws.h"
#include "verbose.h"
#include "sd-fds.h"

struct api_ws
{
	char *path;		/* path of the object for the API */
	char *api;		/* api name of the interface */
	int fd;			/* file descriptor */
	sd_event_source *listensrc; /**< systemd source for server socket */
	struct afb_apiset *apiset;
};

/******************************************************************************/

/*
 * create a structure api_ws not connected to the 'path'.
 */
static struct api_ws *api_ws_make(const char *path)
{
	struct api_ws *api;
	size_t length;

	/* allocates the structure */
	length = strlen(path);
	api = calloc(1, sizeof *api + 1 + length);
	if (api == NULL) {
		errno = ENOMEM;
		goto error;
	}

	/* path is copied after the struct */
	api->path = (char*)(api+1);
	memcpy(api->path, path, length + 1);

	/* api name is at the end of the path */
	while (length && path[length - 1] != '/' && path[length - 1] != ':')
		length = length - 1;
	api->api = &api->path[length];
	if (api->api == NULL || !afb_api_is_valid_name(api->api, 1)) {
		errno = EINVAL;
		goto error2;
	}

	api->fd = -1;
	return api;

error2:
	free(api);
error:
	return NULL;
}

static int api_ws_socket_unix(const char *path, int server)
{
	int fd, rc;
	struct sockaddr_un addr;
	size_t length;

	length = strlen(path);
	if (length >= 108) {
		errno = ENAMETOOLONG;
		return -1;
	}

	if (server && path[0] != '@')
		unlink(path);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return fd;

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path);
	if (addr.sun_path[0] == '@')
		addr.sun_path[0] = 0; /* implement abstract sockets */
	if (server) {
		rc = bind(fd, (struct sockaddr *) &addr, (socklen_t)(sizeof addr));
	} else {
		rc = connect(fd, (struct sockaddr *) &addr, (socklen_t)(sizeof addr));
	}
	if (rc < 0) {
		close(fd);
		return rc;
	}
	return fd;
}

static int api_ws_socket_inet(const char *path, int server)
{
	int rc, fd;
	const char *service, *host, *api;
	struct addrinfo hint, *rai, *iai;

	/* scan the uri */
	api = strrchr(path, '/');
	service = strrchr(path, ':');
	if (api == NULL || service == NULL || api < service) {
		errno = EINVAL;
		return -1;
	}
	host = strndupa(path, service++ - path);
	service = strndupa(service, api - service);

	/* get addr */
	memset(&hint, 0, sizeof hint);
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	rc = getaddrinfo(host, service, &hint, &rai);
	if (rc != 0) {
		errno = EINVAL;
		return -1;
	}

	/* get the socket */
	iai = rai;
	while (iai != NULL) {
		fd = socket(iai->ai_family, iai->ai_socktype, iai->ai_protocol);
		if (fd >= 0) {
			if (server) {
				rc = bind(fd, iai->ai_addr, iai->ai_addrlen);
			} else {
				rc = connect(fd, iai->ai_addr, iai->ai_addrlen);
			}
			if (rc == 0) {
				freeaddrinfo(rai);
				return fd;
			}
			close(fd);
		}
		iai = iai->ai_next;
	}
	freeaddrinfo(rai);
	return -1;
}

static int api_ws_socket(const char *path, int server)
{
	int fd, rc;

	/* check for systemd socket */
	if (0 == strncmp(path, "sd:", 3))
		fd = sd_fds_for(path + 3);
	else {
		/* check for unix socket */
		if (0 == strncmp(path, "unix:", 5))
			/* unix socket */
			fd = api_ws_socket_unix(path + 5, server);
		else
			/* inet socket */
			fd = api_ws_socket_inet(path, server);

		if (fd >= 0 && server) {
			rc = 1;
			setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof rc);
			rc = listen(fd, 5);
		}
	}
	/* configure the socket */
	if (fd >= 0) {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
		fcntl(fd, F_SETFL, O_NONBLOCK);
	}
	return fd;
}

/**********************************************************************************/

int afb_api_ws_add_client(const char *path, struct afb_apiset *apiset, int strong)
{
	struct api_ws *apiws;
	struct afb_stub_ws *stubws;

	/* create the ws client api */
	apiws = api_ws_make(path);
	if (apiws == NULL)
		goto error;

	/* connect to the service */
	apiws->fd = api_ws_socket(apiws->path, 0);
	if (apiws->fd < 0) {
		ERROR("can't connect to ws service %s", apiws->path);
		goto error2;
	}

	stubws = afb_stub_ws_create_client(apiws->fd, apiws->api, apiset);
	if (!stubws) {
		ERROR("can't setup client ws service to %s", apiws->path);
		goto error3;
	}
	if (afb_stub_ws_client_add(stubws, apiset) < 0) {
		ERROR("can't add the client to the apiset for service %s", apiws->path);
		goto error4;
	}
	free(apiws);
	return 0;
error4:
	afb_stub_ws_unref(stubws);
error3:
	close(apiws->fd);
error2:
	free(apiws);
error:
	return -!!strong;
}

int afb_api_ws_add_client_strong(const char *path, struct afb_apiset *apiset)
{
	return afb_api_ws_add_client(path, apiset, 1);
}

int afb_api_ws_add_client_weak(const char *path, struct afb_apiset *apiset)
{
	return afb_api_ws_add_client(path, apiset, 0);
}

static int api_ws_server_accept_client(struct api_ws *apiws, int fd)
{
	return -!afb_stub_ws_create_server(fd, apiws->api, apiws->apiset);
}

static void api_ws_server_accept(struct api_ws *apiws)
{
	int rc, fd;
	struct sockaddr addr;
	socklen_t lenaddr;

	lenaddr = (socklen_t)sizeof addr;
	fd = accept(apiws->fd, &addr, &lenaddr);
	if (fd >= 0) {
		rc = api_ws_server_accept_client(apiws, fd);
		if (rc >= 0)
			return;
		close(fd);
	}
}

static int api_ws_server_connect(struct api_ws *apiws);

static int api_ws_server_listen_callback(sd_event_source *src, int fd, uint32_t revents, void *closure)
{
	struct api_ws *apiws = closure;

	if ((revents & EPOLLIN) != 0)
		api_ws_server_accept(apiws);
	if ((revents & EPOLLHUP) != 0)
		api_ws_server_connect(apiws);
	return 0;
}

static void api_ws_server_disconnect(struct api_ws *apiws)
{
	if (apiws->listensrc != NULL) {
		sd_event_source_unref(apiws->listensrc);
		apiws->listensrc = NULL;
	}
	if (apiws->fd >= 0) {
		close(apiws->fd);
		apiws->fd = -1;
	}
}

static int api_ws_server_connect(struct api_ws *apiws)
{
	int rc;

	/* ensure disconnected */
	api_ws_server_disconnect(apiws);

	/* request the service object name */
	apiws->fd = api_ws_socket(apiws->path, 1);
	if (apiws->fd < 0)
		ERROR("can't create socket %s", apiws->path);
	else {
		/* listen for service */
		rc = sd_event_add_io(afb_systemd_get_event_loop(),
				&apiws->listensrc, apiws->fd, EPOLLIN,
				api_ws_server_listen_callback, apiws);
		if (rc >= 0)
			return 0;
		close(apiws->fd);
		errno = -rc;
		ERROR("can't add ws object %s", apiws->path);
	}
	return -1;
}

/* create the service */
int afb_api_ws_add_server(const char *path, struct afb_apiset *apiset)
{
	int rc;
	struct api_ws *apiws;

	/* creates the ws api object */
	apiws = api_ws_make(path);
	if (apiws == NULL)
		goto error;

	/* check api name */
	if (!afb_apiset_lookup(apiset, apiws->api, 1)) {
		ERROR("Can't provide ws-server for %s: API %s doesn't exist", path, apiws->api);
		goto error2;
	}

	/* connect for serving */
	rc = api_ws_server_connect(apiws);
	if (rc < 0)
		goto error2;

	apiws->apiset = afb_apiset_addref(apiset);
	return 0;

error2:
	free(apiws);
error:
	return -1;
}


