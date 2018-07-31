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

#include "afb-fdev.h"
#include "afb-socket.h"
#include "afb-systemd.h"
#include "fdev.h"
#include "verbose.h"

#define BACKLOG  5

/******************************************************************************/

static int open_unix(const char *spec, int server)
{
	int fd, rc, abstract;
	struct sockaddr_un addr;
	size_t length;

	abstract = spec[0] == '@';

	/* check the length */
	length = strlen(spec);
	if (length >= 108) {
		errno = ENAMETOOLONG;
		return -1;
	}

	/* remove the file on need */
	if (server && !abstract)
		unlink(spec);

	/* create a  socket */
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return fd;

	/* prepare address  */
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, spec);
	if (abstract)
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

static int open_inet(const char *spec, int server)
{
	int rc, fd;
	const char *service, *host, *api;
	struct addrinfo hint, *rai, *iai;

	/* scan the uri */
	api = strrchr(spec, '/');
	service = strrchr(spec, ':');
	if (api == NULL || service == NULL || api < service) {
		errno = EINVAL;
		return -1;
	}
	host = strndupa(spec, service++ - spec);
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

static int open_systemd(const char *spec)
{
#if defined(NO_SYSTEMD_ACTIVATION)
	errno = EAFNOSUPPORT;
	fd = -1;
#else
	return afb_systemd_fds_for(spec);
#endif
}

/******************************************************************************/

enum type {
	Type_Inet,
	Type_Systemd,
	Type_Unix
};

struct entry
{
	const char *prefix;
	unsigned type: 2;
	unsigned noreuseaddr: 1;
	unsigned nolisten: 1;
};

static struct entry entries[] = { /* default at first place */
	{
		.prefix = "tcp:",
		.type = Type_Inet
	},
	{
		.prefix = "sd:",
		.type = Type_Systemd,
		.noreuseaddr = 1,
		.nolisten = 1
	},
	{
		.prefix = "unix:",
		.type = Type_Unix
	}
};

/******************************************************************************/

/* get the entry of the uri by searching to its prefix */
static struct entry *get_entry(const char *uri, int *offset)
{
	int l, search = 1, i = (int)(sizeof entries / sizeof * entries);

	while (search) {
		if (!i) {
			l = 0;
			search = 0;
		} else {
			i--;
			l = (int)strlen(entries[i].prefix);
			search = strncmp(uri, entries[i].prefix, l);
		}
	}

	*offset = l;
	return &entries[i];
}

static int open_any(const char *uri, int server)
{
	int fd, rc, offset;
	struct entry *e;

	/* search for the entry */
	e = get_entry(uri, &offset);

	/* get the names */

	uri += offset;

	/* open the socket */
	switch (e->type) {
	case Type_Unix:
		fd = open_unix(uri, server);
		break;
	case Type_Inet:
		fd = open_inet(uri, server);
		break;
	case Type_Systemd:
		fd = open_systemd(uri);
		break;
	default:
		errno = EAFNOSUPPORT;
		fd = -1;
		break;
	}
	if (fd < 0)
		return -1;

	/* set it up */
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (server) {
		if (!e->noreuseaddr) {
			rc = 1;
			setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof rc);
		}
		if (!e->nolisten)
			listen(fd, BACKLOG);
	}
	return fd;
}

struct fdev *afb_socket_open(const char *uri, int server)
{
	int fd;
	struct fdev *fdev;

	fd = open_any(uri, server);
	if (fd < 0)
		goto error;

	fdev = afb_fdev_create(fd);
	if (!fdev)
		goto error2;

	return fdev;

error2:
	close(fd);
error:
	ERROR("can't make %s socket for %s", server ? "server" : "client", uri);
	return NULL;
}

