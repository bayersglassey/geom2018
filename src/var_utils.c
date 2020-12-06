
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
    }else{
        return UNEXPECTED("an int or str, or one of: null T F");
    }
    return 0;
}




void vars_write(vars_t *vars, FILE *file, int indent){
    for(int i = 0; i < vars->vars_len; i++){
        var_t *var = vars->vars[i];
        _print_tabs(file, indent);
        fus_write_str(file, var->key);
        fputs(": ", file);
        switch(var->value.type){
            case VAL_TYPE_NULL: fputs("null", file); break;
            case VAL_TYPE_BOOL: putc(var->value.u.b? 'T': 'F', file); break;
            case VAL_TYPE_INT: fprintf(file, "%i", var->value.u.i); break;
            case VAL_TYPE_STR: fus_write_str(file, var->value.u.s); break;
            case VAL_TYPE_CONST_STR: fus_write_str(file, var->value.u.cs); break;
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
        GET_STR(name)
        GET("(")
        var_t *var = vars_get_or_add(vars, name);
        if(var == NULL)return 2;
        err = val_parse(&var->value, lexer);
        if(err)return err;
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
