
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "util.h"
#include "vars.h"


/*******
 * VAR *
 *******/

void var_cleanup(var_t *var){
    free(var->key);
    var_unset(var);
}

int var_init(var_t *var, char *key){
    var->key = key;
    var->type = 'n';
    return 0;
}


void var_unset(var_t *var){
    if(var->type == 's')free(var->value.s);
}

void var_set_null(var_t *var){
    var_unset(var);
    var->type = 'n';
}
void var_set_bool(var_t *var, bool b){
    var_unset(var);
    var->type = 'b';
    var->value.b = b;
}
void var_set_int(var_t *var, int i){
    var_unset(var);
    var->type = 'i';
    var->value.i = i;
}
void var_set_str(var_t *var, char *s){
    var_unset(var);
    var->type = 's';
    var->value.s = s;
}


/********
 * VARS *
 ********/

void vars_cleanup(vars_t *vars){
    ARRAY_FREE_PTR(var_t*, vars->vars, var_cleanup)
}

int vars_init(vars_t *vars){
    ARRAY_INIT(vars->vars)
    return 0;
}

int vars_add(vars_t *vars, char *key, var_t **var_ptr){
    ARRAY_PUSH_NEW(var_t*, vars->vars, var)
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
    if(var == NULL)return false;
    return var->value.b;
}
int vars_get_int(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);
    if(var == NULL)return 0;
    return var->value.i;
}
const char *vars_get_str(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);

    /* Do we want to be accurate (there is no string) or safe
    (empty string)?.. */
    //if(var == NULL)return NULL;
    if(var == NULL)return "";

    return var->value.s;
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

