#ifndef _FUS_LEXER_H_
#define _FUS_LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/*
    Lexer usage example:

        fus_lexer_t lexer;
        int err = fus_lexer_init(&lexer, "1 2 (3 4) 5");
        if(err)return err;

        while(1){
            int err = fus_lexer_next(&lexer);
            if(err)return err;

            if(fus_lexer_done(&lexer))break;
            printf("Lexed: ");
            fus_lexer_show(&lexer, stdout);
            printf("\n");
        }

*/

typedef struct fus_lexer {
    bool debug;

    int text_len;
    const char *text;

    int token_len;
    const char *token;
    enum {
        FUS_LEXER_TOKEN_DONE,
        FUS_LEXER_TOKEN_INT,
        FUS_LEXER_TOKEN_SYM,
        FUS_LEXER_TOKEN_OP,
        FUS_LEXER_TOKEN_STR,
        FUS_LEXER_TOKEN_OPEN,
        FUS_LEXER_TOKEN_CLOSE,
        FUS_LEXER_TOKEN_TYPES
    } token_type;

    int pos;
    int row;
    int col;
    int indent;
    int indents_size;
    int n_indents;
    int *indents;

    /* If positive, represents a series of "(" tokens being returned.
    If negative, represents a series of ")" tokens being returned. */
    int returning_indents;
} fus_lexer_t;


int fus_lexer_init(fus_lexer_t *lexer, const char *text);
void fus_lexer_dump(fus_lexer_t *lexer, FILE *f);
void fus_lexer_err_info(fus_lexer_t *lexer);
int fus_lexer_next(fus_lexer_t *lexer);
bool fus_lexer_done(fus_lexer_t *lexer);
int fus_lexer_got(fus_lexer_t *lexer, const char *text);
void fus_lexer_show(fus_lexer_t *lexer, FILE *f);
int fus_lexer_get_name(fus_lexer_t *lexer, char **name);
int fus_lexer_get_int(fus_lexer_t *lexer, int *i);
int fus_lexer_expect(fus_lexer_t *lexer, const char *text);
int fus_lexer_expect_name(fus_lexer_t *lexer, char **name);
int fus_lexer_expect_int(fus_lexer_t *lexer, int *i);
int fus_lexer_unexpected(fus_lexer_t *lexer);
int fus_lexer_parse_silent(fus_lexer_t *lexer);

#endif