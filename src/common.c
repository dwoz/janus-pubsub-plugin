#include <glib.h>
#include <netdb.h>
#include <stdio.h>
#include "common.h"

#ifdef UNIT_TESTING
extern void* _test_malloc(const size_t size, const char* file, const int line);
extern void* _test_calloc(const size_t number_of_elements, const size_t size,
                          const char* file, const int line);
extern void _test_free(void* const ptr, const char* file, const int line);

#define malloc(size) _test_malloc(size, __FILE__, __LINE__)
#define calloc(num, size) _test_calloc(num, size, __FILE__, __LINE__)
#define free(ptr) _test_free(ptr, __FILE__, __LINE__)
#endif // UNIT_TESTING


int foo(void) {
   printf("\n*****WTF*****\n");
   return 0;
}

int bar(void) {
    int * const temporary = (int*)malloc(sizeof(int));
    *temporary = 0;
}
int bam(void) {
    char *foo = malloc(1024);
    return 0;
}
