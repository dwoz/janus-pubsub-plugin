#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>


#include "../common.h"

/* other includes above ^^^ */

#include "dmalloc.h"

/* A test case that does nothing and succeeds. */
static void null_test_success(void **state) {
    (void) state; /* unused */
}

static void test_test(void **state) {
    janus_pubsub_puller *p = NULL;
    int rv = create_puller(p);
    rv = destroy_puller(p);

}
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(null_test_success),
        cmocka_unit_test(test_test),
    };
    int rv = cmocka_run_group_tests(tests, NULL, NULL);
    return rv;
}
