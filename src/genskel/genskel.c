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
struct json_object *root;
const char *init = NULL;
const char *start = NULL;
const char *onevent = NULL;
const char *api = NULL;
const char *scope = NULL;
const char *prefix = NULL;
const char *postfix = NULL;

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

char *get_perm_string(struct json_object *o, int *pl)
{
	struct json_object *x, *y;
	char *a, *b, *c;
	int l, n, i, L;

	/* value for permission */
	if (json_object_object_get_ex(o, "permission", &x)) {
		if (!json_object_is_type(x, json_type_string)) {
			fprintf(stderr, "permission must be a string. Was: %s\n", json_object_get_string(o));
			exit(1);
		}
		a = strdup(json_object_get_string(x));
		oom(a);
		*pl = 0;
		return a;
	}

	/* value for not */
	if (json_object_object_get_ex(o, "not", &x)) {
		a = get_perm_string(x, &l);
		b = malloc(6 + strlen(a));
		oom(b);
		if (l)
			stpcpy(stpcpy(stpcpy(b,"not("),a),")");
		else
			stpcpy(stpcpy(b,"not "),a);
		free(a);
		*pl = 0;
		return b;
	}

	/* value for and or or */
	if (json_object_object_get_ex(o, "allOf", &x))
		L = 1;
	else if (json_object_object_get_ex(o, "anyOf", &x))
		L = 2;
	else {
		fprintf(stderr, "unrecognized permission. Was: %s\n", json_object_get_string(o));
		exit(1);
	}

	/* check the array */
	if (!json_object_is_type(x, json_type_array)) {
		fprintf(stderr, "sequence must be an array. Was: %s\n", json_object_get_string(o));
		exit(1);
	}
	n = json_object_array_length(x);
	if (n == 0) {
		fprintf(stderr, "invalid empty sequence. Was: %s\n", json_object_get_string(o));
		exit(1);
	}

	/* process the array */
	if (n == 1)
		return get_perm_string(json_object_array_get_idx(x, 0), pl);
	b = NULL;
	i = 0;
	while (i != n) {
		y = json_object_array_get_idx(x, i);
		a = get_perm_string(y, &l);
		if (l > L) {
			c = malloc(3 + strlen(a));
			oom(c);
			stpcpy(stpcpy(stpcpy(c,"("),a),")");
			free(a);
			a = c;
		}
		if (!b)
			b = a;
		else {
			c = malloc(6 + strlen(a) + strlen(b));
			oom(c);
			stpcpy(stpcpy(stpcpy(c,b),L==2?" or ":" and "),a);
			free(a);
			free(b);
			b = c;
		}
		i++;
	}
	*pl = L;
	return b;
}

struct json_object *make_perm(struct json_object *o)
{
	int l;
	char *permstr = get_perm_string(o, &l);
	return json_object_new_string(permstr);
}

void make_permissions(struct path path)
{
	struct json_object *o, *x, *y;
	struct json_object_iterator ji, jn; 

	if (json_object_object_get_ex(path.object, "permissions", &o)) {

		/* expand $refs of permissions */
		x = expand_$ref((struct path){ .object = o, .upper = &path });
		if (x != o)
			json_object_object_add(path.object, "permissions", x);

		/* makes the permissions */
		ji = json_object_iter_begin(o);
		jn = json_object_iter_end(o);
		while (!json_object_iter_equal(&ji, &jn)) {
			x = json_object_iter_peek_value(&ji);
			y = make_perm(x);
			if (x != y)
				json_object_object_add(o, json_object_iter_peek_name(&ji), y);
			json_object_iter_next(&ji);
		}
	}
}

char *make_desc(struct json_object *o)
{
	const char *a, *b;
	char *desc, c, buf[3];
	size_t len;
	int i, pos;

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
		i = 0;
		while (buf[i]) {
			if (pos == 77) {
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
			desc[len++] = buf[i++];
			pos++;
		}
	}
	desc[len++] = '"';
	desc[len++] = '\n';
	desc[len] = 0;
	return desc;
}

void print_verbs(int real)
{
	struct json_object_iterator ji, jn;
	struct json_object *o, *v, *x, *y;
	const char *verb, *perm, *loa;
	int i, n, l;
	const char **verbs;

	/* search the verbs */
	o = search("#/verbs");
	if (!o)
		return;

	/* list the verbs and sort it */
	n = json_object_object_length(o);
	verbs = malloc(n * sizeof *verbs);
	oom(verbs);
	n = 0;
	ji = json_object_iter_begin(o);
	jn = json_object_iter_end(o);
	while (!json_object_iter_equal(&ji, &jn)) {
		verb = json_object_iter_peek_name(&ji);
		i = 0;
		while (i < n && strcasecmp(verb, verbs[i]) > 0)
			i++;
		if (i < n)
			memmove(verbs + i + 1, verbs + i, (n - i) * sizeof *verbs);
		verbs[i] = verb;
		n++;
		json_object_iter_next(&ji);
	}

	/* emit the verbs */
	for (i = 0 ; i < n ; i++) {
		verb = verbs[i];
		json_object_object_get_ex(o, verb, &v);

		if (real) {
			if (!json_object_object_get_ex(v, "permissions", &x))
				perm = "NULL";
			else {
				perm = json_object_to_json_string(x);
			}
			l = 0;
			loa = "";
			if (json_object_object_get_ex(v, "LOA", &x)) {
				if (json_object_is_type(x, json_type_int)) {
					loa = "AFB_SESSION_LOA_EQ_";
					y = x;
				} else {
					if (json_object_object_get_ex(x, "minimum", &y))
						loa = "AFB_SESSION_LOA_GE_";
					else if (json_object_object_get_ex(x, "maximum", &y))
						loa = "AFB_SESSION_LOA_LE_";
					else
						y = NULL;
					if (y && !json_object_is_type(y, json_type_int))
						y = NULL;
				}
				l = json_object_get_int(y);
				if (y == NULL || l < 0 || l > 3) {
					fprintf(stderr, "invalid LOA spec. Was: %s",  json_object_get_string(x));
					exit(1);
				}
			}

			printf(
				"    {\n"
				"        .verb = \"%s\",\n"
				"        .callback = %s%s%s,\n"
				"        .permissions = %s,\n"
				"        .session = %s%d,\n"
				"    },\n"
				, verb, prefix, verb, postfix, perm, loa, l
			);
		} else {
			printf(
				"%s void %s%s%s(struct afb_req req);\n"
				, scope, prefix, verb, postfix
			);
		}
	}

	free(verbs);
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

	/* generate permissions strings */
	make_permissions((struct path){ .object = root, .upper = NULL });

	/* expand references */
	root = expand_$ref((struct path){ .object = root, .upper = NULL });

	/* get some names */
	getvar(&api, "#/api", "?");
	getvar(&init, "#/meta-binding/init", NULL);
	getvar(&start, "#/meta-binding/start", NULL);
	getvar(&onevent, "#/meta-binding/onevent", NULL);
	getvar(&scope, "#/meta-binding/scope", "static");
	getvar(&prefix, "#/meta-binding/prefix", "afb_verb_");
	getvar(&postfix, "#/meta-binding/postfix", "_cb");

	/* get the API name */
	printf(
		"\n"
		"static const char _afb_description_v2_[] =\n"
		"%s"
		";\n"
		"\n"
		, desc
	);
	print_verbs(0);
	printf(
		"\n"
		"static const struct afb_verb_v2 _afb_verbs_v2_[] = {\n"
	);
	print_verbs(1);
	printf(
		"    { .verb = NULL }\n"
		"};\n"
		"\n"
		"const struct afb_binding_v2 afbBindingV2 = {\n"
		"    .api = \"%s\",\n"
		"    .specification = _afb_description_v2_,\n"
		"    .verbs = _afb_verbs_v2_,\n"
		"    .init = %s,\n"
		"    .start = %s,\n"
		"    .onevent = %s,\n"
		"};\n"
		"\n"
		, api?:"?", init?:"NULL", start?:"NULL", onevent?:"NULL"
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

