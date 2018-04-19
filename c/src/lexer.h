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
            char *token;
            int token_len;
            int err = fus_lexer_get_word(&lexer, &token, &token_len);
            if(err)return err;

            if(token == NULL)break;
            printf("Lexed: %.*s\n", token_len, token);
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
int fus_lexer_get_word(struct fus_lexer_t *lexer, const char **token, int *token_len);


#endif