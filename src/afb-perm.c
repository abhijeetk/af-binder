/*
 * Copyright (C) 2017 "IoT.bzh"
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


#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "afb-perm.h"
#include "verbose.h"

/*********************************************************************
*** SECTION node
*********************************************************************/

/**
 * types of nodes
 */
enum type
{
	Text,	/**< text node */
	And,	/**< and node */
	Or,	/**< or node */
	Not	/**< not node */
};

/**
 * structure for nodes
 */
struct node
{
	enum type type;	/**< type of the node */
	union {
		struct node *children[2]; /**< possible subnodes */
		char text[1]; /**< text of the node */
	};
};

/**
 * make a text node and return it
 * @param type type of the node (should be Text)
 * @param text the text to set for the node
 * @param length the length of the text
 * @return the allocated node or NULL on memory depletion
 */
static struct node *node_make_text(enum type type, const char *text, size_t length)
{
	struct node *node;

	node = malloc(length + sizeof *node);
	if (!node)
		errno = ENOMEM;
	else {
		node->type = type;
		memcpy(node->text, text, length);
		node->text[length] = 0;
	}
	return node;
}

/**
 * make a node with sub nodes and return it
 * @param type type of the node (should be And, Or or Text)
 * @param left the "left" sub node
 * @param right the "right" sub node (if any or NULL)
 * @return the allocated node or NULL on memory depletion
 */
static struct node *node_make_parent(enum type type, struct node *left, struct node *right)
{
	struct node *node;

	node = malloc(sizeof *node);
	if (!node)
		errno = ENOMEM;
	else {
		node->type = type;
		node->children[0] = left;
		node->children[1] = right;
	}
	return node;
}

/**
 * Frees the node and its possible subnodes
 * @param node the node to free
 */
static void node_free(struct node *node)
{
	struct node *left, *right;

	if (node) {
		switch (node->type) {
		case Text:
			free(node);
			break;
		case And:
		case Or:
			left = node->children[0];
			right = node->children[1];
			free(node);
			node_free(left);
			node_free(right);
			break;
		case Not:
			left = node->children[0];
			free(node);
			node_free(left);
			break;
		}
	}
}

/**
 * Checks the permissions for the 'node' using the 'check' function
 * and its 'closure'.
 * @param node the node to check
 * @param check the function that checks if a pernmission of 'name' is granted for 'closure'
 * @param closure the context closure for the function 'check'
 * @return 1 if the permission is granted or 0 otherwise
 */
static int node_check(struct node *node, int (*check)(void *closure, const char *name), void *closure)
{
	for(;;) {
		switch (node->type) {
		case Text:
			return check(closure, node->text);
		case And:
			if (!node_check(node->children[0], check, closure))
				return 0;
			break;
		case Or:
			if (node_check(node->children[0], check, closure))
				return 1;
			break;
		case Not:
			return !node_check(node->children[0], check, closure);
		}
		node = node->children[1];
	}
}

/*********************************************************************
*** SECTION parse
*********************************************************************/

/**
 * the symbol types
 */
enum symbol
{
	TEXT,	/**< a common text, name of a permission */
	AND,	/**< and keyword */
	OR,	/**< or keyword */
	NOT,	/**< not keyword */
	OBRA,	/**< open bracket */
	CBRA,	/**< close bracket */
	END	/**< end of input */
};

/**
 * structure for parsing permission description
 */
struct parse
{
	const char *desc;	/**< initial permission description */
	const char *symbol;	/**< current symbol parsed */
	size_t symlen;		/**< length of the current symbol */
	enum symbol type;	/**< type of the current symbol */
};

/**
 * updates parse to point to the next symbol if any
 * @param parse parser state to update
 */
static void parse_next(struct parse *parse)
{
	const char *scan;
	size_t len;

	/* skip current symbol */
	scan = &parse->symbol[parse->symlen];

	/* skip white spaces */
	while (*scan && isspace(*scan))
		scan++;

	/* scan the symbol */
	switch (*scan) {
	case 0:
		len = 0;
		parse->type = END;
		break;
	case '(':
		len = 1;
		parse->type = OBRA;
		break;
	case ')':
		len = 1;
		parse->type = CBRA;
		break;
	default:
		/* compute the length */
		len = 0;
		while (scan[len] && !isspace(scan[len]) && scan[len] != ')' && scan[len] != '(')
			len++;
		parse->type = TEXT;

		/* fall to keyword if any */
		switch(len) {
		case 2:
			if (!strncasecmp(scan, "or", len))
				parse->type = OR;
			break;
		case 3:
			if (!strncasecmp(scan, "and", len))
				parse->type = AND;
			else if (!strncasecmp(scan, "not", len))
				parse->type = NOT;
			break;
		}
		break;
	}
	parse->symbol = scan;
	parse->symlen = len;
}

/**
 * Init the parser state 'parse' for the description 'desc'
 * @param parse the parser state to initialise
 * @param desc the description of the permissions to be parsed
 */
static void parse_init(struct parse *parse, const char *desc)
{
	parse->desc = desc;
	parse->symbol = desc;
	parse->symlen = 0;
	parse_next(parse);
}

/*********************************************************************
*** SECTION node_parse
*********************************************************************/

static struct node *node_parse_or(struct parse *parse);

/**
 * Parse a permission name
 * @param parser the parser state
 * @return the parsed node or NULL in case of error
 * in case of error errno is set to EINVAL or ENOMEM
 */
static struct node *node_parse_text(struct parse *parse)
{
	struct node *node;

	if (parse->type == TEXT) {
		node = node_make_text(Text, parse->symbol, parse->symlen);
		parse_next(parse);
	} else {
		errno = EINVAL;
		node = NULL;
	}
	return node;
}

/**
 * Parse a term that is either a name (text) or a sub expression
 * enclosed in brackets.
 * @param parser the parser state
 * @return the parsed node or NULL in case of error
 * in case of error errno is set to EINVAL or ENOMEM
 */
static struct node *node_parse_term(struct parse *parse)
{
	struct node *node;

	if (parse->type != OBRA)
		node = node_parse_text(parse);
	else {
		parse_next(parse);
		node = node_parse_or(parse);
		if (parse->type == CBRA)
			parse_next(parse);
		else {
			errno = EINVAL;
			node_free(node);
			node = NULL;
		}
	}
	return node;
}

/**
 * Parse a term potentially prefixed by not.
 * @param parser the parser state
 * @return the parsed node or NULL in case of error
 * in case of error errno is set to EINVAL or ENOMEM
 */
static struct node *node_parse_not(struct parse *parse)
{
	struct node *node, *not;

	if (parse->type != NOT)
		node = node_parse_term(parse);
	else {
		parse_next(parse);
		node = node_parse_term(parse);
		if (node) {
			not = node_make_parent(Not, node, NULL);
			if (not)
				node = not;
			else {
				node_free(node);
				node = NULL;
			}
		}
	}
	return node;
}

/**
 * Parse a potential sequence of terms connected with the
 * given operator (AND or OR). The function takes care to
 * create an evaluation tree that respects the order given
 * by the description and that will limit the recursivity
 * depth.
 * @param parser the parser state
 * @param operator the symbol type of the operator scanned
 * @param subparse the function for parsing terms of the sequence
 * @param type the node type corresponding to the operator
 * @return the parsed node or NULL in case of error
 * in case of error errno is set to EINVAL or ENOMEM
 */
static struct node *node_parse_infix(
		struct parse *parse,
		enum symbol operator,
		struct node *(*subparse)(struct parse*),
		enum type type
)
{
	struct node *root, **up, *right, *node;

	root = subparse(parse);
	if (root) {
		up = &root;
		while (parse->type == operator) {
			parse_next(parse);
			right = subparse(parse);
			if (!right) {
				node_free(root);
				root = NULL;
				break;
			}
			node = node_make_parent(type, *up, right);
			if (!node) {
				node_free(right);
				node_free(root);
				root = NULL;
				break;
			}
			*up = node;
			up = &node->children[1];
		}
	}
	return root;
}

/**
 * Parse a potential sequence of anded terms.
 * @param parser the parser state
 * @return the parsed node or NULL in case of error
 * in case of error errno is set to EINVAL or ENOMEM
 */
static struct node *node_parse_and(struct parse *parse)
{
	return node_parse_infix(parse, AND, node_parse_not, And);
}

/**
 * Parse a potential sequence of ored terms.
 * @param parser the parser state
 * @return the parsed node or NULL in case of error
 * in case of error errno is set to EINVAL or ENOMEM
 */
static struct node *node_parse_or(struct parse *parse)
{
	return node_parse_infix(parse, OR, node_parse_and, Or);
}

/**
 * Parse description of permissions.
 * @param desc the description to parse
 * @return the parsed node or NULL in case of error
 * in case of error errno is set to EINVAL or ENOMEM
 */
static struct node *node_parse(const char *desc)
{
	struct node *node;
	struct parse parse;

	parse_init(&parse, desc);
	node = node_parse_or(&parse);
	if (node && parse.type != END) {
		node_free(node);
		node = NULL;
	}
	return node;
}

/*********************************************************************
*** SECTION perm
*********************************************************************/

/**
 * structure for storing permissions
 */
struct afb_perm
{
	struct node *root;	/**< root node descripbing the permission */
	int refcount;		/**< the count of use of the structure */
};

/**
 * allocates the structure for the given root
 * @param root the root node to keep
 * @return the created permission object or NULL
 */
static struct afb_perm *make_perm(struct node *root)
{
	struct afb_perm *perm;

	perm = malloc(sizeof *perm);
	if (!perm)
		errno = ENOMEM;
	else {
		perm->root = root;
		perm->refcount = 1;
	}
	return perm;
}

/**
 * Creates the permission for the given description
 * @param desc the description of the permission to create
 * @return the created permission object or NULL
 */
struct afb_perm *afb_perm_parse(const char *desc)
{
	struct node *root;
	struct afb_perm *perm;

	root = node_parse(desc);
	if (root) {
		perm = make_perm(root);
		if (perm)
			return perm;
		node_free(root);
	}
	return NULL;
}

/**
 * Adds a reference to the permissions
 * @param perm the permission to reference
 */
void afb_perm_addref(struct afb_perm *perm)
{
	assert(perm);
	perm->refcount++;
}

/**
 * Removes a reference to the permissions
 * @param perm the permission to dereference
 */
void afb_perm_unref(struct afb_perm *perm)
{
	if (perm && !--perm->refcount) {
		node_free(perm->root);
		free(perm);
	}
}

/**
 * Checks permission 'perm' using the 'check' function
 * and its 'closure'.
 * @param perm the permission to check
 * @param check the function that checks if a pernmission of 'name' is granted for 'closure'
 * @param closure the context closure for the function 'check'
 * @return 1 if the permission is granted or 0 otherwise
 */
int afb_perm_check(struct afb_perm *perm, int (*check)(void *closure, const char *name), void *closure)
{
	return node_check(perm->root, check, closure);
}


