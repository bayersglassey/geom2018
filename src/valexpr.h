#ifndef _VALEXPR_H_
#define _VALEXPR_H_

#include <stdbool.h>

#include "lexer.h"
#include "vars.h"
#include "array.h"


enum valexpr_type {
    VALEXPR_TYPE_LITERAL,
    VALEXPR_TYPE_YOURVAR,
    VALEXPR_TYPE_MAPVAR,
    VALEXPR_TYPE_GLOBALVAR,
    VALEXPR_TYPE_MYVAR,
    VALEXPR_TYPE_IF,
    VALEXPR_TYPE_AS,
    VALEXPR_TYPE_BINOP,
    VALEXPR_TYPE_UNOP,
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
        case VALEXPR_TYPE_AS: return "as";
        case VALEXPR_TYPE_BINOP: return "binop";
        case VALEXPR_TYPE_UNOP: return "unop";
        default: return "unknown";
    }
}


enum valexpr_cond_type {
    VALEXPR_COND_TYPE_EXPR,
    VALEXPR_COND_TYPE_ANY,
    VALEXPR_COND_TYPE_ALL,
    VALEXPR_COND_TYPES
};

static const char *valexpr_cond_type_msg(int type){
    switch(type){
        case VALEXPR_COND_TYPE_EXPR: return "expr";
        case VALEXPR_COND_TYPE_ANY: return "any";
        case VALEXPR_COND_TYPE_ALL: return "all";
        default: return "unknown";
    }
}


enum valexpr_as {
    VALEXPR_AS_YOU
};

static const char *valexpr_as_msg(int type){
    switch(type){
        case VALEXPR_AS_YOU: return "you";
        default: return "unknown";
    }
}


enum valexpr_op {
    VALEXPR_OP_EQ,
    VALEXPR_OP_NE,
    VALEXPR_OP_LT,
    VALEXPR_OP_LE,
    VALEXPR_OP_GT,
    VALEXPR_OP_GE,
    VALEXPR_OP_AND,
    VALEXPR_OP_OR,
    VALEXPR_OP_NOT,
    VALEXPR_OP_LEN,
    VALEXPR_OP_GET,
};

static const char *valexpr_op_msg(int op){
    switch(op){
        case VALEXPR_OP_EQ: return "==";
        case VALEXPR_OP_NE: return "!=";
        case VALEXPR_OP_LT: return "<";
        case VALEXPR_OP_LE: return "<=";
        case VALEXPR_OP_GT: return ">";
        case VALEXPR_OP_GE: return ">=";
        case VALEXPR_OP_AND: return "&&";
        case VALEXPR_OP_OR: return "||";
        case VALEXPR_OP_NOT: return "!";
        case VALEXPR_OP_LEN: return "len";
        case VALEXPR_OP_GET: return "get";
        default: return "???";
    }
}


typedef struct valexpr {
    /* An expression specifying a val_t */
    int type; /* enum valexpr_type */
    union {
        val_t val;
        struct valexpr *key_expr;
        struct {
            struct valexpr_cond *cond;
            struct valexpr *then_expr;
            struct valexpr *else_expr;
        } if_expr;
        struct {
            int type; /* enum valexpr_as */
            struct valexpr *sub_expr;
        } as;
        struct {
            int op; /* enum valexpr_op */
            struct valexpr *sub_expr1;
            struct valexpr *sub_expr2;
        } op;
    } u;
} valexpr_t;

typedef struct valexpr_cond {
    int type; /* enum valexpr_cond_type */
    bool not; /* Inverts cond's value */
    union {
        struct valexpr expr;
        struct {
            ARRAY_DECL(struct valexpr_cond*, subconds)
        } subconds;
    } u;
} valexpr_cond_t;


typedef struct valexpr_context {
    vars_t *yourvars;
    vars_t *mapvars;
    vars_t *globalvars;
    vars_t *myvars;
} valexpr_context_t;


typedef struct valexpr_result {
    /* May be NULL. */
    val_t *val;

    /* The following are both NULL if val is NULL.
    If val is not NULL, the following are either both NULL, or both
    non-NULL. */
    vars_t *vars;
    var_t *var;

} valexpr_result_t;


void valexpr_cleanup(valexpr_t *expr);
int valexpr_copy(valexpr_t *expr1, valexpr_t *expr2);
void valexpr_fprintf(valexpr_t *expr, FILE *file);
void valexpr_set_literal_null(valexpr_t *expr);
void valexpr_set_literal_bool(valexpr_t *expr, bool b);
void valexpr_set_literal_int(valexpr_t *expr, int i);
void valexpr_set_literal_str(valexpr_t *expr, const char *s);
int valexpr_parse(valexpr_t *expr, fus_lexer_t *lexer);
int valexpr_eval(valexpr_t *expr, valexpr_context_t *context,
    valexpr_result_t *result, bool set);
int valexpr_get(valexpr_t *expr, valexpr_context_t *context,
    valexpr_result_t *result);
int valexpr_set(valexpr_t *expr, valexpr_context_t *context,
    valexpr_result_t *result);

bool valexpr_get_bool(valexpr_t *expr, valexpr_context_t *context);
int valexpr_get_int(valexpr_t *expr, valexpr_context_t *context);
const char *valexpr_get_str(valexpr_t *expr, valexpr_context_t *context);


void valexpr_cond_init(valexpr_cond_t *cond, int type, bool not);
void valexpr_cond_cleanup(valexpr_cond_t *cond);
int valexpr_cond_copy(valexpr_cond_t *cond1, valexpr_cond_t *cond2);
void valexpr_cond_fprintf(valexpr_cond_t *cond, FILE *file);
int valexpr_cond_parse(valexpr_cond_t *cond, fus_lexer_t *lexer);
int valexpr_cond_eval(valexpr_cond_t *cond, valexpr_context_t *context,
    bool *result_ptr);
bool valexpr_cond_get_bool(valexpr_cond_t *cond, valexpr_context_t *context);

#endif
