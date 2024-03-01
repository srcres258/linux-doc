// SPDX-License-Identifier: GPL-2.0-only
/*
 * Test cases for string functions.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>

static void test_memset16(struct kunit *test)
{
	unsigned i, j, k;
	u16 v, *p;

	p = kmalloc(256 * 2 * 2, GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, p);

	for (i = 0; i < 256; i++) {
		for (j = 0; j < 256; j++) {
			memset(p, 0xa1, 256 * 2 * sizeof(v));
			memset16(p + i, 0xb1b2, j);
			for (k = 0; k < 512; k++) {
				v = p[k];
				if (k < i) {
					KUNIT_EXPECT_EQ(test, v, 0xa1a1);
				} else if (k < i + j) {
					KUNIT_EXPECT_EQ(test, v, 0xb1b2);
				} else {
					KUNIT_EXPECT_EQ(test, v, 0xa1a1);
				}
			}
		}
	}

	kfree(p);
	if (i < 256)
		KUNIT_EXPECT_EQ(test, 0, (i << 24) | (j << 16) | k | 0x8000);
}

static void test_memset32(struct kunit *test)
{
	unsigned i, j, k;
	u32 v, *p;

	p = kmalloc(256 * 2 * 4, GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, p);

	for (i = 0; i < 256; i++) {
		for (j = 0; j < 256; j++) {
			memset(p, 0xa1, 256 * 2 * sizeof(v));
			memset32(p + i, 0xb1b2b3b4, j);
			for (k = 0; k < 512; k++) {
				v = p[k];
				if (k < i) {
					KUNIT_EXPECT_EQ(test, v, 0xa1a1a1a1);
				} else if (k < i + j) {
					KUNIT_EXPECT_EQ(test, v, 0xb1b2b3b4);
				} else {
					KUNIT_EXPECT_EQ(test, v, 0xa1a1a1a1);
				}
			}
		}
	}

	kfree(p);
	if (i < 256)
		KUNIT_EXPECT_EQ(test, 0, (i << 24) | (j << 16) | k | 0x8000);
}

static void test_memset64(struct kunit *test)
{
	unsigned i, j, k;
	u64 v, *p;

	p = kmalloc(256 * 2 * 8, GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, p);

	for (i = 0; i < 256; i++) {
		for (j = 0; j < 256; j++) {
			memset(p, 0xa1, 256 * 2 * sizeof(v));
			memset64(p + i, 0xb1b2b3b4b5b6b7b8ULL, j);
			for (k = 0; k < 512; k++) {
				v = p[k];
				if (k < i) {
					KUNIT_EXPECT_EQ(test, v, 0xa1a1a1a1a1a1a1a1ULL);
				} else if (k < i + j) {
					KUNIT_EXPECT_EQ(test, v, 0xb1b2b3b4b5b6b7b8ULL);
				} else {
					KUNIT_EXPECT_EQ(test, v, 0xa1a1a1a1a1a1a1a1ULL);
				}
			}
		}
	}

	kfree(p);
	if (i < 256)
		KUNIT_EXPECT_EQ(test, 0, (i << 24) | (j << 16) | k | 0x8000);
}

static void test_strchr(struct kunit *test)
{
	const char *test_string = "abcdefghijkl";
	const char *empty_string = "";
	char *result;
	int i;

	for (i = 0; i < strlen(test_string) + 1; i++) {
		result = strchr(test_string, test_string[i]);
		KUNIT_ASSERT_EQ(test, result - test_string, i);
	}

	result = strchr(empty_string, '\0');
	KUNIT_ASSERT_PTR_EQ(test, result, empty_string);

	result = strchr(empty_string, 'a');
	KUNIT_ASSERT_NULL(test, result);

	result = strchr(test_string, 'z');
	KUNIT_ASSERT_NULL(test, result);
}

static void test_strnchr(struct kunit *test)
{
	const char *test_string = "abcdefghijkl";
	const char *empty_string = "";
	char *result;
	int i, j;

	for (i = 0; i < strlen(test_string) + 1; i++) {
		for (j = 0; j < strlen(test_string) + 2; j++) {
			result = strnchr(test_string, j, test_string[i]);
			if (j <= i) {
				if (!result)
					continue;
				KUNIT_ASSERT_EQ(test, 0, 1);
			}
			if (result - test_string != i)
				KUNIT_ASSERT_EQ(test, 0, 1);
		}
	}

	result = strnchr(empty_string, 0, '\0');
	KUNIT_ASSERT_NULL(test, result);

	result = strnchr(empty_string, 1, '\0');
	KUNIT_ASSERT_PTR_EQ(test, result, empty_string);

	result = strnchr(empty_string, 1, 'a');
	KUNIT_ASSERT_NULL(test, result);

	result = strnchr(NULL, 0, '\0');
	KUNIT_ASSERT_NULL(test, result);
}

static void test_strspn(struct kunit *test)
{
	static const struct strspn_test {
		const char str[16];
		const char accept[16];
		const char reject[16];
		unsigned a;
		unsigned r;
	} tests[] = {
		{ "foobar", "", "", 0, 6 },
		{ "abba", "abc", "ABBA", 4, 4 },
		{ "abba", "a", "b", 1, 1 },
		{ "", "abc", "abc", 0, 0},
	};
	const struct strspn_test *s = tests;
	size_t i, res;

	for (i = 0; i < ARRAY_SIZE(tests); ++i, ++s) {
		res = strspn(s->str, s->accept);
		KUNIT_ASSERT_EQ(test, res, s->a);
		res = strcspn(s->str, s->reject);
		KUNIT_ASSERT_EQ(test, res, s->r);
	}
}

static struct kunit_case string_test_cases[] = {
	KUNIT_CASE(test_memset16),
	KUNIT_CASE(test_memset32),
	KUNIT_CASE(test_memset64),
	KUNIT_CASE(test_strchr),
	KUNIT_CASE(test_strnchr),
	KUNIT_CASE(test_strspn),
	{}
};

static struct kunit_suite string_test_suite = {
	.name = "string",
	.test_cases = string_test_cases,
};

kunit_test_suites(&string_test_suite);

MODULE_LICENSE("GPL v2");
