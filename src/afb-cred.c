/*
 * Copyright (C) 2017 "IoT.bzh"
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "afb-cred.h"

#define MAX_LABEL_LENGTH  1024

static struct afb_cred *current;

static struct afb_cred *mkcred(uid_t uid, gid_t gid, pid_t pid, const char *label, size_t size)
{
	struct afb_cred *cred;
	char *dest;

	cred = malloc(1 + size + sizeof *cred);
	if (!cred)
		errno = ENOMEM;
	else {
		cred->refcount = 1;
		cred->uid = uid;
		cred->gid = gid;
		cred->pid = pid;
		dest = (char*)(&cred[1]);
		memcpy(dest, label, size);
		dest[size] = 0;
		cred->label = dest;
		cred->id = dest;
		dest = strrchr(dest, ':');
		if (dest && dest[1])
			cred->id = &dest[1];
	}
	return cred;
}

static struct afb_cred *mkcurrent()
{
	char label[MAX_LABEL_LENGTH];
	int fd;
	ssize_t rc;

	fd = open("/proc/self/attr/current", O_RDONLY);
	if (fd < 0)
		rc = 0;
	else {
		rc = read(fd, label, sizeof label);
		if (rc < 0)
			rc = 0;
		close(fd);
	}

	return mkcred(getuid(), getgid(), getpid(), label, (size_t)rc);
}

struct afb_cred *afb_cred_create(uid_t uid, gid_t gid, pid_t pid, const char *label)
{
	label = label ? : "";
	return mkcred(uid, gid, pid, label, strlen(label));
}

struct afb_cred *afb_cred_create_for_socket(int fd)
{
	int rc;
	socklen_t length;
	struct ucred ucred;
	char label[MAX_LABEL_LENGTH];

	/* get the credentials */
	length = (socklen_t)(sizeof ucred);
	rc = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &length);
	if (rc < 0 || length != (socklen_t)(sizeof ucred)) {
		if (!rc)
			errno = EINVAL;
		return NULL;
	}

	/* get the security label */
	length = (socklen_t)(sizeof label);
	rc = getsockopt(fd, SOL_SOCKET, SO_PEERSEC, label, &length);
	if (rc < 0 || length > (socklen_t)(sizeof label)) {
		if (!rc)
			errno = EINVAL;
		return NULL;
	}

	/* makes the result */
	return mkcred(ucred.uid, ucred.gid, ucred.pid, label, (size_t)length);
}

struct afb_cred *afb_cred_addref(struct afb_cred *cred)
{
	if (cred)
		__atomic_add_fetch(&cred->refcount, 1, __ATOMIC_RELAXED);
	return cred;
}

void afb_cred_unref(struct afb_cred *cred)
{
	if (cred && !__atomic_sub_fetch(&cred->refcount, 1, __ATOMIC_RELAXED)) {
		if (cred != current)
			free(cred);
		else
			cred->refcount = 1;
	}
}

struct afb_cred *afb_cred_current()
{
	if (!current)
		current = mkcurrent();
	return afb_cred_addref(current);
}

