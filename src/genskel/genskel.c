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
/*
 * This simple program expands the object { "$ref": "#/path/to/a/target" }
 *
 * For example:
 *
 *  {
 *    "type":{
 *      "a": "int",
 *      "b": { "$ref": "#/type/a" }
 *    }
 *  }
 *
 * will be exapanded to
 *
 *  {
 *    "type":{
 *      "a": "int",
 *      "b": "int"
 *    }
 *  }
 * 
 * Invocation:   program  [file|-]...
 *
 * without arguments, it reads the input.
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <json-c/json.h>

#define oom(x) do{if(!(x)){fprintf(stderr,"out of memory\n");exit(1);}}while(0)

/**
 * records path to the expanded node
 */
struct path
{
	struct json_object *object;	/**< node being expanded */
	struct path *upper;		/**< link to upper expanded nodes */
};

/**
 * root of the JSON being parsed
 */
struct json_object *root = NULL;
struct json_object *d_perms = NULL;
struct json_object *a_perms = NULL;
const char *preinit = NULL;
const char *init = NULL;
const char *onevent = NULL;
const char *api = NULL;
const char *scope = NULL;
const char *prefix = NULL;
const char *postfix = NULL;
int priv = -1;
int noconc = -1;

/**
 * Search for a reference of type "#/a/b/c" int the
 * parsed JSON object
 */
struct json_object *search(const char *path)
{
	char *d;
	struct json_object *i;

	/* does it match #/ at the beginning? */
	if (path[0] != '#' || (path[0] && path[1] != '/'))
		return NULL;

	/* search from root to target */
	i = root;
	d = strdupa(path+2);
	d = strtok(d, "/");
	while(i && d) {
		if (!json_object_object_get_ex(i, d, &i))
			return NULL; 
		d = strtok(NULL, "/");
	}
	return i;
}

/**
 * Expands the node designated by path and returns its expanded form
 */
struct json_object *expand_$ref(struct path path)
{
	struct path *p;
	struct json_object *o, *x;
	int n, i;
	struct json_object_iterator ji, jn; 

	/* expansion depends of the type of the node */
	switch (json_object_get_type(path.object)) {
	case json_type_object:
		/* for object, look if it contains a property "$ref" */
		if (json_object_object_get_ex(path.object, "$ref", &o)) {
			/* yes, reference, try to substitute its target */
			if (!json_object_is_type(o, json_type_string)) {
				fprintf(stderr, "found a $ref not being string. Is: %s\n", json_object_get_string(o));
				exit(1);
			}
			x = search(json_object_get_string(o));
			if (!x) {
				fprintf(stderr, "$ref not found. Was: %s\n", json_object_get_string(o));
				exit(1);
			}
			p = &path;
			while(p) {
				if (x == p->object) {
					fprintf(stderr, "$ref recursive. Was: %s\n", json_object_get_string(o));
					exit(1);
				}
				p = p->upper;
			}
			/* cool found, return a new instance of the target */
			return json_object_get(x);
		}
		/* no, expand the values */
		ji = json_object_iter_begin(path.object);
		jn = json_object_iter_end(path.object);
		while (!json_object_iter_equal(&ji, &jn)) {
			o = json_object_iter_peek_value(&ji);
			x = expand_$ref((struct path){ .object = o, .upper = &path });
			if (x != o)
				json_object_object_add(path.object, json_object_iter_peek_name(&ji), x);
			json_object_iter_next(&ji);
		}
		break;
	case json_type_array:
		/* expand the values of arrays */
		i = 0;
		n = json_object_array_length(path.object);
		while (i != n) {
			o = json_object_array_get_idx(path.object, i);
			x = expand_$ref((struct path){ .object = o, .upper = &path });
			if (x != o)
				json_object_array_put_idx(path.object, i, x);
			i++;
		}
		break;
	default:
		/* otherwise no expansion */
		break;
	}

	/* return the given node */
	return path.object;
}


char *make_desc(struct json_object *o)
{
	const char *a, *b;
	char *desc, c, buf[3];
	size_t len;
	int i, pos, e;

	a = b = json_object_to_json_string_ext(root, 0);
	len = 1;
	while((c = *b++)) {
		len += 1 + ('"' == c);
	}

	len += 7 * (1 + len / 72);
	desc = malloc(len);
	oom(desc);

	len = pos = 0;
	b = a;
	while((c = *b++)) {
		if (c == '"') {
			buf[0] = '\\';
			buf[1] = '"';
			buf[2] = 0;
		}
		else if (c == '\\') {
			switch ((c = *b++)) {
			case 0:
				b--;
				break;
			case '/':
				buf[0] = '/';
				buf[1] = 0;
				break;
			default:
				buf[0] = '\\';
				buf[1] = c;
				buf[2] = 0;
				break;
			}
		}
		else {
			buf[0] = c;
			buf[1] = 0;
		}
		i = e = 0;
		while (buf[i]) {
			if (pos >= 77 && !e) {
				desc[len++] = '"';
				desc[len++] = '\n';
				pos = 0;
			}
			if (pos == 0) {
				desc[len++] = ' ';
				desc[len++] = ' ';
				desc[len++] = ' ';
				desc[len++] = ' ';
				desc[len++] = '"';
				pos = 5;
			}
			c = buf[i++];
			desc[len++] = c;
			e = !e && c == '\\';
			pos++;
		}
	}
	desc[len++] = '"';
	desc[len++] = '\n';
	desc[len] = 0;
	return desc;
}

struct json_object *permissions_of_verb(struct json_object *obj)
{
	struct json_object *x, *y;

	if (json_object_object_get_ex(obj, "x-permissions", &x))
		return x;

	if (json_object_object_get_ex(obj, "get", &x))
		if (json_object_object_get_ex(x, "x-permissions", &y))
			return y;

	return NULL;
}

void print_perms()
{
	int i, n;

	n = a_perms ? json_object_array_length(a_perms) : 0;
	if (n) {
		printf("static const struct afb_auth _afb_auths_v2_%s[] = {\n" , api);
		i = 0;
		while (i < n) {
			printf("\t{ %s }", json_object_get_string(json_object_array_get_idx(a_perms, i)));
			printf(",\n"+(++i == n));
		}
		printf("};\n\n");
	}
}

struct json_object *new_perm(struct json_object *obj, const char *desc)
{
	const char *tag;
	char *b;
	struct json_object *x, *y;

	tag = obj ? json_object_to_json_string_ext(obj, 0) : desc;
	if (!json_object_object_get_ex(d_perms, tag, &y)) {
		if (!d_perms) {
			d_perms = json_object_new_object();
			a_perms = json_object_new_array();
		}	

		asprintf(&b, "&_afb_auths_v2_%s[%d]", api, json_object_array_length(a_perms));
		x = json_object_new_string(desc);
		y = json_object_new_string(b);
		json_object_array_add(a_perms, x);
		json_object_object_add(d_perms, tag, y);
		free(b);
	}
	return y;
}

struct json_object *decl_perm(struct json_object *obj);

struct json_object *decl_perm_a(const char *op, struct json_object *obj)
{
	int i, n;
	char *a;
	struct json_object *x, *y;

	x = NULL;
	i = n = obj ? json_object_array_length(obj) : 0;
	while (i) {
		y = decl_perm(json_object_array_get_idx(obj, --i));
		if (!y)
			;
		else if (!x)
			x = y;
		else if (x != y) {
			asprintf(&a, ".type = afb_auth_%s, .first = %s, .next = %s",
				 op, json_object_get_string(y), json_object_get_string(x));
			x = new_perm(NULL, a);
			free(a);
		}
	}
	return x;
}

struct json_object *decl_perm(struct json_object *obj)
{
	char *a;
	struct json_object *x, *y;

	if (json_object_object_get_ex(d_perms, json_object_to_json_string_ext(obj, 0), &x))
		return x;

	if (json_object_object_get_ex(obj, "permission", &x)) {
		asprintf(&a, ".type = afb_auth_Permission, .text = \"%s\"", json_object_get_string(x));
		y = new_perm(obj, a);
		free(a);
	}
	else if (json_object_object_get_ex(obj, "anyOf", &x)) {
		y = decl_perm_a("Or", x);
	}
	else if (json_object_object_get_ex(obj, "allOf", &x)) {
		y = decl_perm_a("And", x);
	}
	else if (json_object_object_get_ex(obj, "not", &x)) {
		x = decl_perm(x);
		asprintf(&a, ".type = afb_auth_Not, .first = %s", json_object_get_string(x));
		y = new_perm(obj, a);
		free(a);
	}
	else if (json_object_object_get_ex(obj, "LOA", &x))
		y = NULL;
	else if (json_object_object_get_ex(obj, "session", &x))
		y = NULL;
	else
		y = NULL;

	return y;
}

void declare_permissions(const char *name, struct json_object *obj)
{
	struct json_object *p;

	p = permissions_of_verb(obj);
	if (p)
		decl_perm(p);
}


#define SESSION_CLOSE  0x000001
#define SESSION_RENEW  0x000010
#define SESSION_CHECK  0x000100
#define SESSION_LOA_1  0x001000
#define SESSION_LOA_2  0x011000
#define SESSION_LOA_3  0x111000
#define SESSION_MASK   0x111111


int get_session(struct json_object *obj);

int get_session_a(int and, struct json_object *obj)
{
	int i, n, x, y;

	n = obj ? json_object_array_length(obj) : 0;
	if (n == 0)
		return 0;

	i = n;
	x = get_session(json_object_array_get_idx(obj, --i));
	while (i) {
		y = get_session(json_object_array_get_idx(obj, --i));
		if (and)
			x &= y;
		else
			x |= y;
	}
	return x;
}

int get_session(struct json_object *obj)
{
	int y;
	const char *a;
	struct json_object *x;

	y = 0;
	if (json_object_object_get_ex(obj, "anyOf", &x)) {
		y = get_session_a(1, x);
	}
	else if (json_object_object_get_ex(obj, "allOf", &x)) {
		y = get_session_a(0, x);
	}
	else if (json_object_object_get_ex(obj, "not", &x)) {
		y = ~get_session(x) & SESSION_MASK;
	}
	else if (json_object_object_get_ex(obj, "LOA", &x)) {
		switch (json_object_get_int(x)) {
		case 3: y = SESSION_LOA_3; break;
		case 2: y = SESSION_LOA_2; break;
		case 1: y = SESSION_LOA_1; break;
		default: break;
		}
	}
	else if (json_object_object_get_ex(obj, "session", &x)) {
		a = json_object_get_string(x);
		if (!strcmp(a, "check"))
			y = SESSION_CHECK;
		else if (!strcmp(a, "close"))
			y = SESSION_CLOSE;
	}
	else if (json_object_object_get_ex(obj, "token", &x)) {
		a = json_object_get_string(x);
		if (!strcmp(a, "refresh"))
			y = SESSION_RENEW;
	}

	return y;
}

void print_session(struct json_object *p)
{
	int s, c, l;

	s = p ? get_session(p) : 0;
	c = 1;
	if (s & SESSION_CHECK) {
		printf("%s", "|AFB_SESSION_CHECK_V2" + c);
		c = 0;
	}
	if (s & SESSION_LOA_3 & ~SESSION_LOA_2)
		l = 3;
	else if (s & SESSION_LOA_2 & ~SESSION_LOA_1)
		l = 2;
	else if (s & SESSION_LOA_1)
		l = 1;
	else
		l = 0;
	if (l) {
		printf("%s%d_V2", "|AFB_SESSION_LOA_" + c, l);
		c = 0;
	}
	if (s & SESSION_CLOSE) {
		printf("%s", "|AFB_SESSION_CLOSE_V2" + c);
		c = 0;
	}
	if (s & SESSION_RENEW) {
		printf("%s", "|AFB_SESSION_REFRESH_V2" + c);
		c = 0;
	}
	if (c)
		printf("AFB_SESSION_NONE_V2");
}

void print_verb(const char *name)
{
	printf("%s%s%s" , prefix, name, postfix);
}

void print_declare_verb(const char *name, struct json_object *obj)
{
	printf("%s void ", scope);
	print_verb(name);
	printf("(struct afb_req req);\n");
}

void print_struct_verb(const char *name, struct json_object *obj)
{
	struct json_object *p;

	p = permissions_of_verb(obj);
	printf(
		"    {\n"
		"        .verb = \"%s\",\n"
		"        .callback = "
		, name
	);
	print_verb(name);
	printf(
		",\n"
		"        .auth = %s,\n"
		"        .info = NULL,\n"
		"        .session = "
		, p ? json_object_get_string(decl_perm(p)) : "NULL"
	);
	print_session(p);
	printf(
		"\n"
		"    },\n"
	);
}

void enum_verbs(void (*func)(const char *name, struct json_object *obj))
{
	struct json_object_iterator ji, jn;
	struct json_object *paths, *obj;
	const char *name;

	/* search the verbs */
	paths = search("#/paths");
	if (!paths)
		return;

	/* list the verbs and sort it */
	ji = json_object_iter_begin(paths);
	jn = json_object_iter_end(paths);
	while (!json_object_iter_equal(&ji, &jn)) {
		name = json_object_iter_peek_name(&ji);
		obj = json_object_iter_peek_value(&ji);
		name += (*name == '/');
		func(name, obj);
		json_object_iter_next(&ji);
	}
}

void getvarbool(int *var, const char *path, int defval)
{
	struct json_object *o;

	if (*var != 0 && *var != 1) {
		o = search(path);
		if (o && json_object_is_type(o, json_type_boolean))
			*var = json_object_get_boolean(o);
		else
			*var = !!defval;
	}
}

void getvar(const char **var, const char *path, const char *defval)
{
	struct json_object *o;

	if (!*var) {
		o = search(path);
		if (o && json_object_is_type(o, json_type_string))
			*var = json_object_get_string(o);
		else
			*var = defval;
	}
}

/**
 * process a file and prints its expansion on stdout
 */
void process(char *filename)
{
	char *desc;

	/* translate - */
	if (!strcmp(filename, "-"))
		filename = "/dev/stdin";

	/* check access */
	if (access(filename, R_OK)) {
		fprintf(stderr, "can't access file %s\n", filename);
		exit(1);
	}

	/* read the file */
	root = json_object_from_file(filename);
	if (!root) {
		fprintf(stderr, "reading file %s produced null\n", filename);
		exit(1);
	}

	/* create the description */
	desc = make_desc(root);

	/* expand references */
	root = expand_$ref((struct path){ .object = root, .upper = NULL });

	/* get some names */
	getvar(&api, "#/info/x-binding-c-generator/api", NULL);
	getvar(&preinit, "#/info/x-binding-c-generator/preinit", NULL);
	getvar(&init, "#/info/x-binding-c-generator/init", NULL);
	getvar(&onevent, "#/info/x-binding-c-generator/onevent", NULL);
	getvar(&scope, "#/info/x-binding-c-generator/scope", "static");
	getvar(&prefix, "#/info/x-binding-c-generator/prefix", "afb_verb_");
	getvar(&postfix, "#/info/x-binding-c-generator/postfix", "_cb");
	getvarbool(&priv, "#/info/x-binding-c-generator/private", 0);
	getvarbool(&noconc, "#/info/x-binding-c-generator/noconcurrency", 0);
	getvar(&api, "#/info/title", "?");

	/* get the API name */
	printf(
		"\n"
		"static const char _afb_description_v2_%s[] =\n"
		"%s"
		";\n"
		"\n"
		, api, desc
	);
	enum_verbs(declare_permissions);
	print_perms();
	enum_verbs(print_declare_verb);
	printf(
		"\n"
		"static const struct afb_verb_v2 _afb_verbs_v2_%s[] = {\n"
                , api
	);
	enum_verbs(print_struct_verb);
	printf(
		"    { .verb = NULL }\n"
		"};\n"
	);
	printf(
		"\n"
		"%sconst struct afb_binding_v2 %s%s = {\n"
		"    .api = \"%s\",\n"
		"    .specification = _afb_description_v2_%s,\n"
		"    .info = NULL,\n"
		"    .verbs = _afb_verbs_v2_%s,\n"
		"    .preinit = %s,\n"
		"    .init = %s,\n"
		"    .onevent = %s,\n"
		"    .noconcurrency = %d\n"
		"};\n"
		"\n"
		, priv ? "static " : ""
		, priv ? "_afb_binding_v2_" : "afbBindingV2"
		, priv ? api : ""
		, api
		, api
		, api
		, preinit ?: "NULL"
		, init ?: "NULL"
		, onevent ?: "NULL"
		, !!noconc
	);

	/* clean up */
	json_object_put(root);
	free(desc);
}

/** process the list of files or stdin if none */
int main(int ac, char **av)
{
	if (!*++av)
		process("-");
	else {
		do { process(*av); } while(*++av);
	}	
	return 0;
}





