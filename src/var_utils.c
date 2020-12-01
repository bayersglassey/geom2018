
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

void vars_write(vars_t *vars, FILE *file, int indent){
    for(int i = 0; i < vars->vars_len; i++){
        var_t *var = vars->vars[i];
        _print_tabs(file, indent);
        fprintf(file, "%s: ", var->key);
        switch(var->value.type){
            case VAR_TYPE_NULL: fputs("null", file); break;
            case VAR_TYPE_BOOL: putc(var->value.u.b? 'T': 'F', file); break;
            case VAR_TYPE_INT: fprintf(file, "%i", var->value.u.i); break;
            case VAR_TYPE_STR: fus_write_str(file, var->value.u.s); break;
            case VAR_TYPE_CONST_STR: fus_write_str(file, var->value.u.cs); break;
            default: fputs("???", file); break;
        }
        fputc('\n', file);
    }
}

void vars_write_simple(vars_t *vars, FILE *file){
    fputs(":\n", file);
    vars_write(vars, file, 4);
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
            NEXT
            DO(vars_set_null(vars, name))
        }else if(GOT("T")){
            NEXT
            DO(vars_set_bool(vars, name, true))
        }else if(GOT("F")){
            NEXT
            DO(vars_set_bool(vars, name, false))
        }else{
            return UNEXPECTED("an int or str, or one of: null T F");
        }
        GET(")")

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
