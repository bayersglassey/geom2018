
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
        case VALEXPR_TYPE_GLOBALVAR:
        case VALEXPR_TYPE_MYVAR:
            valexpr_cleanup(expr->u.key_expr);
            free(expr->u.key_expr);
            break;
        case VALEXPR_TYPE_IF:
            valexpr_cond_cleanup(expr->u.if_expr.cond);
            free(expr->u.if_expr.cond);
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
        case VALEXPR_TYPE_GLOBALVAR:
        case VALEXPR_TYPE_MYVAR:
            expr1->u.key_expr = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.key_expr)return 1;

            err = valexpr_copy(expr1->u.key_expr, expr2->u.key_expr);
            if(err)return err;
            break;
        case VALEXPR_TYPE_IF:
            expr1->u.if_expr.cond = calloc(1, sizeof(valexpr_cond_t));
            if(!expr1->u.if_expr.cond)return 1;
            expr1->u.if_expr.then_expr = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.if_expr.then_expr)return 1;
            expr1->u.if_expr.else_expr = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.if_expr.else_expr)return 1;

            err = valexpr_cond_copy(expr1->u.if_expr.cond,
                expr2->u.if_expr.cond);
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
        case VALEXPR_TYPE_GLOBALVAR:
        case VALEXPR_TYPE_MYVAR:
            fprintf(file, "%s(", valexpr_type_msg(expr->type));
            valexpr_fprintf(expr->u.key_expr, file);
            fputc(')', file);
            break;
        case VALEXPR_TYPE_IF:
            fprintf(file, "if ");
            valexpr_cond_fprintf(expr->u.if_expr.cond, file);
            fprintf(file, " then ");
            valexpr_fprintf(expr->u.if_expr.then_expr, file);
            fprintf(file, " else ");
            valexpr_fprintf(expr->u.if_expr.else_expr, file);
            break;
        default:
            fprintf(file, "<unknown>");
            break;
    }
}

void valexpr_set_literal_null(valexpr_t *expr){
    expr->type = VALEXPR_TYPE_LITERAL;
    expr->u.val.type = VAL_TYPE_NULL;
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

void valexpr_set_literal_str(valexpr_t *expr, const char *cs){
    expr->type = VALEXPR_TYPE_LITERAL;
    expr->u.val.type = VAL_TYPE_CONST_STR;
    expr->u.val.u.cs = cs;
}

int valexpr_parse(valexpr_t *expr, fus_lexer_t *lexer){
    INIT
    int type =
        GOT("yourvar")? VALEXPR_TYPE_YOURVAR:
        GOT("mapvar")? VALEXPR_TYPE_MAPVAR:
        GOT("globalvar")? VALEXPR_TYPE_GLOBALVAR:
        GOT("myvar")? VALEXPR_TYPE_MYVAR:
        GOT("if")? VALEXPR_TYPE_IF:
        VALEXPR_TYPE_LITERAL;

    if(
        type == VALEXPR_TYPE_YOURVAR ||
        type == VALEXPR_TYPE_MAPVAR ||
        type == VALEXPR_TYPE_GLOBALVAR ||
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
        expr->u.if_expr.cond = NULL;
        expr->u.if_expr.then_expr = NULL;
        expr->u.if_expr.else_expr = NULL;

        valexpr_cond_t *cond = calloc(1, sizeof(*cond));
        if(!cond)return 1;
        err = valexpr_cond_parse(cond, lexer);
        if(err)return err;
        expr->u.if_expr.cond = cond;

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

    /* To be returned. It *MAY BE NULL* if e.g. we can't find the variable
    referred to! */
    val_t *result = NULL;

    switch(expr->type){
        case VALEXPR_TYPE_LITERAL: {
            result = &expr->u.val;
            break;
        }
        case VALEXPR_TYPE_YOURVAR:
        case VALEXPR_TYPE_MAPVAR:
        case VALEXPR_TYPE_GLOBALVAR:
        case VALEXPR_TYPE_MYVAR: {
            vars_t *vars =
                expr->type == VALEXPR_TYPE_YOURVAR? context->yourvars:
                expr->type == VALEXPR_TYPE_MAPVAR? context->mapvars:
                expr->type == VALEXPR_TYPE_GLOBALVAR? context->globalvars:
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
            bool cond;
            err = valexpr_cond_eval(expr->u.if_expr.cond, context, &cond);
            if(err)return err;

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




void valexpr_cond_init(valexpr_cond_t *cond, int type, bool not){
    cond->type = type;
    cond->not = not;
    switch(cond->type){
        case VALEXPR_COND_TYPE_EXPR:
            valexpr_set_literal_null(&cond->u.expr);
            break;
        case VALEXPR_COND_TYPE_ANY:
        case VALEXPR_COND_TYPE_ALL:
            ARRAY_INIT(cond->u.subconds.subconds)
            break;
        default: break;
    }
}

void valexpr_cond_cleanup(valexpr_cond_t *cond){
    switch(cond->type){
        case VALEXPR_COND_TYPE_EXPR:
            valexpr_cleanup(&cond->u.expr);
            break;
        case VALEXPR_COND_TYPE_ANY:
        case VALEXPR_COND_TYPE_ALL: {
            for(int i = 0; i < cond->u.subconds.subconds_len; i++){
                valexpr_cond_t *subcond = cond->u.subconds.subconds[i];
                valexpr_cond_cleanup(subcond);
            }
            break;
        }
        default: break;
    }
}

int valexpr_cond_copy(valexpr_cond_t *cond1, valexpr_cond_t *cond2){
    int err;
    cond1->type = cond2->type;
    cond1->not = cond2->not;
    switch(cond1->type){
        case VALEXPR_COND_TYPE_EXPR: {
            err = valexpr_copy(&cond1->u.expr, &cond2->u.expr);
            if(err)return err;
            break;
        }
        case VALEXPR_COND_TYPE_ANY:
        case VALEXPR_COND_TYPE_ALL: {
            for(int i = 0; i < cond2->u.subconds.subconds_len; i++){
                valexpr_cond_t *subcond = cond2->u.subconds.subconds[i];
                ARRAY_PUSH_NEW(valexpr_cond_t *, cond1->u.subconds.subconds,
                    new_subcond)
                err = valexpr_cond_copy(new_subcond, subcond);
                if(err)return err;
            }
            break;
        }
        default: break;
    }
    return 0;
}

void valexpr_cond_fprintf(valexpr_cond_t *cond, FILE *file){
    switch(cond->type){
        case VALEXPR_COND_TYPE_EXPR:
            valexpr_fprintf(&cond->u.expr, file);
            break;
        case VALEXPR_COND_TYPE_ANY:
        case VALEXPR_COND_TYPE_ALL: {
            fputs(cond->type == VALEXPR_COND_TYPE_ANY? "any": "all", file);
            fputc('(', file);
            for(int i = 0; i < cond->u.subconds.subconds_len; i++){
                if(i > 0)fputc(' ', file);
                valexpr_cond_t *subcond = cond->u.subconds.subconds[i];
                valexpr_cond_fprintf(subcond, file);
            }
            fputc(')', file);
            break;
        }
        default:
            fprintf(file, "<unknown>");
            break;
    }
}

int valexpr_cond_parse(valexpr_cond_t *cond, fus_lexer_t *lexer){
    INIT

    bool not = false;
    if(GOT("not")){
        NEXT
        not = true;
    }

    if(GOT("any") || GOT("all")){
        int type = GOT("all")? VALEXPR_COND_TYPE_ALL: VALEXPR_COND_TYPE_ANY;
        NEXT

        valexpr_cond_init(cond, type, not);

        GET("(")
        while(1){
            if(GOT(")"))break;
            ARRAY_PUSH_NEW(valexpr_cond_t*, cond->u.subconds.subconds,
                new_cond)
            err = valexpr_cond_parse(new_cond, lexer);
            if(err)return err;
        }
        NEXT
    }else{
        valexpr_cond_init(cond, VALEXPR_COND_TYPE_EXPR, not);
        err = valexpr_parse(&cond->u.expr, lexer);
        if(err)return err;
    }

    return 0;
}

int valexpr_cond_eval(valexpr_cond_t *cond, valexpr_context_t *context,
    bool *result_ptr
){
    int err;

    bool result;
    switch(cond->type){
        case VALEXPR_COND_TYPE_EXPR:
            result = valexpr_get_bool(&cond->u.expr, context);
            break;
        case VALEXPR_COND_TYPE_ANY: {
            result = false;
            for(int i = 0; i < cond->u.subconds.subconds_len; i++){
                valexpr_cond_t *subcond = cond->u.subconds.subconds[i];
                bool subresult;
                err = valexpr_cond_eval(subcond, context, &subresult);
                if(err)return err;
                if(subresult){
                    result = true;
                    break;
                }
            }
            break;
        }
        case VALEXPR_COND_TYPE_ALL: {
            result = true;
            for(int i = 0; i < cond->u.subconds.subconds_len; i++){
                valexpr_cond_t *subcond = cond->u.subconds.subconds[i];
                bool subresult;
                err = valexpr_cond_eval(subcond, context, &subresult);
                if(err)return err;
                if(!subresult){
                    result = false;
                    break;
                }
            }
            break;
        }
        default:
            fprintf(stderr, "Unrecognized cond type: %i\n", cond->type);
            return 2;
    }

    *result_ptr = cond->not? !result: result;
    return 0;
}

bool valexpr_cond_get_bool(valexpr_cond_t *cond, valexpr_context_t *context){
    bool result;
    int err = valexpr_cond_eval(cond, context, &result);
    if(err){
        fprintf(stderr, "Error while getting bool from valexpr_cond\n");
        return false;
    }
    return result;
}
