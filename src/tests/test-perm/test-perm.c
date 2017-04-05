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


#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "afb-perm.h"

char *exprs[] = {
	"a",
	"not a",
	"a or b",
	"a or b or c",
	"a or b or c or d",
	"a and b",
	"a and b and c",
	"a and b and c and d",
	"a and b or c and d",
	"a or b and c or d",
	"(a or b) and (c or d)",
	"not (a or b or c or d)",
	"a and not (b or c or d)",
	"b and not (a or c or d)",
	"c and not (a or b or d)",
	"d and not (a or b or c)",
	NULL
};

int check(void *closure, const char *name)
{
	int x;

	x = *(int*)closure;
	if (name[0] < 'a' || name[0] > 'd' || name[1])
		return 0;
	return 1 & (x >> (name[0] - 'a'));
}

int test(const char *expr)
{
	int x, r, m, c;
	struct afb_perm *perm;

	r = 0;
	m = 1;
	perm = afb_perm_parse(expr);
	if (!perm)
		printf("error for %s\n", expr);
	else {
		printf("\nabcd   %s\n", expr);
		for (x = 0; x < 16 ; x++) {
			c = afb_perm_check(perm, check, &x);
			printf("%c%c%c%c   %d\n",
				'0'+!!(x&1), '0'+!!(x&2), '0'+!!(x&4), '0'+!!(x&8),
				c);
			if (c)
				r |= m;
			m <<= 1;
		}
	}
	afb_perm_unref(perm);
	return r;
}

void add(char *buffer, const char *fmt, ...)
{
	char b[60];
	va_list vl;

	va_start(vl, fmt);
	vsprintf(b, fmt, vl);
	va_end(vl);
	strcat(buffer, b);
}

void mke(int value, int bits, char *buffer)
{
	int nval = 1 << bits;
	int sval = 1 << (bits - 1);
	int mask = (1 << nval) - 1;
	int smask = (1 << sval) - 1;
	int val = value & mask;
	int val0 = val & smask;
	int val1 = (val >> sval) & smask;
	char c = (char)('a' + bits - 1);

	if (bits == 1) {
		switch(val) {
		case 0: add(buffer, "x"); break;
		case 1: add(buffer, "not %c", c); break;
		case 2: add(buffer, "%c", c); break;
		case 3: add(buffer, "(%c or not %c) ", c, c); break;
		}
	} else if (val0 != val1) {
		if (val0) {
			add(buffer, "not %c", c);
			if (val0 != smask) {
				add(buffer, " and (");
				mke(val0, bits - 1, buffer);
				add(buffer, ")");
			}
		}
		if (val0 && val1)
			add(buffer, " or ");
		if (val1) {
			add(buffer, "%c", c);
			if (val1 != smask) {
				add(buffer, " and (");
				mke(val1, bits - 1, buffer);
				add(buffer, ")");
			}
		}
	} else {
		mke(val0, bits - 1, buffer);
	}
}

void makeexpr(int value, char *buffer)
{
	if (!value)
		strcpy(buffer, "x");
	else {
		buffer[0] = 0;
		mke(value, 4, buffer);
	}
}

int fulltest()
{
	char buffer[4096];
	int i, j, r;

	r = 0;
	for (i = 0 ; i < 65536 ; i++) {
		makeexpr(i, buffer);
		j = test(buffer);
		printf("[[[ %d %s %d ]]]\n", i, i==j?"==":"!=", j);
		if (i != j)
			r = 1;
	}
}

int main()
{
	int i = 0;
	while(exprs[i])
		test(exprs[i++]);
	return fulltest();
}

