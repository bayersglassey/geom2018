
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stringstore.h"
#include "../str_utils.h"


#define ASSERT(COND) n_tests++; if(COND){ \
    fprintf(stderr, "OK: " #COND "\n"); \
}else { \
    n_fails++; \
    fprintf(stderr, "FAIL: " #COND "\n"); \
}


int stringstore_tests(){
    int n_tests = 0;
    int n_fails = 0;

    stringstore_t _store, *store=&_store;
    stringstore_init(store);

    ASSERT(store->entries_len == 0)
    const char *x1 = stringstore_get(store, "x");
    ASSERT(store->entries_len == 1)
    const char *x2 = stringstore_get(store, "x");
    ASSERT(store->entries_len == 1)
    ASSERT(x1 == x2)
    ASSERT(!strcmp(x1, "x"))

    const char *y1 = stringstore_get(store, "y");
    ASSERT(store->entries_len == 2)

    char *z1 = strdup("z");
    ASSERT(z1 != NULL)

    const char *z2 = stringstore_get_donate(store, z1);
    ASSERT(store->entries_len == 3)
    ASSERT(z2 == z1)

    const char *z3 = stringstore_get(store, "z");
    ASSERT(store->entries_len == 3)
    ASSERT(z3 == z1)

    stringstore_dump(store, stderr);
    stringstore_cleanup(store);

    if(n_fails > 0){
        printf("### ! %i/%i TESTS FAILED ! ###\n", n_fails, n_tests);
    }else{
        printf("### %i TESTS OK ###\n", n_tests);
    }
    return n_fails;
}


int main(int n_args, char *args[]){
    int n_fails = stringstore_tests();
    return n_fails > 0? EXIT_FAILURE: EXIT_SUCCESS;
}

