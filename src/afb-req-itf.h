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

struct afb_arg {
	const char *name;
	const char *value;
	size_t size;
	int is_file;
};

struct afb_req_itf {
	struct afb_arg (*get)(void *data, const char *name);
	void (*iterate)(void *data, int (*iterator)(void *closure, struct afb_arg arg), void *closure);
};

struct afb_req {
	const struct afb_req_itf *itf;
	void *data;
};

static inline struct afb_arg afb_req_get(struct afb_req req, const char *name)
{
	return req.itf->get(req.data, name);
}

static inline const char *afb_req_argument(struct afb_req req, const char *name)
{
	return afb_req_get(req, name).value;
}

static inline int afb_req_is_argument_file(struct afb_req req, const char *name)
{
	return afb_req_get(req, name).is_file;
}

static inline void afb_req_iterate(struct afb_req req, int (*iterator)(void *closure, struct afb_arg arg), void *closure)
{
	req.itf->iterate(req.data, iterator, closure);
}

