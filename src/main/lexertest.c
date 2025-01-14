#include "../lexer.h"

static bool token_cmp(
    size_t len1, const char *token1,
    size_t len2, const char *token2
){
    return len1 == len2 && !strncmp(token1, token2, len1);
}

static void print_lexer(fus_lexer_t *lexer, const char *msg){
    fputs(msg, stderr);
    fus_lexer_show(lexer, stderr);
    fputc('\n', stderr);
}

static int _run_test(const char *test_text, const char *expected_text){
    int err;

    fprintf(stderr, "Test: %s\n", test_text);
    fprintf(stderr, "Expected: %s\n", expected_text);

    fus_lexer_t test_lexer, expected_lexer;
    err = fus_lexer_init_with_vars(&test_lexer,
        test_text, "<test>", NULL);
    if(err)return err;
    err = fus_lexer_init_with_vars(&expected_lexer,
        expected_text, "<expected>", NULL);
    if(err)return err;

    while(1){
        bool test_done = fus_lexer_done(&test_lexer);
        bool expected_done = fus_lexer_done(&expected_lexer);
        if(test_done & !expected_done){
            fprintf(stderr, "  Test finished early!\n");
            print_lexer(&expected_lexer, "  Expected: ");
            return 2;
        }else if(!test_done & expected_done){
            fprintf(stderr, "  Expected finished early!\n");
            print_lexer(&test_lexer    , "  Test:     ");
            return 2;
        }else if(test_done && expected_done){
            break;
        }
        if(!token_cmp(
            test_lexer.token_len, test_lexer.token,
            expected_lexer.token_len, expected_lexer.token
        )){
            fprintf(stderr, "  Lexed different tokens!\n");
            print_lexer(&test_lexer    , "  Test:     ");
            print_lexer(&expected_lexer, "  Expected: ");
            return 2;
        }

        print_lexer(&test_lexer, "  Lexed: ");

        err = fus_lexer_next(&test_lexer);
        if(err)return err;
        err = fus_lexer_next(&expected_lexer);
        if(err)return err;
    }

    fus_lexer_cleanup(&test_lexer);
    fus_lexer_cleanup(&expected_lexer);

    return 0;
}

static int run_test(const char *test_text, const char *expected_text){
    int err = _run_test(test_text, expected_text);
    if(err){
        fprintf(stderr, "** FAIL\n");
    }else{
        fprintf(stderr, "OK!\n");
    }
    return err;
}

static int _run_tests(){
    int err;

    /* Basics */
    err = run_test(
        "1 2 (3 4) 5 6",
        "1 2 (3 4) 5 6");
    if(err)return err;

    /* BOOL macros */
    err = run_test(
        "$UNSET_BOOL COND 1 2 $IF COND(3 4) 5 6",
        "1 2 5 6");
    if(err)return err;
    err = run_test(
        "1 2 $UNSET_BOOL COND $IF COND(3 4) 5 6",
        "1 2 5 6");
    if(err)return err;
    err = run_test(
        "$SET_BOOL COND 1 2 $IF COND(3 4) 5 6",
        "1 2 3 4 5 6");
    if(err)return err;
    err = run_test(
        "1 2 $SET_BOOL COND $IF COND(3 4) 5 6",
        "1 2 3 4 5 6");
    if(err)return err;
    err = run_test(
        "$UNSET_BOOL A 1 $IF A($SET_BOOL B) 2 $IF B(3)",
        "1 2");
    if(err)return err;
    err = run_test(
        "$SET_BOOL A 1 $IF A($SET_BOOL B) 2 $IF B(3)",
        "1 2 3");
    if(err)return err;

    /* STR macros */
    err = run_test(
        "1 $STR \"aaa\" 2",
        "1 \"aaa\" 2");
    if(err)return err;
    err = run_test(
        "1 $STR (lines(\"aaa\" \"bbb\")) 2",
        "1 \"aaa\\nbbb\\n\" 2");
    if(err)return err;
    err = run_test(
        "1 $STR (lines(at(2 1) \"aaa\" \"bbb\")) 2",
        "1 \"\\n  aaa\\n  bbb\\n\" 2");
    if(err)return err;
    err = run_test(
        "1 2 $SET_STR X xx $GET_STR X 3 4",
        "1 2 \"xx\" 3 4");
    if(err)return err;
    err = run_test(
        "1 2 $SET_STR X xx $GET_SYM X 3 4",
        "1 2 xx 3 4");
    if(err)return err;
    err = run_test(
        "$SET_STR X xx 1 2 $PREFIX X lala 5 6",
        "1 2 xxlala 5 6");
    if(err)return err;
    err = run_test(
        "$SET_STR X \"xx\" 1 2 $PREFIX X \"lala\" 5 6",
        "1 2 \"xxlala\" 5 6");
    if(err)return err;
    err = run_test(
        "$SET_STR X xx 1 2 $SUFFIX X lala 5 6",
        "1 2 lalaxx 5 6");
    if(err)return err;
    err = run_test(
        "$SET_STR X \"xx\" 1 2 $SUFFIX X \"lala\" 5 6",
        "1 2 \"lalaxx\" 5 6");
    if(err)return err;
    err = run_test(
        "$SET_STR A (\"lala\") $GET_STR A",
        "\"lala\"");
    if(err)return err;
    err = run_test(
        "$SET_STR A (lines (\"a\" \"b\")) $GET_STR A",
        "\"a\\nb\\n\"");
    if(err)return err;
    err = run_test(
        "$SET_STR A (lines (at(2 3) \"a\" \"b\")) $GET_STR A",
        "\"\\n\\n\\n  a\\n  b\\n\"");
    if(err)return err;

    /* INT macros */
    err = run_test(
        "1 2 $GET_INT X 3 4",
        "1 2 0 3 4");
    if(err)return err;
    err = run_test(
        "$SET_INT X 10 1 2 $GET_INT X 3 4",
        "1 2 10 3 4");
    if(err)return err;
    err = run_test(
        "1 2 $SET_INT X 10 $GET_INT X 3 4",
        "1 2 10 3 4");
    if(err)return err;

    /* UNSET macro */
    err = run_test(
        "$SET_INT X 2 $UNSET X 1 $GET_INT X 3",
        "1 0 3");
    if(err)return err;

    /* PRINT macros */
    err = run_test(
        "$SET_STR A aa $PRINTVAR A $SET_STR A aaaa",
        "");
    if(err)return err;
    err = run_test(
        "1 $SET_STR A aa 2 $PRINTVAR A 3 $SET_STR A aaaa 4",
        "1 2 3 4");
    if(err)return err;
    err = run_test(
        "$PRINTVARS $SET_STR A aa $PRINTVARS $PRINTVAR A $PRINTVARS $SET_STR A aaaa $PRINTVARS",
        "");
    if(err)return err;

    /* SKIP macros */
    err = run_test(
        "$SKIP()",
        "");
    if(err)return err;
    err = run_test(
        "$SKIP(1 2 3)",
        "");
    if(err)return err;
    err = run_test(
        "10 $SKIP() 20",
        "10 20");
    if(err)return err;
    err = run_test(
        "10 $SKIP(1 2 3) 20",
        "10 20");
    if(err)return err;
    err = run_test(
        "10 $SKIP((1) (2) (3)) 20",
        "10 20");
    if(err)return err;
    err = run_test(
        "vars: $SKIP:\n"
        "    : \"hello\"\n"
        "    : \"world\"\n"
        "numbers: $SKIP:\n"
        "    : 1\n"
        "    : 2\n"
        ,
        "vars() numbers()");
    if(err)return err;

    return 0;
}

static int run_tests(){
    int err = _run_tests();
    if(err){
        fprintf(stderr, "*** SUITE FAILED! ***\n");
    }else{
        fprintf(stderr, "SUITE OK!\n");
    }
    return err;
}

int main(int n_args, char **args){
    int err;

    return run_tests();

    fus_lexer_t lexer;
    err = fus_lexer_init_with_vars(&lexer,
        "1 2 $UNSET_BOOL COND $IF COND(3 4) 5 6",
        "<fake file>", NULL);
    if(err)return err;

    while(1){
        if(fus_lexer_done(&lexer))break;
        printf("Lexed: ");
        fus_lexer_show(&lexer, stdout);
        printf("\n");
        int err = fus_lexer_next(&lexer);
        if(err)return err;
    }

    return 0;
}
