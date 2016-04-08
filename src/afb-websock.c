/*
 * Copyright 2016 IoT.bzh
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
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <openssl/sha.h>
#include <microhttpd.h>

#include "afb-ws-json.h"

#include "afb-method.h"
#include "afb-hreq.h"
#include "afb-websock.h"

/**************** WebSocket connection upgrade ****************************/

static const char websocket_s[] = "websocket";
static const char sec_websocket_key_s[] = "Sec-WebSocket-Key";
static const char sec_websocket_version_s[] = "Sec-WebSocket-Version";
static const char sec_websocket_accept_s[] = "Sec-WebSocket-Accept";
static const char sec_websocket_protocol_s[] = "Sec-WebSocket-Protocol";
static const char websocket_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static void enc64(unsigned char *in, char *out)
{
	static const char tob64[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	out[0] = tob64[in[0] >> 2];
	out[1] = tob64[((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)];
	out[2] = tob64[((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)];
	out[3] = tob64[in[2] & 0x3f];
}

static void make_accept_value(const char *key, char result[29])
{
	unsigned char md[SHA_DIGEST_LENGTH+1];
	size_t len = strlen(key);
	char *buffer = alloca(len + sizeof websocket_guid - 1);
	memcpy(buffer, key, len);
	memcpy(buffer + len, websocket_guid, sizeof websocket_guid - 1);
	SHA1((const unsigned char *)buffer, (unsigned long)(len + sizeof websocket_guid - 1), md);
	assert(SHA_DIGEST_LENGTH == 20);
	md[20] = 0;
	enc64(&md[0], &result[0]);
	enc64(&md[3], &result[4]);
	enc64(&md[6], &result[8]);
	enc64(&md[9], &result[12]);
	enc64(&md[12], &result[16]);
	enc64(&md[15], &result[20]);
	enc64(&md[18], &result[24]);
	result[27] = '=';
	result[28] = 0;
}

static const char vseparators[] = " \t,";

static int headerhas(const char *header, const char *needle)
{
	size_t len, n;

	n = strlen(needle);
	for(;;) {
		header += strspn(header, vseparators);
		if (!*header)
			return 0;
		len = strcspn(header, vseparators);
		if (n == len && 0 == strncasecmp(needle, header, n))
			return 1;
		header += len;
	}
}

struct protodef
{
	const char *name;
	void *(*create)(int fd, struct AFB_clientCtx *context, void (*cleanup)(void*), void *closure);
};

static const struct protodef protodefs[] = {
	{ "x-afb-ws-json1",	(void*)afb_ws_json_create },
	{ NULL, NULL }
};

static const struct protodef *search_proto(const char *protocols)
{
	int i;
	size_t len;

	for(;;) {
		protocols += strspn(protocols, vseparators);
		if (!*protocols)
			return NULL;
		len = strcspn(protocols, vseparators);
		for (i = 0 ; protodefs[i].name != NULL ; i++)
			if (!strncasecmp(protodefs[i].name, protocols, len)
			 && !protodefs[i].name[len])
				return &protodefs[i];
		protocols += len;
	}
}

int afb_websock_check_upgrade(struct afb_hreq *hreq)
{
	const char *connection, *upgrade, *key, *version, *protocols;
	char acceptval[29];
	int vernum;
	struct MHD_Response *response;
	const struct protodef *proto;
	void *ws;

	/* is an upgrade to websocket ? */
	upgrade = afb_hreq_get_header(hreq, MHD_HTTP_HEADER_UPGRADE);
	if (upgrade == NULL || strcasecmp(upgrade, websocket_s))
		return 0;

	/* is a connection for upgrade ? */
	connection = afb_hreq_get_header(hreq, MHD_HTTP_HEADER_CONNECTION);
	if (connection == NULL
         || !headerhas (connection, MHD_HTTP_HEADER_UPGRADE))
		return 0;

	/* is a get ? */
	if (hreq->method != afb_method_get
	 || strcasecmp(hreq->version, MHD_HTTP_VERSION_1_1))
		return 0;

	/* has a key and a version ? */
	key = afb_hreq_get_header(hreq, sec_websocket_key_s);
	version = afb_hreq_get_header(hreq, sec_websocket_version_s);
	if (key == NULL || version == NULL)
		return 0;

	/* is a supported version ? */
	vernum = atoi(version);
	if (vernum != 13) {
		response = MHD_create_response_from_buffer(0, NULL,
						MHD_RESPMEM_PERSISTENT);
		MHD_add_response_header (response, sec_websocket_version_s,
						"13");
		MHD_queue_response (hreq->connection, MHD_HTTP_BAD_REQUEST,
						response);
		MHD_destroy_response (response);
		return 1;
	}

	/* is the protocol supported ? */
	protocols = afb_hreq_get_header(hreq, sec_websocket_protocol_s);
	proto = protocols == NULL ? NULL : search_proto(protocols);
	if (proto == NULL) {
		afb_hreq_reply_error(hreq, MHD_HTTP_PRECONDITION_FAILED);
		return 1;
	}

	/* create the web socket */
	/* TODO fullfil ondisconnection and records!!! */
	ws = proto->create(dup(MHD_get_connection_info(hreq->connection,MHD_CONNECTION_INFO_CONNECTION_FD)->connect_fd),
			afb_hreq_context(hreq),
			(void*)MHD_resume_connection,
			hreq->connection);
	if (ws == NULL) {
		afb_hreq_reply_error(hreq, MHD_HTTP_INTERNAL_SERVER_ERROR);
		return 1;
	}

	/* send the accept connection */
	make_accept_value(key, acceptval);
	response = MHD_create_response_from_buffer(0, NULL,
						MHD_RESPMEM_PERSISTENT);
	MHD_add_response_header (response, sec_websocket_accept_s, acceptval);
	MHD_add_response_header (response, sec_websocket_protocol_s, 
								proto->name);
	MHD_add_response_header (response, MHD_HTTP_HEADER_CONNECTION,
						MHD_HTTP_HEADER_UPGRADE);
	MHD_add_response_header (response, MHD_HTTP_HEADER_UPGRADE,
								websocket_s);
	MHD_queue_response (hreq->connection, MHD_HTTP_SWITCHING_PROTOCOLS,
								response);
	MHD_destroy_response (response);

	hreq->upgrade = 1;
	return 1;
}

