
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../frozen_string.h"


#define ASSERT(COND) fprintf(stderr, "T%i: ", n_tests); n_tests++; if(COND){ \
    fprintf(stderr, "OK: " #COND "\n"); \
}else { \
    n_fails++; \
    fprintf(stderr, "FAIL: " #COND "\n"); \
}


int run_tests(){
    int n_tests = 0;
    int n_fails = 0;

    frozen_string_t z0, z1, *z2, *z3;

    /* Frozen const string */
    frozen_string_init(&z0, "I AM STRING");
    ASSERT(!strcmp(z0.data, "I AM STRING"))
    ASSERT(z0.managed_data == NULL)
    ASSERT(z0.refcount == 1)
    ASSERT(frozen_string_dup(&z0) == &z0)
    ASSERT(z0.refcount == 2)
    frozen_string_cleanup(&z0);
    ASSERT(z0.refcount == 1)
    frozen_string_cleanup(&z0);
    ASSERT(z0.refcount == 0)

    /* Malloced frozen const string */
    z2 = frozen_string_create("LPELPE");
    ASSERT(!strcmp(z2->data, "LPELPE"))
    ASSERT(z2->managed_data == NULL)
    ASSERT(z2->refcount == 1)
    frozen_string_free(z2);

    /* Frozen managed string */
    const char s1[] = "CHALLO WARLD";
    char *managed_s1 = malloc(strlen(s1) + 1);
    strcpy(managed_s1, s1);
    ASSERT(!strcmp(managed_s1, "CHALLO WARLD"))
    ASSERT(managed_s1 != NULL)
    if(managed_s1 != NULL){
        frozen_string_init_managed(&z1, managed_s1);
        ASSERT(!strcmp(z1.data, "CHALLO WARLD"))
        ASSERT(z1.data == managed_s1)
        ASSERT(z1.managed_data == managed_s1)
        ASSERT(z1.refcount == 1)
        ASSERT(frozen_string_dup(&z1) == &z1)
        ASSERT(z1.refcount == 2)
        frozen_string_cleanup(&z1);
        ASSERT(z1.refcount == 1)
        frozen_string_cleanup(&z1);
        ASSERT(z1.refcount == 0)
    }

    /* Malloced frozen managed string */
    z3 = frozen_string_create("XSXSX");
    ASSERT(!strcmp(z3->data, "XSXSX"))
    ASSERT(z3->managed_data == NULL)
    ASSERT(z3->refcount == 1)
    frozen_string_free(z3);

    if(n_fails > 0){
        printf("### ! %i/%i TESTS FAILED ! ###\n", n_fails, n_tests);
    }else{
        printf("### %i TESTS OK ###\n", n_tests);
    }
    return n_fails;
}


int main(int n_args, char *args[]){
    int n_fails = run_tests();
    return n_fails > 0? EXIT_FAILURE: EXIT_SUCCESS;
}

