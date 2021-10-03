#ifndef _VALEXPR_H_
#define _VALEXPR_H_

#include <stdbool.h>

#include "lexer.h"
#include "vars.h"


enum valexpr_type {
    VALEXPR_TYPE_LITERAL,
    VALEXPR_TYPE_YOURVAR,
    VALEXPR_TYPE_MAPVAR,
    VALEXPR_TYPE_GLOBALVAR,
    VALEXPR_TYPE_MYVAR,
    VALEXPR_TYPE_IF,
    VALEXPR_TYPES
};

static const char *valexpr_type_msg(int type){
    switch(type){
        case VALEXPR_TYPE_LITERAL: return "literal";
        case VALEXPR_TYPE_YOURVAR: return "yourvar";
        case VALEXPR_TYPE_MAPVAR: return "mapvar";
        case VALEXPR_TYPE_GLOBALVAR: return "globalvar";
        case VALEXPR_TYPE_MYVAR: return "myvar";
        case VALEXPR_TYPE_IF: return "if";
        default: return "unknown";
    }
}


typedef struct valexpr {
    /* An expression specifying a val_t */
    int type; /* enum valexpr_type */
    union {
        val_t val;
        struct valexpr *key_expr;
        struct {
            bool cond_not; /* Inverts cond_expr's value */
            struct valexpr *cond_expr;
            struct valexpr *then_expr;
            struct valexpr *else_expr;
        } if_expr;
    } u;
} valexpr_t;

typedef struct valexpr_context {
    vars_t *yourvars;
    vars_t *mapvars;
    vars_t *globalvars;
    vars_t *myvars;
} valexpr_context_t;


void valexpr_cleanup(valexpr_t *expr);
int valexpr_copy(valexpr_t *expr1, valexpr_t *expr2);
void valexpr_fprintf(valexpr_t *expr, FILE *file);
void valexpr_set_literal_bool(valexpr_t *expr, bool b);
void valexpr_set_literal_int(valexpr_t *expr, int i);
int valexpr_parse(valexpr_t *expr, fus_lexer_t *lexer);
int valexpr_eval(valexpr_t *expr, valexpr_context_t *context,
    val_t **result_ptr, bool set);
int valexpr_get(valexpr_t *expr, valexpr_context_t *context,
    val_t **result_ptr);
int valexpr_set(valexpr_t *expr, valexpr_context_t *context,
    val_t **result_ptr);

bool valexpr_get_bool(valexpr_t *expr, valexpr_context_t *context);
int valexpr_get_int(valexpr_t *expr, valexpr_context_t *context);
const char *valexpr_get_str(valexpr_t *expr, valexpr_context_t *context);

#endif