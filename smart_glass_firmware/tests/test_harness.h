#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define MAX_TESTS 256
#define MAX_TNAME 128

typedef void (*test_fn)(void);

typedef struct {
    char tstr[MAX_TNAME];
    test_fn fn;
    int failures;
} test_reg_t;

static test_reg_t __tests[MAX_TESTS];
static int __test_count = 0;
static int __global_failures = 0;

#define TEST(tname) \
    static void tn_##tname(void); \
    __attribute__((constructor)) static void reg_##tname(void) { \
        if (__test_count < MAX_TESTS) { \
            snprintf(__tests[__test_count].tstr, MAX_TNAME, "%s", #tname); \
            __tests[__test_count].fn = tn_##tname; \
            __test_count++; \
        } \
    } \
    static void tn_##tname(void)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%s:%d] ASSERT_TRUE(%s)\n", __FILE__, __LINE__, #cond); \
        __global_failures++; return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "  FAIL [%s:%d] ASSERT_EQ(%s, %s): got %lld, expected %lld\n", \
                __FILE__, __LINE__, #a, #b, (long long)(a), (long long)(b)); \
        __global_failures++; return; \
    } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b, eps) do { \
    float _diff = (float)fabs((double)(a) - (double)(b)); \
    if (_diff > (eps)) { \
        fprintf(stderr, "  FAIL [%s:%d] ASSERT_FLOAT_EQ(%s, %s): got %f, expected %f (diff %f > %f)\n", \
                __FILE__, __LINE__, #a, #b, (double)(a), (double)(b), _diff, (double)(eps)); \
        __global_failures++; return; \
    } \
} while(0)

#define ASSERT_STREQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        fprintf(stderr, "  FAIL [%s:%d] ASSERT_STREQ(%s, %s): got \"%s\", expected \"%s\"\n", \
                __FILE__, __LINE__, #a, #b, (a), (b)); \
        __global_failures++; return; \
    } \
} while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        fprintf(stderr, "  FAIL [%s:%d] ASSERT_NULL(%s)\n", __FILE__, __LINE__, #ptr); \
        __global_failures++; return; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "  FAIL [%s:%d] ASSERT_NOT_NULL(%s)\n", __FILE__, __LINE__, #ptr); \
        __global_failures++; return; \
    } \
} while(0)

static int run_all_tests(void)
{
    printf("Running %d test(s)...\n", __test_count);
    for (int i = 0; i < __test_count; i++) {
        int before = __global_failures;
        __tests[i].fn();
        int failed = __global_failures - before;
        printf("  %-50s [%s]\n", __tests[i].tstr, failed ? "FAIL" : "OK");
    }
    printf("\n%d test(s) total, %d failure(s)\n", __test_count, __global_failures);
    return __global_failures > 0 ? 1 : 0;
}

#endif
