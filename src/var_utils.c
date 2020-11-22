
#include <stdio.h>
#include <stdbool.h>

#include "var_utils.h"
#include "lexer_macros.h"
#include "write.h"

void vars_write(vars_t *vars, FILE *file, const char *tabs){
    for(int i = 0; i < vars->vars_len; i++){
        var_t *var = vars->vars[i];
        fprintf(file, "%s%s: ", tabs, var->key);
        switch(var->type){
            case VAR_TYPE_NULL: fputs("null", file); break;
            case VAR_TYPE_BOOL: putc(var->value.b? 'T': 'F', file); break;
            case VAR_TYPE_INT: fprintf(file, "%i", var->value.i); break;
            case VAR_TYPE_STR: fus_write_str(file, var->value.s); break;
            case VAR_TYPE_CONST_STR: fus_write_str(file, var->value.cs); break;
            default: fputs("???", file); break;
        }
        fputc('\n', file);
    }
}

void vars_write_simple(vars_t *vars, FILE *file){
    fputs(":\n", file);
    vars_write(vars, file, "  ");
}

int vars_parse(vars_t *vars, fus_lexer_t *lexer){
    INIT
    while(true){
        if(DONE || GOT(")"))break;

        char *name;
        GET_NAME(name)
        GET("(")
        if(GOT_INT){
            int i;
            GET_INT(i)
            DO(vars_set_int(vars, name, i))
        }else if(GOT_STR){
            char *s;
            GET_STR(s)
            DO(vars_set_str(vars, name, s))
        }else if(GOT("null")){
            DO(vars_set_null(vars, name))
        }else if(GOT("t")){
            DO(vars_set_bool(vars, name, true))
        }else if(GOT("f")){
            DO(vars_set_bool(vars, name, false))
        }else{
            return UNEXPECTED("an int or str, or one of: null, t, f");
        }
        GET(")")

        free(name);
    }
    return 0;
}
