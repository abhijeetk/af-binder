/*
 * Copyright (C) 2016, 2017 "IoT.bzh"
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <systemd/sd-event.h>
#include <systemd/sd-daemon.h>

#include <uuid/uuid.h>
#include <json-c/json.h>
#include <afb/afb-binding-v2.h>

#include "afs-supervision.h"
#include "afb-systemd.h"
#include "afb-session.h"
#include "afb-cred.h"
#include "afb-stub-ws.h"
#include "afb-api.h"
#include "afb-xreq.h"
#include "afb-api-so-v2.h"
#include "afb-api-ws.h"
#include "afb-apiset.h"
#include "afb-fdev.h"
#include "jobs.h"
#include "verbose.h"
#include "wrap-json.h"

extern void afs_discover(const char *pattern, void (*callback)(void *closure, pid_t pid), void *closure);

/* supervised items */
struct supervised
{
	/* link to the next supervised */
	struct supervised *next;

	/* credentials of the supervised */
	struct afb_cred *cred;

	/* connection with the supervised */
	struct afb_stub_ws *stub;
};

/* api and apiset name */
static const char supervision_apiname[] = AFS_SURPERVISION_APINAME_INTERNAL;
static const char supervisor_apiname[] = "supervisor";

/* the main apiset */
struct afb_apiset *main_apiset;

/* the empty apiset */
static struct afb_apiset *empty_apiset;

/* supervision socket path */
static const char supervision_socket_path[] = AFS_SURPERVISION_SOCKET;

/* global mutex */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* list of supervised daemons */
static struct supervised *superviseds;

/*************************************************************************************/

static int afb_init_supervision_api();

/*************************************************************************************/

/**
 * Creates the supervisor socket for 'path' and return it
 * return -1 in case of failure
 */
static int create_supervision_socket(const char *path)
{
	int fd, rc;
	struct sockaddr_un addr;
	size_t length;

	/* check the path's length */
	length = strlen(path);
	if (length >= 108) {
		ERROR("Path name of supervision socket too long: %d", (int)length);
		errno = ENAMETOOLONG;
		return -1;
	}

	/* create a socket */
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		ERROR("Can't create socket: %m");
		return fd;
	}

	/* setup the bind to a path */
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path);
	if (addr.sun_path[0] == '@')
		addr.sun_path[0] = 0; /* abstract sockets */
	else
		unlink(path);

	/* binds the socket to the path */
	rc = bind(fd, (struct sockaddr *) &addr, (socklen_t)(sizeof addr));
	if (rc < 0) {
		ERROR("can't bind socket to %s", path);
		close(fd);
		return rc;
	}
	return fd;
}

/**
 * send on 'fd' an initiator with 'command'
 * return 0 on success or -1 on failure
 */
static int send_initiator(int fd, const char *command)
{
	int rc;
	ssize_t swr;
	struct afs_supervision_initiator asi;

	/* set  */
	memset(&asi, 0, sizeof asi);
	strcpy(asi.interface, AFS_SURPERVISION_INTERFACE_1);
	if (command)
		strncpy(asi.extra, command, sizeof asi.extra - 1);

	/* send the initiator */
	swr = write(fd, &asi, sizeof asi);
	if (swr < 0) {
		ERROR("Can't send initiator: %m");
		rc = -1;
	} else if (swr < sizeof asi) {
		ERROR("Sending incomplete initiator: %m");
		rc = -1;
	} else
		rc = 0;
	return rc;
}

/*
 * checks whether the incomming supervised represented by its creds
 * is to be accepted or not.
 * return 1 if yes or 0 otherwise.
 */
static int should_accept(struct afb_cred *cred)
{
	return cred && cred->pid != getpid(); /* not me! */
}

static void on_supervised_hangup(struct afb_stub_ws *stub)
{
	struct supervised *s, **ps;
	pthread_mutex_lock(&mutex);
	ps = &superviseds;
	while ((s = *ps) && s->stub != stub)
		ps = &s->next;
	if (s) {
		*ps = s->next;
		afb_stub_ws_unref(stub);
	}
	pthread_mutex_unlock(&mutex);
}

/*
 * create a supervised for socket 'fd' and 'cred'
 * return 0 in case of success or -1 in case of error
 */
static int make_supervised(int fd, struct afb_cred *cred)
{
	struct supervised *s;
	struct fdev *fdev;

	s = malloc(sizeof *s);
	if (!s)
		return -1;

	fdev = afb_fdev_create(fd);
	if (!fdev) {
		free(s);
		return -1;
	}

	s->cred = cred;
	s->stub = afb_stub_ws_create_client(fdev, supervision_apiname, empty_apiset);
	if (!s->stub) {
		free(s);
		return -1;
	}
	pthread_mutex_lock(&mutex);
	s->next = superviseds;
	superviseds = s;
	pthread_mutex_unlock(&mutex);
	afb_stub_ws_on_hangup(s->stub, on_supervised_hangup);
	return 0;
}

/**
 * Search the supervised of 'pid', return it or NULL.
 */
static struct supervised *supervised_of_pid(pid_t pid)
{
	struct supervised *s;

	pthread_mutex_lock(&mutex);
	s = superviseds;
	while (s && pid != s->cred->pid)
		s = s->next;
	pthread_mutex_unlock(&mutex);

	return s;
}

/*
 * handles incoming connection on 'sock'
 */
static void accept_supervision_link(int sock)
{
	int rc, fd;
	struct sockaddr addr;
	socklen_t lenaddr;
	struct afb_cred *cred;

	lenaddr = (socklen_t)sizeof addr;
	fd = accept(sock, &addr, &lenaddr);
	if (fd >= 0) {
		cred = afb_cred_create_for_socket(fd);
		rc = should_accept(cred);
		if (rc) {
			rc = send_initiator(fd, NULL);
			if (!rc) {
				rc = make_supervised(fd, cred);
				if (!rc)
					return;
			}
		}
		afb_cred_unref(cred);
		close(fd);
	}
}

/*
 * handle even on server socket
 */
static int listening(sd_event_source *src, int fd, uint32_t revents, void *closure)
{
	if ((revents & EPOLLIN) != 0)
		accept_supervision_link(fd);
	if ((revents & EPOLLHUP) != 0) {
		ERROR("supervision socket closed");
		exit(1);
	}
	return 0;
}

/*
 */
static void discovered_cb(void *closure, pid_t pid)
{
	struct supervised *s;

	s = supervised_of_pid(pid);
	if (!s) {
		(*(int*)closure)++;
		kill(pid, SIGHUP);
	}
}

static int discover_supervised()
{
	int n = 0;
	afs_discover("afb-daemon", discovered_cb, &n);
	return n;
}

/**
 * initalize the supervision
 */
static int init(const char *spec)
{
	int rc, fd;

	/* check argument */
	if (!spec) {
		ERROR("invalid socket spec");
		return -1;
	}

	rc = afb_session_init(100, 600, "");
	/* TODO check that value */

	/* create the apisets */
	main_apiset = afb_apiset_create(supervisor_apiname, 0);
	if (!main_apiset) {
		ERROR("Can't create supervisor's apiset");
		return -1;
	}
	empty_apiset = afb_apiset_create(supervision_apiname, 0);
	if (!empty_apiset) {
		ERROR("Can't create supervision apiset");
		return -1;
	}


	/* init the main apiset */
	rc = afb_init_supervision_api();
	if (rc < 0) {
		ERROR("Can't create supervision's apiset: %m");
		return -1;
	}

	/* create the supervision socket */
	fd = create_supervision_socket(supervision_socket_path);
	if (fd < 0)
		return fd;

	/* listen the socket */
	rc = listen(fd, 5);
	if (rc < 0) {
		ERROR("refused to listen on socket");
		return rc;
	}

	/* integrate the socket to the loop */
	rc = sd_event_add_io(afb_systemd_get_event_loop(),
				NULL, fd, EPOLLIN,
				listening, NULL);
	if (rc < 0) {
		ERROR("handling socket event isn't possible");
		return rc;
	}

	/* adds the server socket */
	rc = afb_api_ws_add_server(spec, main_apiset);
	if (rc < 0) {
		ERROR("can't start the server socket");
		return -1;
	}
	return 0;
}

/* start should not be null but */
static void start(int signum, void *arg)
{
	char *xpath = arg;
	int rc;

	if (signum)
		exit(1);

	rc = init(xpath);
	if (rc)
		exit(1);

	sd_notify(1, "READY=1");

	discover_supervised();
}

/**
 * initalize the supervision
 */
int main(int ac, char **av)
{
	verbosity = Verbosity_Level_Debug;
	/* enter job processing */
	jobs_start(3, 0, 10, start, av[1]);
	WARNING("hoops returned from jobs_enter! [report bug]");
	return 1;
}

/*********************************************************************************************************/

static struct afb_binding_data_v2 datav2;

static void f_list(struct afb_req req)
{
	char pid[50];
	struct json_object *resu, *item;
	struct supervised *s;

	resu = json_object_new_object();
	s = superviseds;
	while (s) {
		sprintf(pid, "%d", (int)s->cred->pid);
		item = NULL;
		wrap_json_pack(&item, "{si si si ss ss ss}",
				"pid", (int)s->cred->pid,
				"uid", (int)s->cred->uid,
				"gid", (int)s->cred->gid,
				"id", s->cred->id,
				"label", s->cred->label,
				"user", s->cred->user
				);
		json_object_object_add(resu, pid, item);
		s = s->next;
	}
	afb_req_success(req, resu, NULL);
}

static void f_discover(struct afb_req req)
{
	discover_supervised();
	afb_req_success(req, NULL, NULL);
}

static void propagate(struct afb_req req, const char *verb)
{
	struct afb_xreq *xreq;
	struct json_object *args, *item;
	struct supervised *s;
	struct afb_api api;
	int p;

	xreq = xreq_from_request(req.closure);
	args = afb_xreq_json(xreq);
	if (!json_object_object_get_ex(args, "pid", &item)) {
		afb_xreq_fail(xreq, "no-pid", NULL);
		return;
	}
	errno = 0;
	p = json_object_get_int(item);
	if (!p && errno) {
		afb_xreq_fail(xreq, "bad-pid", NULL);
		return;
	}
	s = supervised_of_pid((pid_t)p);
	if (!s) {
		afb_req_fail(req, "unknown-pid", NULL);
		return;
	}
	json_object_object_del(args, "pid");
	if (verb)
		xreq->request.verb = verb;
	api = afb_stub_ws_client_api(s->stub);
	api.itf->call(api.closure, xreq);
}

static void f_do(struct afb_req req)
{
	propagate(req, NULL);
}

static void f_config(struct afb_req req)
{
	propagate(req, NULL);
}

static void f_trace(struct afb_req req)
{
	propagate(req, NULL);
}

static void f_sessions(struct afb_req req)
{
	propagate(req, "slist");
}

static void f_session_close(struct afb_req req)
{
	propagate(req, "sclose");
}

static void f_exit(struct afb_req req)
{
	propagate(req, NULL);
}

static void f_debug_wait(struct afb_req req)
{
	propagate(req, "wait");
}

static void f_debug_break(struct afb_req req)
{
	propagate(req, "break");
}

static const struct afb_auth _afb_auths_v2_supervision[] = {
	/* 0 */
	{
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:#supervision:platform:access"
	}
};

static const struct afb_verb_v2 _afb_verbs_v2_supervision[] = {
    {
        .verb = "list",
        .callback = f_list,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "config",
        .callback = f_config,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "do",
        .callback = f_do,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "trace",
        .callback = f_trace,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "sessions",
        .callback = f_sessions,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "session-close",
        .callback = f_session_close,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "exit",
        .callback = f_exit,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "debug-wait",
        .callback = f_debug_wait,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "debug-break",
        .callback = f_debug_break,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "discover",
        .callback = f_discover,
        .auth = &_afb_auths_v2_supervision[0],
        .info = NULL,
        .session = AFB_SESSION_NONE_V2
    },
    { .verb = NULL }
};

static const struct afb_binding_v2 _afb_binding_v2_supervision = {
    .api = supervisor_apiname,
    .specification = NULL,
    .info = NULL,
    .verbs = _afb_verbs_v2_supervision,
    .preinit = NULL,
    .init = NULL,
    .onevent = NULL,
    .noconcurrency = 0
};

static int afb_init_supervision_api()
{
	return afb_api_so_v2_add_binding(&_afb_binding_v2_supervision, NULL, main_apiset, &datav2);
}

