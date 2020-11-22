
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vars.h"


#define ASSERT(COND) n_tests++; if(COND){ \
    fprintf(stderr, "OK: " #COND "\n"); \
}else { \
    n_fails++; \
    fprintf(stderr, "FAIL: " #COND "\n"); \
}

static char *strdup(const char *s1){
    char *s2 = malloc(strlen(s1) + 1);
    if(s2 == NULL)return NULL;
    strcpy(s2, s1);
    return s2;
}

int testrunner(){
    int n_tests = 0;
    int n_fails = 0;

    vars_t _vars, *vars=&_vars;
    vars_init(vars);

    ASSERT(vars->vars_len == 0)

    ASSERT(!vars_set_null(vars, "x"))
    ASSERT(vars->vars_len == 1)

    ASSERT(!vars_set_null(vars, "x"))
    ASSERT(vars->vars_len == 1)

    ASSERT(!vars_set_int(vars, "x", 3))
    ASSERT(vars->vars_len == 1)
    ASSERT(vars_get_int(vars, "x") == 3)

    ASSERT(!vars_set_const_str(vars, "y", "HAHA"))
    ASSERT(vars->vars_len == 2)
    ASSERT(!strcmp(vars_get_str(vars, "y"), "HAHA"))

    ASSERT(!vars_set_str(vars, "z", strdup("LAWL")))
    ASSERT(vars->vars_len == 3)
    ASSERT(!strcmp(vars_get_str(vars, "z"), "LAWL"))

    ASSERT(!vars_set_bool(vars, "yes", true))
    ASSERT(vars->vars_len == 4)
    ASSERT(vars_get_bool(vars, "yes"))

    ASSERT(!vars_set_bool(vars, "no", false))
    ASSERT(vars->vars_len == 5)
    ASSERT(!vars_get_bool(vars, "no"))

    ASSERT(!vars_get_bool(vars, "fake"))
    ASSERT(vars_get_int(vars, "fake") == 0)
    ASSERT(vars_get_str(vars, "fake") == NULL)

    vars_dump(vars);
    vars_cleanup(vars);

    if(n_fails > 0){
        printf("### ! %i/%i TESTS FAILED ! ###\n", n_fails, n_tests);
    }else{
        printf("### %i TESTS OK ###\n", n_tests);
    }
    return n_fails;
}


int main(int n_args, char *args[]){
    int n_fails = testrunner();
    return n_fails > 0? EXIT_FAILURE: EXIT_SUCCESS;
}

