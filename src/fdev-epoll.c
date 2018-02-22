/*
 * Copyright (C) 2018 "IoT.bzh"
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

#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>

#define FDEV_PROVIDER
#include "fdev.h"
#include "fdev-epoll.h"

#define epollfd(fdev_epoll)  ((int)(intptr_t)fdev_epoll)

static void disable(void *closure, const struct fdev *fdev)
{
	struct fdev_epoll *fdev_epoll = closure;
	epoll_ctl(epollfd(fdev_epoll), EPOLL_CTL_DEL, fdev_fd(fdev), 0);
}

static void enable(void *closure, const struct fdev *fdev)
{
	struct fdev_epoll *fdev_epoll = closure;
	struct epoll_event event;
	int rc, fd;

	fd = fdev_fd(fdev);
	event.events = fdev_events(fdev);
	event.data.ptr = (void*)fdev;
	rc = epoll_ctl(epollfd(fdev_epoll), EPOLL_CTL_MOD, fd, &event);
	if (rc < 0 && errno == ENOENT)
		epoll_ctl(epollfd(fdev_epoll), EPOLL_CTL_ADD, fd, &event);
}

static struct fdev_itf itf =
{
	.unref = 0,
	.disable = disable,
	.enable = enable
};

struct fdev_epoll *fdev_epoll_create()
{
	int fd = epoll_create1(EPOLL_CLOEXEC);
	if (!fd) {
		fd = dup(fd);
		close(0);
	}
	return fd < 0 ? 0 : (struct fdev_epoll*)(intptr_t)fd;
}

void fdev_epoll_destroy(struct fdev_epoll *fdev_epoll)
{
	close(epollfd(fdev_epoll));
}

int fdev_epoll_fd(struct fdev_epoll *fdev_epoll)
{
	return epollfd(fdev_epoll);
}

struct fdev *fdev_epoll_add(struct fdev_epoll *fdev_epoll, int fd)
{
	struct fdev *fdev;

	fdev = fdev_create(fd);
	if (fdev)
		fdev_set_itf(fdev, &itf, fdev_epoll);
	return fdev;
}

void fdev_epoll_wait_and_dispatch(struct fdev_epoll *fdev_epoll, int timeout_ms)
{
	struct fdev *fdev;
	struct epoll_event events[8];
	int rc, i;

	rc = epoll_wait(epollfd(fdev_epoll), events, sizeof events / sizeof *events, timeout_ms < 0 ? -1 : timeout_ms);
	for (i = 0 ; i < rc ; i++) {
		fdev = events[i].data.ptr;
		fdev_dispatch(fdev, events[i].events);
	}
}

