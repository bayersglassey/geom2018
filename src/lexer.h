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
            if(fus_lexer_done(&lexer))break;
            printf("Lexed: ");
            fus_lexer_show(&lexer, stdout);
            printf("\n");
            int err = fus_lexer_next(&lexer);
            if(err)return err;
        }

*/

typedef struct fus_lexer {
    bool debug;

    const char *filename;

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
        FUS_LEXER_TOKEN_BLOCKSTR,
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


void fus_lexer_cleanup(fus_lexer_t *lexer);
int fus_lexer_init(fus_lexer_t *lexer, const char *text,
    const char *filename);
int fus_lexer_copy(fus_lexer_t *lexer, fus_lexer_t *lexer2);
void fus_lexer_dump(fus_lexer_t *lexer, FILE *f);
void fus_lexer_info(fus_lexer_t *lexer, FILE *f);
void fus_lexer_err_info(fus_lexer_t *lexer);
int fus_lexer_get_pos(fus_lexer_t *lexer);
void fus_lexer_set_pos(fus_lexer_t *lexer, int pos);
int fus_lexer_next(fus_lexer_t *lexer);
bool fus_lexer_done(fus_lexer_t *lexer);
bool fus_lexer_got(fus_lexer_t *lexer, const char *text);
bool fus_lexer_got_name(fus_lexer_t *lexer);
bool fus_lexer_got_str(fus_lexer_t *lexer);
bool fus_lexer_got_int(fus_lexer_t *lexer);
void fus_lexer_show(fus_lexer_t *lexer, FILE *f);
int fus_lexer_get(fus_lexer_t *lexer, const char *text);
int fus_lexer_get_name(fus_lexer_t *lexer, char **name);
int fus_lexer_get_str(fus_lexer_t *lexer, char **s);
int fus_lexer_get_chr(fus_lexer_t *lexer, char *c);
int fus_lexer_get_int(fus_lexer_t *lexer, int *i);
int fus_lexer_get_bool(fus_lexer_t *lexer, bool *b);
int fus_lexer_get_int_fancy(fus_lexer_t *lexer, int *i_ptr);
int fus_lexer_get_int_range(fus_lexer_t *lexer, int maxlen,
    int *i_ptr, int *len_ptr);
int fus_lexer_get_attr_int(fus_lexer_t *lexer, const char *attr, int *i,
    bool optional);
int fus_lexer_get_attr_bool(fus_lexer_t *lexer, const char *attr, bool *b,
    bool optional);
int fus_lexer_unexpected(fus_lexer_t *lexer, const char *expected);
int fus_lexer_parse_silent(fus_lexer_t *lexer);

#endif