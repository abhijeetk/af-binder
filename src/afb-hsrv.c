/*
 * Copyright (C) 2016, 2017, 2018 "IoT.bzh"
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <json-c/json.h>
#include <microhttpd.h>

#include "afb-method.h"
#include "afb-context.h"
#include "afb-xreq.h"
#include "afb-hreq.h"
#include "afb-hsrv.h"
#include "afb-fdev.h"
#include "fdev.h"
#include "verbose.h"
#include "locale-root.h"

#include "afb-systemd.h"
#include "jobs.h"

#define JSON_CONTENT  "application/json"
#define FORM_CONTENT  MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA


struct hsrv_handler {
	struct hsrv_handler *next;
	const char *prefix;
	size_t length;
	int (*handler) (struct afb_hreq *, void *);
	void *data;
	int priority;
};

struct hsrv_alias {
	struct locale_root *root;
	int relax;
};

struct afb_hsrv {
	unsigned refcount;
	struct hsrv_handler *handlers;
	struct MHD_Daemon *httpd;
	struct fdev *fdev;
	char *cache_to;
};

static void reply_error(struct MHD_Connection *connection, unsigned int status)
{
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	struct MHD_Response *response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
	MHD_queue_response(connection, status, response);
	MHD_destroy_response(response);
}

static int postproc(void *cls,
                    enum MHD_ValueKind kind,
                    const char *key,
                    const char *filename,
                    const char *content_type,
                    const char *transfer_encoding,
                    const char *data,
		    uint64_t off,
		    size_t size)
{
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	struct afb_hreq *hreq = cls;
	if (filename != NULL)
		return afb_hreq_post_add_file(hreq, key, filename, data, size);
	else
		return afb_hreq_post_add(hreq, key, data, size);
}

static int access_handler(
		void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *methodstr,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **recordreq)
{
	int rc;
	struct afb_hreq *hreq;
	enum afb_method method;
	struct afb_hsrv *hsrv;
	struct hsrv_handler *iter;
	const char *type;
	enum json_tokener_error jerr;
    fprintf(stderr, "NETWORK : [%d] %s %s %d \nurl %s \nmethodstr %s \nversion %s \nupload_data %s\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__, url, methodstr, version, upload_data);
	hsrv = cls;
	hreq = *recordreq;
	if (hreq == NULL) {
		/* get the method */
		method = get_method(methodstr);
		method &= afb_method_get | afb_method_post;
		if (method == afb_method_none) {
			WARNING("Unsupported HTTP operation %s", methodstr);
			reply_error(connection, MHD_HTTP_BAD_REQUEST);
			return MHD_YES;
		}

		/* create the request */
		hreq = afb_hreq_create();
		if (hreq == NULL) {
			ERROR("Can't allocate 'hreq'");
			reply_error(connection, MHD_HTTP_INTERNAL_SERVER_ERROR);
			return MHD_YES;
		}

		/* init the request */
		hreq->hsrv = hsrv;
		hreq->cacheTimeout = hsrv->cache_to;
		hreq->connection = connection;
		hreq->method = method;
		hreq->version = version;
		hreq->lang = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_ACCEPT_LANGUAGE);
		hreq->tail = hreq->url = url;
		hreq->lentail = hreq->lenurl = strlen(url);
		*recordreq = hreq;

		/* init the post processing */
		if (method == afb_method_post) {
			type = afb_hreq_get_header(hreq, MHD_HTTP_HEADER_CONTENT_TYPE);
			if (type == NULL) {
				/* an empty post, let's process it as a get */
				hreq->method = afb_method_get;
			} else if (strcasestr(type, FORM_CONTENT) != NULL) {
				hreq->postform = MHD_create_post_processor (connection, 65500, postproc, hreq);
				if (hreq->postform == NULL) {
					ERROR("Can't create POST processor");
					afb_hreq_reply_error(hreq, MHD_HTTP_INTERNAL_SERVER_ERROR);
				}
				return MHD_YES;
			} else if (strcasestr(type, JSON_CONTENT) != NULL) {
				hreq->tokener = json_tokener_new();
				if (hreq->tokener == NULL) {
					ERROR("Can't create tokener for POST");
					afb_hreq_reply_error(hreq, MHD_HTTP_INTERNAL_SERVER_ERROR);
				}
				return MHD_YES;
                        } else {
				WARNING("Unsupported media type %s", type);
				afb_hreq_reply_error(hreq, MHD_HTTP_UNSUPPORTED_MEDIA_TYPE);
				return MHD_YES;
			}
		}
	}

	/* process further data */
	if (*upload_data_size) {
		if (hreq->postform != NULL) {
			if (!MHD_post_process (hreq->postform, upload_data, *upload_data_size)) {
				ERROR("error in POST processor");
				afb_hreq_reply_error(hreq, MHD_HTTP_INTERNAL_SERVER_ERROR);
				return MHD_YES;
			}
		} else if (hreq->tokener) {
			hreq->json = json_tokener_parse_ex(hreq->tokener, upload_data, (int)*upload_data_size);
			jerr = json_tokener_get_error(hreq->tokener);
			if (jerr == json_tokener_continue) {
				hreq->json = json_tokener_parse_ex(hreq->tokener, "", 1);
				jerr = json_tokener_get_error(hreq->tokener);
			}
			if (jerr != json_tokener_success) {
				ERROR("error in POST json: %s", json_tokener_error_desc(jerr));
				afb_hreq_reply_error(hreq, MHD_HTTP_BAD_REQUEST);
				return MHD_YES;
			}
		}
		*upload_data_size = 0;
		return MHD_YES;
	}

	/* flush the data */
	if (hreq->postform != NULL) {
		rc = MHD_destroy_post_processor(hreq->postform);
		hreq->postform = NULL;
		if (rc == MHD_NO) {
			ERROR("error detected in POST processing");
			afb_hreq_reply_error(hreq, MHD_HTTP_BAD_REQUEST);
			return MHD_YES;
		}
	}
	if (hreq->tokener != NULL) {
		json_tokener_free(hreq->tokener);
		hreq->tokener = NULL;
	}

	if (hreq->scanned != 0) {
		if (hreq->replied == 0 && hreq->suspended == 0) {
			MHD_suspend_connection (connection);
			hreq->suspended = 1;
		}
		return MHD_YES;
	}

	/* search an handler for the request */
	hreq->scanned = 1;
	iter = hsrv->handlers;
	while (iter) {
		if (afb_hreq_unprefix(hreq, iter->prefix, iter->length)) {
			if (iter->handler(hreq, iter->data)) {
				if (hreq->replied == 0 && hreq->suspended == 0) {
					MHD_suspend_connection (connection);
					hreq->suspended = 1;
				}
				return MHD_YES;
			}
			hreq->tail = hreq->url;
			hreq->lentail = hreq->lenurl;
		}
		iter = iter->next;
	}

	/* no handler */
	WARNING("Unhandled request to %s", hreq->url);
	afb_hreq_reply_error(hreq, MHD_HTTP_NOT_FOUND);
	return MHD_YES;
}

/* Because of POST call multiple time requestApi we need to free POST handle here */
static void end_handler(void *cls, struct MHD_Connection *connection, void **recordreq,
			enum MHD_RequestTerminationCode toe)
{
	fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	struct afb_hreq *hreq;

	hreq = *recordreq;
	if (hreq) {
		afb_hreq_unref(hreq);
	}
}

static void do_run(int signum, void *arg)
{
	    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	MHD_UNSIGNED_LONG_LONG to;
	struct afb_hsrv *hsrv = arg;

	if (!signum) {
		do { MHD_run(hsrv->httpd); } while(MHD_get_timeout(hsrv->httpd, &to) == MHD_YES && !to);
	}
	fdev_set_events(hsrv->fdev, EPOLLIN);
}

void afb_hsrv_run(struct afb_hsrv *hsrv)
{
	    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	fdev_set_events(hsrv->fdev, 0);
	if (jobs_queue(hsrv, 0, do_run, hsrv) < 0)
		do_run(0, hsrv);
}

static void listen_callback(void *hsrv, uint32_t revents, struct fdev *fdev)
{
	    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	afb_hsrv_run(hsrv);
}

static int new_client_handler(void *cls, const struct sockaddr *addr, socklen_t addrlen)
{
	return MHD_YES;
}

static struct hsrv_handler *new_handler(
		struct hsrv_handler *head,
		const char *prefix,
		int (*handler) (struct afb_hreq *, void *),
		void *data,
		int priority)
{
	struct hsrv_handler *link, *iter, *previous;
	size_t length;
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	/* get the length of the prefix without its leading / */
	length = strlen(prefix);
	while (length && prefix[length - 1] == '/')
		length--;

	/* allocates the new link */
	link = malloc(sizeof *link);
	if (link == NULL)
		return NULL;

	/* initialize it */
	link->prefix = prefix;
	link->length = length;
	link->handler = handler;
	link->data = data;
	link->priority = priority;

	/* adds it */
	previous = NULL;
	iter = head;
	while (iter && (priority < iter->priority || (priority == iter->priority && length <= iter->length))) {
		previous = iter;
		iter = iter->next;
	}
	link->next = iter;
	if (previous == NULL)
		return link;
	previous->next = link;
	return head;
}

static int handle_alias(struct afb_hreq *hreq, void *data)
{
	int rc;
	struct hsrv_alias *da = data;
	struct locale_search *search;
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	if (hreq->method != afb_method_get) {
		if (da->relax)
			return 0;
		afb_hreq_reply_error(hreq, MHD_HTTP_METHOD_NOT_ALLOWED);
		return 1;
	}

	search = locale_root_search(da->root, hreq->lang, 0);
	rc = afb_hreq_reply_locale_file_if_exist(hreq, search, &hreq->tail[1]);
	locale_search_unref(search);
	if (rc == 0) {
		if (da->relax)
			return 0;
		afb_hreq_reply_error(hreq, MHD_HTTP_NOT_FOUND);
	}
	return 1;
}

int afb_hsrv_add_handler(
		struct afb_hsrv *hsrv,
		const char *prefix,
		int (*handler) (struct afb_hreq *, void *),
		void *data,
		int priority)
{
	struct hsrv_handler *head;
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	head = new_handler(hsrv->handlers, prefix, handler, data, priority);
	if (head == NULL)
		return 0;
	hsrv->handlers = head;
	return 1;
}

int afb_hsrv_add_alias_root(struct afb_hsrv *hsrv, const char *prefix, struct locale_root *root, int priority, int relax)
{
	struct hsrv_alias *da;
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	da = malloc(sizeof *da);
	if (da != NULL) {
		da->root = root;
		da->relax = relax;
		if (afb_hsrv_add_handler(hsrv, prefix, handle_alias, da, priority)) {
			locale_root_addref(root);
			return 1;
		}
		free(da);
	}
	return 0;
}

int afb_hsrv_add_alias(struct afb_hsrv *hsrv, const char *prefix, int dirfd, const char *alias, int priority, int relax)
{
	struct locale_root *root;
	int rc;
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	root = locale_root_create_at(dirfd, alias);
	if (root == NULL) {
		ERROR("can't connect to directory %s: %m", alias);
		rc = 0;
	} else {
		rc = afb_hsrv_add_alias_root(hsrv, prefix, root, priority, relax);
		locale_root_unref(root);
	}
	return rc;
}

int afb_hsrv_set_cache_timeout(struct afb_hsrv *hsrv, int duration)
{
	int rc;
	char *dur;
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	rc = asprintf(&dur, "%d", duration);
	if (rc < 0)
		return 0;

	free(hsrv->cache_to);
	hsrv->cache_to = dur;
	return 1;
}

int afb_hsrv_start(struct afb_hsrv *hsrv, uint16_t port, unsigned int connection_timeout)
{
	struct fdev *fdev;
	struct MHD_Daemon *httpd;
	const union MHD_DaemonInfo *info;
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	httpd = MHD_start_daemon(
		MHD_USE_EPOLL | MHD_ALLOW_UPGRADE | MHD_USE_TCP_FASTOPEN | MHD_USE_DEBUG | MHD_USE_SUSPEND_RESUME,
		port,				/* port */
		new_client_handler, NULL,	/* Tcp Accept call back + extra attribute */
		access_handler, hsrv,	/* Http Request Call back + extra attribute */
		MHD_OPTION_NOTIFY_COMPLETED, end_handler, hsrv,
		MHD_OPTION_CONNECTION_TIMEOUT, connection_timeout,
		MHD_OPTION_END);	/* options-end */

	if (httpd == NULL) {
		ERROR("httpStart invalid httpd port: %d", (int)port);
		return 0;
	}

	info = MHD_get_daemon_info(httpd, MHD_DAEMON_INFO_EPOLL_FD_LINUX_ONLY);
	if (info == NULL) {
		MHD_stop_daemon(httpd);
		ERROR("httpStart no pollfd");
		return 0;
	}

	fdev = afb_fdev_create(info->listen_fd);
	if (fdev == NULL) {
		MHD_stop_daemon(httpd);
		ERROR("connection to events for httpd failed");
		return 0;
	}
	fdev_set_autoclose(fdev, 0);
	fdev_set_events(fdev, EPOLLIN);
	fdev_set_callback(fdev, listen_callback, hsrv);

	hsrv->httpd = httpd;
	hsrv->fdev = fdev;
	return 1;
}

void afb_hsrv_stop(struct afb_hsrv *hsrv)
{
	    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	if (hsrv->fdev != NULL) {
		fdev_unref(hsrv->fdev);
		hsrv->fdev = NULL;
	}
	if (hsrv->httpd != NULL)
		MHD_stop_daemon(hsrv->httpd);
	hsrv->httpd = NULL;
}

struct afb_hsrv *afb_hsrv_create()
{
    fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	struct afb_hsrv *result = calloc(1, sizeof(struct afb_hsrv));
	if (result != NULL)
		result->refcount = 1;
	return result;
}

void afb_hsrv_put(struct afb_hsrv *hsrv)
{
	fprintf(stderr, "NETWORK : [%d] %s %s %d\r\n", (int)getpid(), __FILE__, __FUNCTION__, __LINE__);
	assert(hsrv->refcount != 0);
	if (!--hsrv->refcount) {
		afb_hsrv_stop(hsrv);
		free(hsrv);
	}
}

