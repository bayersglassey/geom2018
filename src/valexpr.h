#ifndef _VALEXPR_H_
#define _VALEXPR_H_

#include <stdbool.h>

#include "lexer.h"
#include "vars.h"


enum valexpr_type {
    VALEXPR_TYPE_LITERAL,
    VALEXPR_TYPE_MAPVAR,
    VALEXPR_TYPE_MYVAR,
    VALEXPR_TYPES
};


typedef struct valexpr {
    /* An expression specifying a val_t */
    int type; /* enum valexpr_type */
    union {
        val_t val;
        struct valexpr *key_expr;
    } u;
} valexpr_t;


void valexpr_cleanup(valexpr_t *expr);
int valexpr_parse(valexpr_t *expr, fus_lexer_t *lexer);
int valexpr_eval(valexpr_t *expr, vars_t *mapvars, vars_t *myvars,
    val_t **result_ptr, bool set);

#endif