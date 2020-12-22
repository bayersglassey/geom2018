
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
        case VALEXPR_TYPE_MAPVAR:
        case VALEXPR_TYPE_MYVAR:
            valexpr_cleanup(expr->u.key_expr);
            free(expr->u.key_expr);
            break;
        default: break;
    }
}


int valexpr_parse(valexpr_t *expr, fus_lexer_t *lexer){
    INIT
    int type =
	GOT("mapvar")? VALEXPR_TYPE_MAPVAR:
        GOT("myvar")? VALEXPR_TYPE_MYVAR:
        VALEXPR_TYPE_LITERAL;

    if(type == VALEXPR_TYPE_MAPVAR || type == VALEXPR_TYPE_MYVAR){
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
    }else{
	expr->type = type;
        err = val_parse(&expr->u.val, lexer);
        if(err)return err;
    }
    return 0;
}


int valexpr_eval(valexpr_t *expr, vars_t *mapvars, vars_t *myvars,
    val_t **result_ptr, bool set
){
    int err;
    val_t *result = NULL;
    switch(expr->type){
        case VALEXPR_TYPE_LITERAL: {
            result = &expr->u.val;
            break;
        }
        case VALEXPR_TYPE_MAPVAR:
        case VALEXPR_TYPE_MYVAR: {
            vars_t *vars = expr->type == VALEXPR_TYPE_MAPVAR?
                mapvars: myvars;

            if(!vars){
                fprintf(stderr, "valexpr_eval: No \"%s\" vars provided\n",
                    valexpr_type_msg(expr->type));
                return 2;
            }

            val_t *key_val;
            err = valexpr_eval(expr->u.key_expr,
                mapvars, myvars, &key_val, false);
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
        default: {
            fprintf(stderr,
                "valexpr_eval: Unknown expr type: %i\n", expr->type);
            return 2;
        }
    }
    *result_ptr = result;
    return 0;
}


int valexpr_get(valexpr_t *expr, vars_t *mapvars, vars_t *myvars,
    val_t **result_ptr
){
    /* NOTE: Caller needs to check whether *result_ptr is NULL. */
    return valexpr_eval(expr, mapvars, myvars, result_ptr, false);
}

int valexpr_set(valexpr_t *expr, vars_t *mapvars, vars_t *myvars,
    val_t **result_ptr
){
    /* NOTE: valexpr_set guarantees that we find (or create, if necessary)
    a val, so caller doesn't need to check whether *result_ptr is NULL. */
    return valexpr_eval(expr, mapvars, myvars, result_ptr, true);
}



#define VALEXPR_GET_TYPE(TYPE, VAL_TYPE) \
TYPE valexpr_get_##VAL_TYPE(valexpr_t *expr, \
    vars_t *mapvars, vars_t *myvars \
){ \
    val_t *result; \
    int err = valexpr_get(expr, mapvars, myvars, &result); \
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
