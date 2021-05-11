#ifndef _FUS_LEXER_H_
#define _FUS_LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "vars.h"

/*
    Lexer usage example:

        fus_lexer_t lexer;
        int err = fus_lexer_init(&lexer, "1 2 (3 4) 5", "<fake file>");
        if(err)return err;

        while(1){
            if(fus_lexer_done(&lexer))break;
            printf("Lexed: ");
            fus_lexer_show(&lexer, stdout);
            printf("\n");
            int err = fus_lexer_next(&lexer);
            if(err)return err;
        }

        fus_lexer_cleanup(&lexer);

*/

typedef enum fus_lexer_token_type {
    FUS_LEXER_TOKEN_DONE,
    FUS_LEXER_TOKEN_INT,
    FUS_LEXER_TOKEN_SYM,
    FUS_LEXER_TOKEN_OP,
    FUS_LEXER_TOKEN_STR,
    FUS_LEXER_TOKEN_BLOCKSTR,
    FUS_LEXER_TOKEN_OPEN,
    FUS_LEXER_TOKEN_CLOSE,
    FUS_LEXER_TOKEN_TYPES
} fus_lexer_token_type_t;

#define FUS_LEXER_TOKEN_TYPE_MSGS \
    "done", "int", "sym", "op", "str", "blockstr", \
    "open", "close" \

typedef enum fus_lexer_frame_type {
    FUS_LEXER_FRAME_NORMAL,
    FUS_LEXER_FRAME_IF,
} fus_lexer_frame_type_t;

typedef struct fus_lexer_frame {
    fus_lexer_frame_type_t type;
    int indent;
    bool is_block;
    struct fus_lexer_frame *next;
} fus_lexer_frame_t;

typedef struct fus_lexer {
    vars_t _vars;
    vars_t *vars;

    const char *filename;

    int text_len;
    const char *text;

    /* whitespace: pos+len of the whitespace preceding the token
    we just parsed */
    int whitespace_pos;
    int whitespace_len;

    /* token_start is usually whitespace_pos + whitespace_len... unless we
    parsed one or more macros, in which case token_start is the start of
    the "actual" token (after the macros). */
    int token_start;

    /* is_macro_token: whether this token was generated by a macro */
    bool is_macro_token;

    /* _token_len: the "true" length of the token, e.g. if token was
    dynamically generated by $PREFIX */
    int token_len;
    int _token_len;
    const char *token;
    fus_lexer_token_type_t token_type;

    /* mem_managed_token: when a token is constructed dynamically, e.g. by the
    $PREFIX macro, it is stored in mem_managed_token and token is set to that. */
    char *mem_managed_token;

    int pos;
    int row;
    int col;
    int indent;

    int returning_indents;

    fus_lexer_frame_t *frame_list;
    fus_lexer_frame_t *free_frame_list;
} fus_lexer_t;


void fus_lexer_cleanup(fus_lexer_t *lexer);
int fus_lexer_init(fus_lexer_t *lexer, const char *text,
    const char *filename);
int fus_lexer_init_with_vars(fus_lexer_t *lexer, const char *text,
    const char *filename, vars_t *vars);
const char *fus_lexer_token_type_msg(fus_lexer_token_type_t token_type);
void fus_lexer_info(fus_lexer_t *lexer, FILE *f);
void fus_lexer_err_info(fus_lexer_t *lexer);
int fus_lexer_get_pos(fus_lexer_t *lexer);
void fus_lexer_set_pos(fus_lexer_t *lexer, int pos);
int fus_lexer_next(fus_lexer_t *lexer);
bool fus_lexer_done(fus_lexer_t *lexer);
bool fus_lexer_got(fus_lexer_t *lexer, const char *text);
bool fus_lexer_got_chr(fus_lexer_t *lexer);
bool fus_lexer_got_name(fus_lexer_t *lexer);
bool fus_lexer_got_str(fus_lexer_t *lexer);
bool fus_lexer_got_int(fus_lexer_t *lexer);
void fus_lexer_show(fus_lexer_t *lexer, FILE *f);
int _fus_lexer_get(fus_lexer_t *lexer, const char *text);
int fus_lexer_get(fus_lexer_t *lexer, const char *text);
int _fus_lexer_get_name(fus_lexer_t *lexer, char **name);
int fus_lexer_get_name(fus_lexer_t *lexer, char **name);
int _fus_lexer_get_str(fus_lexer_t *lexer, char **s);
int fus_lexer_get_str(fus_lexer_t *lexer, char **s);
int _fus_lexer_get_str_fancy(fus_lexer_t *lexer, char **s);
int fus_lexer_get_str_fancy(fus_lexer_t *lexer, char **s);
int _fus_lexer_get_name_or_str(fus_lexer_t *lexer, char **s);
int fus_lexer_get_name_or_str(fus_lexer_t *lexer, char **s);
int fus_lexer_get_chr(fus_lexer_t *lexer, char *c);
int _fus_lexer_get_int(fus_lexer_t *lexer, int *i);
int fus_lexer_get_int(fus_lexer_t *lexer, int *i);
int fus_lexer_get_bool(fus_lexer_t *lexer, bool *b);
int fus_lexer_get_yn(fus_lexer_t *lexer, bool *b);
int fus_lexer_get_yesno(fus_lexer_t *lexer, bool *b);
int fus_lexer_get_int_fancy(fus_lexer_t *lexer, int *i_ptr);
int fus_lexer_get_int_range(fus_lexer_t *lexer, int maxlen,
    int *i_ptr, int *len_ptr);
int fus_lexer_get_attr_int(fus_lexer_t *lexer, const char *attr, int *i,
    bool optional);
int fus_lexer_get_attr_str(fus_lexer_t *lexer, const char *attr, char **s,
    bool optional);
int fus_lexer_get_attr_yn(fus_lexer_t *lexer, const char *attr, bool *b,
    bool optional);
int fus_lexer_get_attr_yesno(fus_lexer_t *lexer, const char *attr, bool *b,
    bool optional);
int fus_lexer_get_attr_bool(fus_lexer_t *lexer, const char *attr, bool *b,
    bool optional);
int fus_lexer_unexpected(fus_lexer_t *lexer, const char *expected);
int fus_lexer_parse_silent(fus_lexer_t *lexer);

#endif