
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "str_utils.h"
#include "vars.h"


/*******
 * VAR *
 *******/

void var_cleanup(var_t *var){
    free(var->key);
    var_unset(var);
}

void var_init(var_t *var, char *key){
    var->key = key;
    var->type = 'n';
}

void var_fprintf(var_t *var, FILE *file){
    switch(var->type){
        case VAR_TYPE_NULL: fputs("null", file); break;
        case VAR_TYPE_BOOL: putc(var->value.b? 'T': 'F', file); break;
        case VAR_TYPE_INT: fprintf(file, "%i", var->value.i); break;
        case VAR_TYPE_STR: fputs(var->value.s, file); break;
        case VAR_TYPE_CONST_STR: fputs(var->value.cs, file); break;
        default: fputs("???", file); break;
    }
}

const char *var_type_name(int type){
    static const char *names[VAR_TYPES] = {
        "null", "bool", "int", "str", "str"
    };
    if(type < 0 || type >= VAR_TYPES)return "unknown";
    return names[type];
}


void var_unset(var_t *var){
    if(var->type == VAR_TYPE_STR)free(var->value.s);
}

void var_set_null(var_t *var){
    var_unset(var);
    var->type = VAR_TYPE_NULL;
}
void var_set_bool(var_t *var, bool b){
    var_unset(var);
    var->type = VAR_TYPE_BOOL;
    var->value.b = b;
}
void var_set_int(var_t *var, int i){
    var_unset(var);
    var->type = VAR_TYPE_INT;
    var->value.i = i;
}
void var_set_str(var_t *var, char *s){
    var_unset(var);
    var->type = VAR_TYPE_STR;
    var->value.s = s;
}
void var_set_const_str(var_t *var, const char *cs){
    var_unset(var);
    var->type = VAR_TYPE_CONST_STR;
    var->value.cs = cs;
}


/********
 * VARS *
 ********/

void vars_cleanup(vars_t *vars){
    ARRAY_FREE_PTR(var_t*, vars->vars, var_cleanup)
}

void vars_init(vars_t *vars){
    ARRAY_INIT(vars->vars)
}

static void _vars_dumpvar(vars_t *vars, var_t *var){
    fprintf(stderr, "%s (%s): ", var->key,
        var_type_name(var->type));
    var_fprintf(var, stderr);
    putc('\n', stderr);
}

void vars_dump(vars_t *vars){
    fprintf(stderr, "VARS (%p):\n", vars);
    for(int i = 0; i < vars->vars_len; i++){
        var_t *var = vars->vars[i];
        fprintf(stderr, "  ");
        _vars_dumpvar(vars, var);
    }
}

void vars_dumpvar(vars_t *vars, const char *key){
    fprintf(stderr, "VAR ");
    var_t *var = vars_get(vars, key);
    if(!var){
        fprintf(stderr, "%s (null)\n", key);
        return;
    }
    _vars_dumpvar(vars, var);
}

int vars_add(vars_t *vars, char *key, var_t **var_ptr){
    ARRAY_PUSH_NEW(var_t*, vars->vars, var)
    var->key = key;
    *var_ptr = var;
    return 0;
}

var_t *vars_get(vars_t *vars, const char *key){
    for(int i = 0; i < vars->vars_len; i++){
        var_t *var = vars->vars[i];
        if(!strcmp(var->key, key))return var;
    }
    return NULL;
}

var_t *vars_get_or_add(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);
    if(!var){
        char *new_key = strdup(key);
        if(new_key == NULL){
            perror("vars_get_or_add");
            return NULL;
        }
        int err = vars_add(vars, new_key, &var);
        if(err)return NULL;
    }
    return var;
}


bool vars_get_bool(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);
    if(var == NULL || var->type != VAR_TYPE_BOOL)return false;
    return var->value.b;
}
int vars_get_int(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);
    if(var == NULL || var->type != VAR_TYPE_INT)return 0;
    return var->value.i;
}
const char *vars_get_str(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);
    if(var == NULL)return NULL;
    if(var->type == VAR_TYPE_STR)return var->value.s;
    if(var->type == VAR_TYPE_CONST_STR)return var->value.cs;
    return NULL;
}

int vars_set_null(vars_t *vars, const char *key){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    var_set_null(var);
    return 0;
}
int vars_set_bool(vars_t *vars, const char *key, bool b){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    var_set_bool(var, b);
    return 0;
}
int vars_set_int(vars_t *vars, const char *key, int i){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    var_set_int(var, i);
    return 0;
}
int vars_set_str(vars_t *vars, const char *key, char *s){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    var_set_str(var, s);
    return 0;
}
int vars_set_const_str(vars_t *vars, const char *key, const char *cs){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    var_set_const_str(var, cs);
    return 0;
}

