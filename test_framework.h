#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int _g_test_failed = 0;

#define TEST(name) static void name(void)
#define RUN(name)  do { \
    _g_test_failed = 0; \
    printf("  %-50s ", #name); \
    fflush(stdout); \
    name(); \
    if (!_g_test_failed) { printf("OK\n"); tests_run++; tests_passed++; } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_run++; tests_failed++; \
        _g_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { \
        printf("FAIL\n    %s:%d: %lld != %lld\n", __FILE__, __LINE__, _a, _b); \
        tests_run++; tests_failed++; \
        _g_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_STREQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL\n    %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        tests_run++; tests_failed++; \
        _g_test_failed = 1; \
        return; \
    } \
} while(0)

#endif
