#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include <check.h>

#include "afb-session.h"

#define GOOD_UUID  "123456789012345678901234567890123456"
#define BAD_UUID   "1234567890123456789012345678901234567"

START_TEST (test_initialisation)
{
	ck_assert_int_eq(0, afb_session_init(0, 0, NULL));
	ck_assert_int_eq(0, afb_session_init(200, 0, NULL));
	ck_assert_int_eq(0, afb_session_init(10, 0, GOOD_UUID));
	ck_assert_str_eq(GOOD_UUID, afb_session_initial_token());
	ck_assert_int_eq(-1, afb_session_init(10, 0, BAD_UUID));
	ck_assert_int_eq(errno, EINVAL);
}
END_TEST


START_TEST (test_sanity)
{
	struct afb_session *s;
	s = afb_session_addref(NULL);
	ck_assert(!s);
	afb_session_unref(NULL);
	ck_assert(1);
}
END_TEST


START_TEST (test_creation)
{
	char *uuid;
	struct afb_session *s, *x;

	/* init */
	ck_assert_int_eq(0, afb_session_init(10, 3600, GOOD_UUID));

	/* create a session */
	s = afb_session_create(AFB_SESSION_TIMEOUT_DEFAULT);
	ck_assert(s);

	/* the session is valid */
	ck_assert(afb_session_uuid(s) != NULL);
	ck_assert(afb_session_token(s) != NULL);
	ck_assert(!afb_session_is_closed(s));
	
	/* token is the initial one */
	ck_assert_str_eq(afb_session_token(s), GOOD_UUID);
	ck_assert(afb_session_check_token(s, GOOD_UUID));
	ck_assert(afb_session_check_token(s, afb_session_token(s)));

	/* token can be renewed */
	afb_session_new_token(s);
	ck_assert(strcmp(afb_session_token(s), GOOD_UUID));
	ck_assert(!afb_session_check_token(s, GOOD_UUID));
	ck_assert(afb_session_check_token(s, afb_session_token(s)));

	/* query the session */
	uuid = strdup(afb_session_uuid(s));
	x = afb_session_search(uuid);
	ck_assert(x == s);

	/* still alive after search */
	afb_session_unref(x);
	afb_session_unref(s);
	s = afb_session_search(uuid);
	ck_assert(s);
	ck_assert(x == s);

	/* but not after closing */
	afb_session_close(s);
	ck_assert(afb_session_is_closed(s));
	afb_session_unref(s);
	afb_session_purge();
	s = afb_session_search(uuid);
	ck_assert(!s);
	free(uuid);
}
END_TEST


START_TEST (test_capacity)
{
	struct afb_session *s[3];
	ck_assert_int_eq(0, afb_session_init(2, 3600, GOOD_UUID));
	s[0] = afb_session_create(AFB_SESSION_TIMEOUT_DEFAULT);
	ck_assert(s[0]);
	s[1] = afb_session_create(AFB_SESSION_TIMEOUT_DEFAULT);
	ck_assert(s[1]);
	s[2] = afb_session_create(AFB_SESSION_TIMEOUT_DEFAULT);
	ck_assert(!s[2]);
	afb_session_close(s[0]);
	afb_session_unref(s[0]);
	s[2] = afb_session_create(AFB_SESSION_TIMEOUT_DEFAULT);
	ck_assert(s[2]);
	s[0] = afb_session_create(AFB_SESSION_TIMEOUT_DEFAULT);
	ck_assert(!s[0]);
	afb_session_unref(s[0]);
	afb_session_unref(s[1]);
	afb_session_unref(s[2]);
}
END_TEST


void *mkcookie_got;
void *mkcookie(void *closure)
{
	mkcookie_got = closure;
	return closure;
}

void *freecookie_got;
void freecookie(void *item)
{
	freecookie_got = item;
}

START_TEST (test_cookies)
{
	char *k[] = { "key1", "key2", "key3", NULL }, *p, *q, *d = "default";
	struct afb_session *s;
	int i, j;

	/* init */
	ck_assert_int_eq(0, afb_session_init(10, 3600, GOOD_UUID));

extern void *afb_session_cookie(struct afb_session *session, const void *key, void *(*makecb)(void *closure), void (*freecb)(void *item), void *closure, int replace);

	/* create a session */
	s = afb_session_create(AFB_SESSION_TIMEOUT_DEFAULT);
	ck_assert(s);

	/* set the cookie */
	for (i = 0 ; k[i] ; i++) {
		for (j = 0 ; k[j] ; j++) {
			/* retrieve the previous value */
			mkcookie_got = freecookie_got = NULL;
			p = afb_session_cookie(s, k[j], NULL, NULL, NULL, 0);
			if (!p) {
				/* never set (i = 0) */
				q = afb_session_cookie(s, k[j], NULL, NULL, d, 0);
				ck_assert(q == d);
				p = afb_session_cookie(s, k[j], NULL, NULL, NULL, 0);
				ck_assert(!p);
			}
			q = afb_session_cookie(s, k[j], mkcookie, freecookie, k[i], 1);
			ck_assert(q == k[i]);
			ck_assert(mkcookie_got == q);
			ck_assert(freecookie_got == p);
		}
	}

	/* drop cookies */
	for (i = 1 ; k[i] ; i++) {
		mkcookie_got = freecookie_got = NULL;
		p = afb_session_cookie(s, k[i], NULL, NULL, NULL, 0);
		ck_assert(!freecookie_got);
		q = afb_session_cookie(s, k[i], NULL, NULL, NULL, 1);
		ck_assert(!q);
		ck_assert(freecookie_got == p);
	}

	/* closing session */
	p = afb_session_cookie(s, k[0], NULL, NULL, NULL, 0);
	mkcookie_got = freecookie_got = NULL;
	afb_session_close(s);
	ck_assert(freecookie_got == p);
	p = afb_session_cookie(s, k[0], NULL, NULL, NULL, 0);
	ck_assert(!p);
	afb_session_unref(s);
}
END_TEST

static Suite *suite;
static TCase *tcase;

void mksuite(const char *name) { suite = suite_create(name); }
void addtcase(const char *name) { tcase = tcase_create(name); suite_add_tcase(suite, tcase); }
void addtest(TFun fun) { tcase_add_test(tcase, fun); }
int srun()
{
	int nerr;
	SRunner *srunner = srunner_create(suite);
	srunner_run_all(srunner, CK_NORMAL);
	nerr = srunner_ntests_failed(srunner);
	srunner_free(srunner);
	return nerr;
}

int main(int ac, char **av)
{
	mksuite("session");
		addtcase("session");
			addtest(test_initialisation);
			addtest(test_sanity);
			addtest(test_creation);
			addtest(test_capacity);
			addtest(test_cookies);
	return !!srun();
}
