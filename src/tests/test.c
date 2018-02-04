#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>


#include "../common.h"


void leak_memory() {
    int * const temporary = (int*)malloc(sizeof(int));
    *temporary = 0;
}


/* A test case that does nothing and succeeds. */
static void test_foo(void **state) {
    (void) state; /* unused */
    int rv = foo();
    return;
}

static void test_bar(void **state) {
    int rv = bar();
    return;
}

static void test_bam(void **state) {
    assert_int_equal(bam(), 0);
    return;
}

static void test_bat(void **state) {
    leak_memory();
    return;
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_foo),
        cmocka_unit_test(test_bar),
        cmocka_unit_test(test_bam),
        cmocka_unit_test(test_bat),
    };
    int rv = cmocka_run_group_tests(tests, NULL, NULL);
    return rv;
}
