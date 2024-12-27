
#include <stdio.h>
#include <stdbool.h>

#include "var_utils.h"
#include "file_utils.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "write.h"

static void _print_tabs(FILE *file, int indent){
    for(int i = 0; i < indent; i++)putc(' ', file);
}

int val_parse(val_t *val, fus_lexer_t *lexer){
    INIT
    if(GOT_INT){
        int i;
        GET_INT(i)
        val_set_int(val, i);
    }else if(GOT_STR){
        char *s;
        GET_STR(s)
        val_set_str(val, s);
    }else if(GOT("null")){
        NEXT
        val_set_null(val);
    }else if(GOT("T")){
        NEXT
        val_set_bool(val, true);
    }else if(GOT("F")){
        NEXT
        val_set_bool(val, false);
    }else if(GOT("(")){
        NEXT

        /* Parse unary operators */
        char unop = '\0';
        if(GOT("-")){
            NEXT
            unop = lexer->token[0];
        }

        /* Parse initial value */
        err = val_parse(val, lexer);
        if(err)return err;

        /* Handle unary operator */
        if(unop == '-'){
            /* Can only use unary '-' operator with int */
            if(val->type != VAL_TYPE_INT){
                fus_lexer_err_info(lexer);
                fprintf(stderr,
                    "unary operators not supported by %s\n",
                    val_type_name(val->type));
                return 2;
            }

            /* Flip sign */
            val->u.i = -val->u.i;
        }

        /* Can only use binary operators with int (for now) */
        if(!GOT(")") && val->type != VAL_TYPE_INT){
            fus_lexer_err_info(lexer);
            fprintf(stderr,
                "binary operators not supported by %s\n",
                val_type_name(val->type));
            return 2;
        }

        /* Parse binary operators */
        while(!GOT(")")){
            if(GOT("+") || GOT("-") || GOT("*") || GOT("/") || GOT("%")){
                char binop = lexer->token[0];
                NEXT
                val_t _val2, *val2=&_val2;
                val_init(val2);
                err = val_parse(val2, lexer);
                if(err)return err;
                if(val2->type != VAL_TYPE_INT){
                    fus_lexer_err_info(lexer);
                    fprintf(stderr,
                        "Binary operator '%c' expected int, got: %s\n",
                        binop, val_type_name(val2->type));
                    return 2;
                }
                switch(binop){
                    case '+': val->u.i += val2->u.i; break;
                    case '-': val->u.i -= val2->u.i; break;
                    case '*': val->u.i *= val2->u.i; break;
                    case '/': val->u.i /= val2->u.i; break;
                    case '%': val->u.i %= val2->u.i; break;
                    default: break;
                }
            }else{
                return UNEXPECTED("one of: + - * / %");
            }
        }
        NEXT
    }else{
        return UNEXPECTED("an int or str, or one of: null T F");
    }
    return 0;
}

static void _print_props(FILE *file, vars_t *vars, var_t *var){
    var_props_t props = var->props;
    int prop_i = 0;
    while(props){
        bool prop = props & 1;
        if(prop){
            const char *prop_name = vars->prop_names[prop_i];
            fprintf(file, "%s ", prop_name);
        }
        props >>= 1;
        prop_i++;
    }
}
void vars_write_with_mask(vars_t *vars, FILE *file, int indent,
    var_props_t nowrite_props_mask
){
    for(int i = 0; i < vars->vars_len; i++){
        var_t *var = vars->vars[i];

        /* Skip vars whose props say they shouldn't be written */
        if(var->props & nowrite_props_mask)continue;

        _print_tabs(file, indent);
        _print_props(file, vars, var);
        fus_write_str(file, var->key);
        fputs(": ", file);
        switch(var->value.type){
            case VAL_TYPE_NULL: fputs("null", file); break;
            case VAL_TYPE_BOOL: putc(var->value.u.b? 'T': 'F', file); break;
            case VAL_TYPE_INT: fprintf(file, "%i", var->value.u.i); break;
            case VAL_TYPE_STR: fus_write_str(file, var->value.u.s); break;
            default: fputs("???", file); break;
        }
        fputc('\n', file);
    }
}

void vars_write(vars_t *vars, FILE *file, int indent){
    var_props_t nowrite_props_mask = 0; /* Write all vars by default */
    vars_write_with_mask(vars, file, indent, nowrite_props_mask);
}

void vars_write_simple(vars_t *vars, FILE *file){
    fputs(":\n", file);
    vars_write(vars, file, 4);
}

int vars_parse(vars_t *vars, fus_lexer_t *lexer){
    INIT
    while(true){
        if(DONE || GOT(")"))break;

        var_props_t props = 0;
        while(GOT_NAME){
            char *prop_name;
            GET_NAME(prop_name)
            int prop_i = vars_get_prop_i(vars, prop_name);
            if(prop_i < 0){
                fprintf(stderr, "Couldn't find prop: %s\n", prop_name);
                fprintf(stderr, "Among props:");
                for(int i = 0; i < vars->prop_names_len; i++){
                    fprintf(stderr, " %s", vars->prop_names[i]);
                }
                fputc('\n', stderr);
                return 2;
            }

            props |= 1 << prop_i;

            free(prop_name);
        }

        char *name;
        GET_STR(name)
        OPEN
        var_t *var = vars_get_or_add(vars, name);
        if(var == NULL)return 2;
        var->props = props;
        err = val_parse(&var->value, lexer);
        if(err)return err;
        err = vars_callback(vars, var);
        if(err)return err;
        CLOSE

        free(name);
    }
    return 0;
}

int vars_load(vars_t *vars, const char *filename){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = vars_parse(vars, &lexer);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}
