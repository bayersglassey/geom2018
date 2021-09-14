
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "lexer_macros.h"
#include "vars.h"
#include "var_utils.h"
#include "valexpr.h"


void valexpr_cleanup(valexpr_t *expr){
    switch(expr->type){
        case VALEXPR_TYPE_LITERAL:
            val_cleanup(&expr->u.val);
            break;
        case VALEXPR_TYPE_YOURVAR:
        case VALEXPR_TYPE_MAPVAR:
        case VALEXPR_TYPE_MYVAR:
            valexpr_cleanup(expr->u.key_expr);
            free(expr->u.key_expr);
            break;
        case VALEXPR_TYPE_IF:
            valexpr_cleanup(expr->u.if_expr.cond_expr);
            free(expr->u.if_expr.cond_expr);
            valexpr_cleanup(expr->u.if_expr.then_expr);
            free(expr->u.if_expr.then_expr);
            valexpr_cleanup(expr->u.if_expr.else_expr);
            free(expr->u.if_expr.else_expr);
            break;
        default: break;
    }
}

int valexpr_copy(valexpr_t *expr1, valexpr_t *expr2){
    int err;
    expr1->type = expr2->type;
    switch(expr2->type){
        case VALEXPR_TYPE_LITERAL:
            err = val_copy(&expr1->u.val, &expr2->u.val);
            if(err)return err;
            break;
        case VALEXPR_TYPE_YOURVAR:
        case VALEXPR_TYPE_MAPVAR:
        case VALEXPR_TYPE_MYVAR:
            expr1->u.key_expr = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.key_expr)return 1;

            err = valexpr_copy(expr1->u.key_expr, expr2->u.key_expr);
            if(err)return err;
            break;
        case VALEXPR_TYPE_IF:
            expr1->u.if_expr.cond_expr = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.if_expr.cond_expr)return 1;
            expr1->u.if_expr.then_expr = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.if_expr.then_expr)return 1;
            expr1->u.if_expr.else_expr = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.if_expr.else_expr)return 1;

            err = valexpr_copy(expr1->u.if_expr.cond_expr,
                expr2->u.if_expr.cond_expr);
            if(err)return err;
            err = valexpr_copy(expr1->u.if_expr.then_expr,
                expr2->u.if_expr.then_expr);
            if(err)return err;
            err = valexpr_copy(expr1->u.if_expr.else_expr,
                expr2->u.if_expr.else_expr);
            if(err)return err;
            break;
        default: break;
    }
    return 0;
}

void valexpr_fprintf(valexpr_t *expr, FILE *file){
    switch(expr->type){
        case VALEXPR_TYPE_LITERAL:
            val_fprintf(&expr->u.val, file);
            break;
        case VALEXPR_TYPE_YOURVAR:
        case VALEXPR_TYPE_MAPVAR:
        case VALEXPR_TYPE_MYVAR:
            fprintf(file,
                expr->type == VALEXPR_TYPE_YOURVAR? "yourvar(":
                expr->type == VALEXPR_TYPE_MAPVAR? "mapvar(":
                "myvar(");
            valexpr_fprintf(expr->u.key_expr, file);
            fputc(')', file);
            break;
        case VALEXPR_TYPE_IF:
            fprintf(file, "if ");
            if(expr->u.if_expr.cond_not)fprintf(file, "not ");
            valexpr_fprintf(expr->u.if_expr.cond_expr, file);
            fprintf(file, "then ");
            valexpr_fprintf(expr->u.if_expr.then_expr, file);
            fprintf(file, "else ");
            valexpr_fprintf(expr->u.if_expr.else_expr, file);
            break;
        default:
            fprintf(file, "<unknown>");
            break;
    }
}

void valexpr_set_literal_bool(valexpr_t *expr, bool b){
    expr->type = VALEXPR_TYPE_LITERAL;
    expr->u.val.type = VAL_TYPE_BOOL;
    expr->u.val.u.b = b;
}

void valexpr_set_literal_int(valexpr_t *expr, int i){
    expr->type = VALEXPR_TYPE_LITERAL;
    expr->u.val.type = VAL_TYPE_INT;
    expr->u.val.u.i = i;
}

int valexpr_parse(valexpr_t *expr, fus_lexer_t *lexer){
    INIT
    int type =
        GOT("yourvar")? VALEXPR_TYPE_YOURVAR:
        GOT("mapvar")? VALEXPR_TYPE_MAPVAR:
        GOT("myvar")? VALEXPR_TYPE_MYVAR:
        GOT("if")? VALEXPR_TYPE_IF:
        VALEXPR_TYPE_LITERAL;

    if(
        type == VALEXPR_TYPE_YOURVAR ||
        type == VALEXPR_TYPE_MAPVAR ||
        type == VALEXPR_TYPE_MYVAR
    ){
        NEXT
        expr->type = type;
        expr->u.key_expr = NULL;
        GET("(")

        valexpr_t *key_expr = calloc(1, sizeof(*key_expr));
        if(!key_expr)return 1;

        err = valexpr_parse(key_expr, lexer);
        if(err)return err;

        expr->u.key_expr = key_expr;
        GET(")")
    }else if(type == VALEXPR_TYPE_IF){
        NEXT
        expr->type = type;
        expr->u.if_expr.cond_not = false;
        expr->u.if_expr.cond_expr = NULL;
        expr->u.if_expr.then_expr = NULL;
        expr->u.if_expr.else_expr = NULL;

        if(GOT("not")){
            NEXT
            expr->u.if_expr.cond_not = true;
        }

        valexpr_t *cond_expr = calloc(1, sizeof(*cond_expr));
        if(!cond_expr)return 1;
        err = valexpr_parse(cond_expr, lexer);
        if(err)return err;
        expr->u.if_expr.cond_expr = cond_expr;

        GET("then")

        valexpr_t *then_expr = calloc(1, sizeof(*then_expr));
        if(!then_expr)return 1;
        err = valexpr_parse(then_expr, lexer);
        if(err)return err;
        expr->u.if_expr.then_expr = then_expr;

        GET("else")

        valexpr_t *else_expr = calloc(1, sizeof(*else_expr));
        if(!else_expr)return 1;
        err = valexpr_parse(else_expr, lexer);
        if(err)return err;
        expr->u.if_expr.else_expr = else_expr;
    }else{
        expr->type = type;
        err = val_parse(&expr->u.val, lexer);
        if(err)return err;
    }
    return 0;
}


int valexpr_eval(valexpr_t *expr, valexpr_context_t *context,
    val_t **result_ptr, bool set
){
    int err;
    val_t *result = NULL;
    switch(expr->type){
        case VALEXPR_TYPE_LITERAL: {
            result = &expr->u.val;
            break;
        }
        case VALEXPR_TYPE_YOURVAR:
        case VALEXPR_TYPE_MAPVAR:
        case VALEXPR_TYPE_MYVAR: {
            vars_t *vars =
                expr->type == VALEXPR_TYPE_YOURVAR? context->yourvars:
                expr->type == VALEXPR_TYPE_MAPVAR? context->mapvars:
                context->myvars;

            if(!vars){
                fprintf(stderr, "valexpr_eval: No \"%s\" vars provided\n",
                    valexpr_type_msg(expr->type));
                return 2;
            }

            val_t *key_val;
            err = valexpr_get(expr->u.key_expr, context, &key_val);
            if(err)return err;
            if(!key_val){
                fprintf(stderr,
                    "valexpr_eval: Couldn't get key value\n");
                return 2;
            }

            const char *key = val_get_str(key_val);
            if(!key){
                fprintf(stderr,
                    "valexpr_eval: "
                    "Looking up a var requires a str, but got: ");
                val_fprintf(key_val, stderr);
                fputc('\n', stderr);
                return 2;
            }

            var_t *var;
            if(set){
                /* If we're setting this value, then create the var if
                it doesn't exist yet */
                var = vars_get_or_add(vars, key);
                if(!var)return 1;
            }else{
                /* If we're getting this value... */
                var = vars_get(vars, key);
                if(!var){
                    /* ...if it doesn't exist, return result=NULL */
                    break;
                }
            }

            result = &var->value;
            break;
        }
        case VALEXPR_TYPE_IF: {
            val_t *cond_val;
            err = valexpr_get(expr->u.if_expr.cond_expr,
                context, &cond_val);
            if(err)return err;
            if(!cond_val){
                fprintf(stderr,
                    "valexpr_eval: Couldn't get cond value\n");
                return 2;
            }

            bool cond = val_get_bool(cond_val);
            if(expr->u.if_expr.cond_not)cond = !cond;

            if(cond){
                err = valexpr_eval(expr->u.if_expr.then_expr,
                    context, &result, set);
                if(err)return err;
            }else{
                err = valexpr_eval(expr->u.if_expr.else_expr,
                    context, &result, set);
                if(err)return err;
            }
            break;
        }
        default: {
            fprintf(stderr,
                "valexpr_eval: Unknown expr type: %i\n", expr->type);
            return 2;
        }
    }
    *result_ptr = result;
    return 0;
}


int valexpr_get(valexpr_t *expr, valexpr_context_t *context,
    val_t **result_ptr
){
    /* NOTE: Caller needs to check whether *result_ptr is NULL. */
    return valexpr_eval(expr, context, result_ptr, false);
}

int valexpr_set(valexpr_t *expr, valexpr_context_t *context,
    val_t **result_ptr
){
    /* NOTE: valexpr_set guarantees that we find (or create, if necessary)
    a val, so caller doesn't need to check whether *result_ptr is NULL. */
    return valexpr_eval(expr, context, result_ptr, true);
}



#define VALEXPR_GET_TYPE(TYPE, VAL_TYPE) \
TYPE valexpr_get_##VAL_TYPE(valexpr_t *expr, \
    valexpr_context_t *context \
){ \
    val_t *result; \
    int err = valexpr_get(expr, context, &result); \
    if(err){ \
        fprintf(stderr, "Error while getting int from valexpr\n"); \
        return 0; \
    }else if(!result){ \
        fprintf(stderr, "Val not found while getting int from valexpr\n"); \
        return 0; \
    } \
    return val_get_##VAL_TYPE(result); \
}
VALEXPR_GET_TYPE(bool, bool)
VALEXPR_GET_TYPE(int, int)
VALEXPR_GET_TYPE(const char *, str)
#undef VALEXPR_GET_TYPE
