
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../vars.h"
#include "../valexpr.h"
#include "../var_utils.h"


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

static int parse_valexpr(valexpr_t *expr, const char *text){
    int err;
    fus_lexer_t lexer;
    err = fus_lexer_init(&lexer, text, "<test>");
    if(err)return err;
    err = valexpr_parse(expr, &lexer);
    if(!fus_lexer_done(&lexer)){
        fprintf(stderr, "### Not all input lexed!.. input was:\n%s\n", text);
        return 2;
    }
    fus_lexer_cleanup(&lexer);
    return 0;
}

int testrunner(int *n_tests_ptr, int *n_fails_ptr){
    int err;

    int n_tests = 0;
    int n_fails = 0;

    /* VAL OPERATIONS, DIFFERENT TYPES */
    {
        val_t val1 = {.type = VAL_TYPE_INT, .u.i = 2};
        val_t val2 = {.type = VAL_TYPE_CONST_STR, .u.cs = "HELLO"};
        ASSERT(!val_eq(&val1, &val2));
        ASSERT(val_ne(&val1, &val2));
    }

    /* NULL VAL OPERATIONS */
    {
        val_t val1 = {.type = VAL_TYPE_NULL};
        ASSERT(val_eq(&val1, &val1));
        ASSERT(!val_ne(&val1, &val1));
    }

    /* BOOL VAL OPERATIONS */
    {
        val_t val1 = {.type = VAL_TYPE_BOOL, .u.b = true};
        ASSERT(val_eq(&val1, &val1));
        val_t val2 = {.type = VAL_TYPE_BOOL, .u.b = false};
        ASSERT(!val_eq(&val1, &val2));
        ASSERT(val_ne(&val1, &val2));
    }

    /* INT VAL OPERATIONS */
    {
        val_t val1 = {.type = VAL_TYPE_INT, .u.i = 1};
        ASSERT(val_eq(&val1, &val1));
        val_t val2 = {.type = VAL_TYPE_INT, .u.i = 2};
        ASSERT(!val_eq(&val1, &val2));
        ASSERT(val_ne(&val1, &val2));
        ASSERT(val_lt(&val1, &val2));
        ASSERT(val_le(&val1, &val2));
        ASSERT(!val_gt(&val1, &val2));
        ASSERT(!val_ge(&val1, &val2));
    }

    /* STR VAL OPERATIONS */
    {
        val_t val1 = {.type = VAL_TYPE_CONST_STR, .u.cs = "AAA"};
        val_t val2 = {.type = VAL_TYPE_CONST_STR, .u.cs = "BBB"};
        ASSERT(!val_eq(&val1, &val2));
        ASSERT(val_ne(&val1, &val2));
        ASSERT(val_lt(&val1, &val2));
        ASSERT(val_le(&val1, &val2));
        ASSERT(!val_gt(&val1, &val2));
        ASSERT(!val_ge(&val1, &val2));
    }

    /* VARS TESTS */
    {
        vars_t _vars, *vars=&_vars;
        vars_init(vars);

        ASSERT(vars->vars_len == 0)

        ASSERT(!vars_set_null(vars, "x"))
        ASSERT(vars->vars_len == 1)
        ASSERT(!vars_get_bool(vars, "x"))

        ASSERT(!vars_set_null(vars, "x"))
        ASSERT(vars->vars_len == 1)

        ASSERT(!vars_set_int(vars, "x", 0))
        ASSERT(vars->vars_len == 1)
        ASSERT(vars_get_int(vars, "x") == 0)
        ASSERT(!vars_get_bool(vars, "x"))

        ASSERT(!vars_set_int(vars, "x", 3))
        ASSERT(vars->vars_len == 1)
        ASSERT(vars_get_int(vars, "x") == 3)
        ASSERT(vars_get_bool(vars, "x"))

        ASSERT(!vars_set_const_str(vars, "y", NULL))
        ASSERT(vars->vars_len == 2)
        ASSERT(vars_get_str(vars, "y") == NULL)
        ASSERT(!vars_get_bool(vars, "y"))

        ASSERT(!vars_set_const_str(vars, "y", "HAHA"))
        ASSERT(vars->vars_len == 2)
        ASSERT(!strcmp(vars_get_str(vars, "y"), "HAHA"))
        ASSERT(vars_get_bool(vars, "y"))

        ASSERT(!vars_set_str(vars, "z", strdup("LAWL")))
        ASSERT(vars->vars_len == 3)
        ASSERT(!strcmp(vars_get_str(vars, "z"), "LAWL"))

        ASSERT(!vars_set_bool(vars, "yes", true))
        ASSERT(vars->vars_len == 4)
        ASSERT(vars_get_bool(vars, "yes"))

        ASSERT(!vars_set_bool(vars, "no", false))
        ASSERT(vars->vars_len == 5)
        ASSERT(!vars_get_bool(vars, "no"))

        ASSERT(!vars_set_null(vars, "nothing"))
        ASSERT(vars->vars_len == 6)

        ASSERT(!vars_get_bool(vars, "fake"))
        ASSERT(vars_get_int(vars, "fake") == 0)
        ASSERT(vars_get_str(vars, "fake") == NULL)

        vars_dump(vars);
        vars_write_simple(vars, stderr);
        vars_cleanup(vars);
    }

    /* VARS PARSING TEST */
    {
        vars_t _vars, *vars=&_vars;
        vars_init(vars);

        ASSERT(!vars_load(vars, "test_data/varstest_data/test.fus"))
        ASSERT(vars_get_int(vars, "x") == 3)
        ASSERT(!strcmp(vars_get_str(vars, "hello"), "world"))
        ASSERT(vars_get_bool(vars, "yes"))
        ASSERT(!vars_get_bool(vars, "no"))

        /* Maybe todo: add vars_is_{int,str,bool,null}?..
        Or vars_typeof() and it can be any of:
            VAL_TYPE_{INT,STR,BOOL,NULL,MISSING}
        ..?
        */
        var_t *var = vars_get(vars, "nothing");
        ASSERT(var != NULL)
        ASSERT(var->value.type == VAL_TYPE_NULL)

        vars_cleanup(vars);
    }

    /* VALEXPR TESTS */
    {
        vars_t yourvars;
        vars_t mapvars;
        vars_t globalvars;
        vars_t myvars;
        vars_init(&yourvars);
        vars_init(&mapvars);
        vars_init(&globalvars);
        vars_init(&myvars);
        valexpr_context_t context = {
            .yourvars = &yourvars,
            .mapvars = &mapvars,
            .globalvars = &globalvars,
            .myvars = &myvars
        };

        valexpr_t _expr, *expr=&_expr;

        valexpr_set_literal_bool(expr, false);
        ASSERT(!valexpr_get_bool(expr, &context));
        valexpr_cleanup(expr);

        valexpr_set_literal_bool(expr, true);
        ASSERT(valexpr_get_bool(expr, &context));
        valexpr_cleanup(expr);

        valexpr_set_literal_int(expr, 3);
        ASSERT(valexpr_get_int(expr, &context) == 3);
        valexpr_cleanup(expr);

        valexpr_set_literal_int(expr, 3);
        ASSERT(valexpr_get_bool(expr, &context));
        valexpr_cleanup(expr);

        valexpr_set_literal_str(expr, "hello");
        ASSERT(!strcmp(valexpr_get_str(expr, &context), "hello"));
        valexpr_cleanup(expr);

        {
            vars_set_const_str(&mapvars, "<A>", "<B>");
            vars_set_const_str(&myvars, "<B>", "<C>");
            vars_set_int(&yourvars, "<C>", 45);

            err = parse_valexpr(expr, "yourvar(myvar(mapvar(\"<A>\")))");
            if(err)return err;
            ASSERT(valexpr_get_int(expr, &context) == 45);
            valexpr_cleanup(expr);

            err = parse_valexpr(expr, "globalvar(\"nothing\")");
            if(err)return err;
            ASSERT(valexpr_get_int(expr, &context) == 0);
            valexpr_cleanup(expr);
        }

        {
            vars_set_bool(&yourvars, "cond", true);
            vars_set_int(&myvars, "x", 111);
            vars_set_int(&myvars, "y", 222);

            err = parse_valexpr(expr, "myvar(if yourvar(\"cond\") then \"x\" else \"y\")");
            if(err)return err;
            ASSERT(valexpr_get_int(expr, &context) == 111);
            valexpr_cleanup(expr);

            err = parse_valexpr(expr, "myvar(if not yourvar(\"cond\") then \"x\" else \"y\")");
            if(err)return err;
            ASSERT(valexpr_get_int(expr, &context) == 222);
            valexpr_cleanup(expr);
        }

        vars_cleanup(&yourvars);
        vars_cleanup(&mapvars);
        vars_cleanup(&globalvars);
        vars_cleanup(&myvars);
    }

    *n_tests_ptr = n_tests;
    *n_fails_ptr = n_fails;
    return 0;
}


int main(int n_args, char *args[]){
    int err;

    int n_tests, n_fails;
    err = testrunner(&n_tests, &n_fails);
    if(err){
        fprintf(stderr, "### ! TEST SUITE FAILED WITH ERROR ! ###\n");
        return err;
    }

    if(n_fails > 0){
        fprintf(stderr, "### ! %i/%i TESTS FAILED ! ###\n", n_fails, n_tests);
    }else{
        fprintf(stderr, "### %i TESTS OK ###\n", n_tests);
    }

    return n_fails > 0? EXIT_FAILURE: EXIT_SUCCESS;
}

