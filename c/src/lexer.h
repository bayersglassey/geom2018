#ifndef _FUS_LEXER_H_
#define _FUS_LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/*
    Lexer usage example:

        struct fus_lexer_t lexer;
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

struct fus_lexer_t {
    bool debug;

    int text_len;
    const char *text;

    int token_len;
    const char *token;

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
};


int fus_lexer_init(struct fus_lexer_t *lexer, const char *text);
int fus_lexer_next(struct fus_lexer_t *lexer);
bool fus_lexer_done(struct fus_lexer_t *lexer);
int fus_lexer_got(struct fus_lexer_t *lexer, const char *text);
void fus_lexer_show(struct fus_lexer_t *lexer, FILE *f);
int fus_lexer_expect(struct fus_lexer_t *lexer, const char *text);
int fus_lexer_unexpected(struct fus_lexer_t *lexer);

#endif