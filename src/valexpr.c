
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
        case VALEXPR_TYPE_AS:
            valexpr_cleanup(expr->u.as.sub_expr);
            free(expr->u.as.sub_expr);
            break;
        case VALEXPR_TYPE_BINOP:
            valexpr_cleanup(expr->u.op.sub_expr1);
            free(expr->u.op.sub_expr1);
            valexpr_cleanup(expr->u.op.sub_expr2);
            free(expr->u.op.sub_expr2);
            break;
        case VALEXPR_TYPE_UNOP:
            valexpr_cleanup(expr->u.op.sub_expr1);
            free(expr->u.op.sub_expr1);
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
        case VALEXPR_TYPE_AS:
            expr1->u.as.type = expr2->u.as.type;

            expr1->u.as.sub_expr = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.as.sub_expr)return 1;
            err = valexpr_copy(expr1->u.as.sub_expr,
                expr2->u.as.sub_expr);
            if(err)return err;
            break;
        case VALEXPR_TYPE_BINOP:
            expr1->u.op.op = expr2->u.op.op;

            expr1->u.op.sub_expr1 = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.op.sub_expr1)return 1;
            err = valexpr_copy(expr1->u.op.sub_expr1,
                expr2->u.op.sub_expr1);
            if(err)return err;

            expr1->u.op.sub_expr2 = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.op.sub_expr2)return 1;
            err = valexpr_copy(expr1->u.op.sub_expr2,
                expr2->u.op.sub_expr2);
            if(err)return err;
            break;
        case VALEXPR_TYPE_UNOP:
            expr1->u.op.op = expr2->u.op.op;

            expr1->u.op.sub_expr1 = calloc(1, sizeof(valexpr_t));
            if(!expr1->u.op.sub_expr1)return 1;
            err = valexpr_copy(expr1->u.op.sub_expr1,
                expr2->u.op.sub_expr1);
            if(err)return err;
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
        case VALEXPR_TYPE_AS:
            fprintf(file, "as %s (", valexpr_as_msg(expr->u.as.type));
            valexpr_fprintf(expr->u.as.sub_expr, file);
            fputc(')', file);
            break;
        case VALEXPR_TYPE_BINOP:
            fprintf(file, "%s ", valexpr_op_msg(expr->u.op.op));
            valexpr_fprintf(expr->u.op.sub_expr1, file);
            fputc(' ', file);
            valexpr_fprintf(expr->u.op.sub_expr2, file);
            break;
        case VALEXPR_TYPE_UNOP:
            fprintf(file, "%s ", valexpr_op_msg(expr->u.op.op));
            valexpr_fprintf(expr->u.op.sub_expr1, file);
            break;
        default:
            fprintf(file, "<unknown>");
            break;
    }
}

void valexpr_set_literal_null(valexpr_t *expr){
    expr->type = VALEXPR_TYPE_LITERAL;
    val_init(&expr->u.val);
    val_set_null(&expr->u.val);
}

void valexpr_set_literal_bool(valexpr_t *expr, bool b){
    expr->type = VALEXPR_TYPE_LITERAL;
    val_init(&expr->u.val);
    val_set_bool(&expr->u.val, b);
}

void valexpr_set_literal_int(valexpr_t *expr, int i){
    expr->type = VALEXPR_TYPE_LITERAL;
    val_init(&expr->u.val);
    val_set_int(&expr->u.val, i);
}

void valexpr_set_literal_str(valexpr_t *expr, const char *s){
    expr->type = VALEXPR_TYPE_LITERAL;
    val_init(&expr->u.val);
    val_set_const_str(&expr->u.val, s);
}

int valexpr_parse(valexpr_t *expr, fus_lexer_t *lexer){
    INIT

    int type = -1;
    int op = -1;
    if(GOT("==")){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_EQ; }
    else if(GOT("!=")){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_NE; }
    else if(GOT("<" )){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_LT; }
    else if(GOT("<=")){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_LE; }
    else if(GOT(">" )){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_GT; }
    else if(GOT(">=")){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_GE; }
    else if(GOT("&&")){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_AND; }
    else if(GOT("||")){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_OR; }
    else if(GOT("!")){ type = VALEXPR_TYPE_UNOP; op = VALEXPR_OP_NOT; }
    else if(GOT("len")){ type = VALEXPR_TYPE_UNOP; op = VALEXPR_OP_LEN; }
    else if(GOT("get")){ type = VALEXPR_TYPE_BINOP; op = VALEXPR_OP_GET; }
    else type =
        GOT("yourvar")? VALEXPR_TYPE_YOURVAR:
        GOT("mapvar")? VALEXPR_TYPE_MAPVAR:
        GOT("globalvar")? VALEXPR_TYPE_GLOBALVAR:
        GOT("myvar")? VALEXPR_TYPE_MYVAR:
        GOT("if")? VALEXPR_TYPE_IF:
        GOT("as")? VALEXPR_TYPE_AS:
        VALEXPR_TYPE_LITERAL;

    if(type == VALEXPR_TYPE_BINOP || type == VALEXPR_TYPE_UNOP){
        NEXT
        expr->type = type;
        expr->u.op.op = op;

        valexpr_t *sub_expr1 = calloc(1, sizeof(*sub_expr1));
        if(!sub_expr1)return 1;
        err = valexpr_parse(sub_expr1, lexer);
        if(err)return err;
        expr->u.op.sub_expr1 = sub_expr1;

        if(type == VALEXPR_TYPE_BINOP){
            valexpr_t *sub_expr2 = calloc(1, sizeof(*sub_expr2));
            if(!sub_expr2)return 1;
            err = valexpr_parse(sub_expr2, lexer);
            if(err)return err;
            expr->u.op.sub_expr2 = sub_expr2;
        }
    }else if(
        type == VALEXPR_TYPE_YOURVAR ||
        type == VALEXPR_TYPE_MAPVAR ||
        type == VALEXPR_TYPE_GLOBALVAR ||
        type == VALEXPR_TYPE_MYVAR
    ){
        NEXT
        expr->type = type;
        expr->u.key_expr = NULL;
        OPEN

        valexpr_t *key_expr = calloc(1, sizeof(*key_expr));
        if(!key_expr)return 1;

        err = valexpr_parse(key_expr, lexer);
        if(err)return err;

        expr->u.key_expr = key_expr;
        CLOSE
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
    }else if(type == VALEXPR_TYPE_AS){
        NEXT
        expr->type = type;

        if(GOT("you")){
            NEXT
            expr->u.as.type = VALEXPR_AS_YOU;
        }else{
            return UNEXPECTED("you");
        }

        expr->u.as.sub_expr = NULL;

        OPEN
        valexpr_t *sub_expr = calloc(1, sizeof(*sub_expr));
        if(!sub_expr)return 1;
        err = valexpr_parse(sub_expr, lexer);
        if(err)return err;
        expr->u.as.sub_expr = sub_expr;
        CLOSE
    }else{
        expr->type = type;
        err = val_parse(&expr->u.val, lexer);
        if(err == 2){
            fprintf(stderr, "...while parsing a valexpr, and seeing what we assumed to be a literal value\n");
        }
        if(err)return err;
    }
    return 0;
}


int valexpr_eval(valexpr_t *expr, valexpr_context_t *context,
    valexpr_result_t *result, bool set
){
    int err;

    switch(expr->type){
        case VALEXPR_TYPE_LITERAL: {
            result->val = &expr->u.val;
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

            valexpr_result_t key_result = {0};
            err = valexpr_get(expr->u.key_expr, context, &key_result);
            if(err)return err;
            val_t *key_val = key_result.val;
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
                    /* ...return result->val = NULL */
                    break;
                }
            }

            result->val = &var->value;
            result->var = var;
            result->vars = vars;
            break;
        }
        case VALEXPR_TYPE_IF: {
            bool cond;
            err = valexpr_cond_eval(expr->u.if_expr.cond, context, &cond);
            if(err)return err;

            if(cond){
                err = valexpr_eval(expr->u.if_expr.then_expr,
                    context, result, set);
                if(err)return err;
            }else{
                err = valexpr_eval(expr->u.if_expr.else_expr,
                    context, result, set);
                if(err)return err;
            }
            break;
        }
        case VALEXPR_TYPE_AS: {
            switch(expr->u.as.type){
                case VALEXPR_AS_YOU: {
                    /* We evaluate our sub-expr "as you", that is, with
                    my_vars and your_vars swapped.
                    The terminology here is just too silly, eh? */
                    valexpr_context_t sub_context = *context;
                    sub_context.yourvars = context->myvars;
                    sub_context.myvars = context->yourvars;

                    err = valexpr_eval(expr->u.as.sub_expr,
                        &sub_context, result, set);
                    if(err)return err;
                    break;
                }
                default:
                    fprintf(stderr, "Unknown \"as\" type: %i\n",
                        expr->u.as.type);
                    return 2;
            }
            break;
        }
        case VALEXPR_TYPE_BINOP:
        case VALEXPR_TYPE_UNOP: {
            valexpr_result_t result1 = {0};
            err = valexpr_get(expr->u.op.sub_expr1, context, &result1);
            if(err)return err;
            val_t *val1 = result1.val;
            if(val1 == NULL){
                fprintf(stderr, "Couldn't get value for LHS: ");
                valexpr_fprintf(expr->u.op.sub_expr1, stderr);
                fputc('\n', stderr);
                return 2;
            }

            val_t *val2 = NULL;
            if(expr->type == VALEXPR_TYPE_BINOP){
                valexpr_result_t result2 = {0};
                err = valexpr_get(expr->u.op.sub_expr2, context, &result2);
                if(err)return err;
                val2 = result2.val;
                if(val2 == NULL){
                    fprintf(stderr, "Couldn't get value for RHS: ");
                    valexpr_fprintf(expr->u.op.sub_expr2, stderr);
                    fputc('\n', stderr);
                    return 2;
                }
            }

            switch(expr->u.op.op){
                case VALEXPR_OP_EQ: result->val = val_bool(val_eq(val1, val2)); break;
                case VALEXPR_OP_NE: result->val = val_bool(val_ne(val1, val2)); break;
                case VALEXPR_OP_LT: result->val = val_bool(val_lt(val1, val2)); break;
                case VALEXPR_OP_LE: result->val = val_bool(val_le(val1, val2)); break;
                case VALEXPR_OP_GT: result->val = val_bool(val_gt(val1, val2)); break;
                case VALEXPR_OP_GE: result->val = val_bool(val_ge(val1, val2)); break;
                case VALEXPR_OP_AND: result->val = val_bool(val_and(val1, val2)); break;
                case VALEXPR_OP_OR: result->val = val_bool(val_or(val1, val2)); break;
                case VALEXPR_OP_NOT: result->val = val_bool(val_not(val1)); break;
                case VALEXPR_OP_LEN: result->val = val_get_arr_len_val(val1); break;
                case VALEXPR_OP_GET: result->val = val2->type == VAL_TYPE_INT?
                    val_get_arr_item(val1, val2->u.i): NULL; break;
                default:
                    fprintf(stderr, "Unrecognized op: %i\n", expr->u.op.op);
                    return 2;
            }

            break;
        }
        default: {
            fprintf(stderr,
                "valexpr_eval: Unknown expr type: %i\n", expr->type);
            return 2;
        }
    }

    return 0;
}


int valexpr_get(valexpr_t *expr, valexpr_context_t *context,
    valexpr_result_t *result
){
    /* NOTE: Caller needs to check whether result->val is NULL. */
    return valexpr_eval(expr, context, result, false);
}

int valexpr_set(valexpr_t *expr, valexpr_context_t *context,
    valexpr_result_t *result
){
    /* NOTE: valexpr_set guarantees that we find (or create, if necessary)
    a val, so caller doesn't need to check whether result->val is NULL. */
    return valexpr_eval(expr, context, result, true);
}



#define VALEXPR_GET_TYPE(TYPE, VAL_TYPE) \
TYPE valexpr_get_##VAL_TYPE(valexpr_t *expr, \
    valexpr_context_t *context \
){ \
    valexpr_result_t result = {0}; \
    int err = valexpr_get(expr, context, &result); \
    if(err){ \
        fprintf(stderr, "Error while getting %s from valexpr: ", \
            #TYPE); \
        valexpr_fprintf(expr, stderr); \
        fputc('\n', stderr); \
        return 0; \
    }else if(!result.val){ \
        return 0; \
    } \
    return val_get_##VAL_TYPE(result.val); \
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
            ARRAY_FREE_PTR(valexpr_cond_t *, cond->u.subconds.subconds,
                valexpr_cond_cleanup)
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

        OPEN
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
