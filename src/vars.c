
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "str_utils.h"
#include "vars.h"
#include "write.h"


/* CONSTANT VALS (marked "extern" in vars.h) */
val_t val_null = { .type = VAL_TYPE_NULL };
val_t val_true = { .type = VAL_TYPE_BOOL, .u.b = true };
val_t val_false = { .type = VAL_TYPE_BOOL, .u.b = false };



/*******
 * VAR *
 *******/

void var_cleanup(var_t *var){
    free(var->key);
    val_cleanup(&var->value);
}

void var_init(var_t *var, char *key){
    var->key = key;
    var->props = 0; /* Empty bit array */
    val_init(&var->value);
}

void var_set_prop(var_t *var, unsigned i){
    var->props |= 1 << i;
}


/*******
 * VAL *
 *******/

void val_cleanup(val_t *val){
    free(val->s);
    val->s = NULL;
}

void val_init(val_t *val){
    val->type = VAL_TYPE_NULL;
    val->s = NULL;
}

void val_fprintf(val_t *val, FILE *file){
    switch(val->type){
        case VAL_TYPE_NULL: fputs("null", file); break;
        case VAL_TYPE_BOOL: putc(val->u.b? 'T': 'F', file); break;
        case VAL_TYPE_INT: fprintf(file, "%i", val->u.i); break;
        case VAL_TYPE_STR: fus_write_str(file, val->u.s); break;
        default: fputs("???", file); break;
    }
}

const char *val_type_name(int type){
    static const char *names[VAL_TYPES] = {
        "null", "bool", "int", "str"
    };
    if(type < 0 || type >= VAL_TYPES)return "unknown";
    return names[type];
}


bool val_get_bool(val_t *val){
    if(val->type == VAL_TYPE_BOOL)return val->u.b;
    if(val->type == VAL_TYPE_INT)return val->u.i;
    if(val->type == VAL_TYPE_STR)return val->u.s;
    return false;
}
int val_get_int(val_t *val){
    if(val->type != VAL_TYPE_INT)return 0;
    return val->u.i;
}
const char *val_get_str(val_t *val){
    if(val->type == VAL_TYPE_STR)return val->u.s;
    return NULL;
}

void val_set_null(val_t *val){
    val_cleanup(val);
    val->type = VAL_TYPE_NULL;
}
void val_set_bool(val_t *val, bool b){
    val_cleanup(val);
    val->type = VAL_TYPE_BOOL;
    val->u.b = b;
}
void val_set_int(val_t *val, int i){
    val_cleanup(val);
    val->type = VAL_TYPE_INT;
    val->u.i = i;
}
void val_set_str(val_t *val, char *s){
    val_cleanup(val);
    val->type = VAL_TYPE_STR;
    val->u.s = s;
    val->s = s;
}
void val_set_const_str(val_t *val, const char *s){
    val_cleanup(val);
    val->type = VAL_TYPE_STR;
    val->u.s = s;
}

int val_copy(val_t *val1, val_t *val2){
    int err;
    switch(val2->type){
        case VAL_TYPE_NULL:
            val_set_null(val1);
            break;
        case VAL_TYPE_BOOL:
            val_set_bool(val1, val2->u.b);
            break;
        case VAL_TYPE_INT:
            val_set_int(val1, val2->u.i);
            break;
        case VAL_TYPE_STR: {
            if(val2->s){
                char *s2 = strdup(val2->s);
                if(!s2)return 1;
                val_set_str(val1, s2);
            }else{
                val_set_const_str(val1, val2->u.s);
            }
            break;
        }
        default:
            fprintf(stderr, "Unrecognized var type: %i\n", val2->type);
            return 2;
    }
    return 0;
}

bool val_eq(val_t *val1, val_t *val2){
    if(val1->type != val2->type)return false;
    switch(val1->type){
        case VAL_TYPE_NULL: return true;
        case VAL_TYPE_BOOL: return val1->u.b == val2->u.b;
        case VAL_TYPE_INT: return val1->u.i == val2->u.i;
        case VAL_TYPE_STR: return !strcmp(val1->u.s, val2->u.s);
        default: return false;
    }
}

bool val_ne(val_t *val1, val_t *val2){
    if(val1->type != val2->type)return true;
    switch(val1->type){
        case VAL_TYPE_NULL: return false;
        case VAL_TYPE_BOOL: return val1->u.b != val2->u.b;
        case VAL_TYPE_INT: return val1->u.i != val2->u.i;
        case VAL_TYPE_STR: return strcmp(val1->u.s, val2->u.s);
        default: return true;
    }
}

bool val_lt(val_t *val1, val_t *val2){
    if(val1->type != val2->type)return false;
    switch(val1->type){
        case VAL_TYPE_NULL: return false;
        case VAL_TYPE_BOOL: return val1->u.b < val2->u.b;
        case VAL_TYPE_INT: return val1->u.i < val2->u.i;
        case VAL_TYPE_STR: return strcmp(val1->u.s, val2->u.s) < 0;
        default: return false;
    }
}

bool val_le(val_t *val1, val_t *val2){
    if(val1->type != val2->type)return false;
    switch(val1->type){
        case VAL_TYPE_NULL: return true;
        case VAL_TYPE_BOOL: return val1->u.b <= val2->u.b;
        case VAL_TYPE_INT: return val1->u.i <= val2->u.i;
        case VAL_TYPE_STR: return strcmp(val1->u.s, val2->u.s) <= 0;
        default: return false;
    }
}

bool val_gt(val_t *val1, val_t *val2){
    if(val1->type != val2->type)return false;
    switch(val1->type){
        case VAL_TYPE_NULL: return false;
        case VAL_TYPE_BOOL: return val1->u.b > val2->u.b;
        case VAL_TYPE_INT: return val1->u.i > val2->u.i;
        case VAL_TYPE_STR: return strcmp(val1->u.s, val2->u.s) > 0;
        default: return false;
    }
}

bool val_ge(val_t *val1, val_t *val2){
    if(val1->type != val2->type)return false;
    switch(val1->type){
        case VAL_TYPE_NULL: return true;
        case VAL_TYPE_BOOL: return val1->u.b >= val2->u.b;
        case VAL_TYPE_INT: return val1->u.i >= val2->u.i;
        case VAL_TYPE_STR: return strcmp(val1->u.s, val2->u.s) >= 0;
        default: return false;
    }
}



/********
 * VARS *
 ********/

void vars_cleanup(vars_t *vars){
    ARRAY_FREE_PTR(var_t*, vars->vars, var_cleanup)
}

void vars_init(vars_t *vars){
    vars_init_with_props(vars, NULL);
}

void vars_init_with_props(vars_t *vars, const char **prop_names){
    ARRAY_INIT(vars->vars)

    int prop_names_len = 0;
    if(prop_names)while(prop_names[prop_names_len] != NULL)prop_names_len++;
    vars->prop_names = prop_names;
    vars->prop_names_len = prop_names_len;
}

static void _vars_dumpvar(vars_t *vars, var_t *var){

    /* Iterate over the bits in the var->props bit array */
    var_props_t props = var->props;
    int prop_i = 0;
    while(props){
        bool prop = props & 1;
        if(prop){
            /* Look up corresponding prop name & display it before the
            var's key */
            const char *prop_name = vars->prop_names[prop_i];
            fprintf(stderr, "%s ", prop_name);
        }
        props >>= 1;
        prop_i++;
    }

    /* Print var's key */
    fprintf(stderr, "%s (%s): ", var->key,
        val_type_name(var->value.type));

    /* Print var's value */
    val_fprintf(&var->value, stderr);

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
    var_init(var, key);
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
            perror("strdup");
            return NULL;
        }
        int err = vars_add(vars, new_key, &var);
        if(err)return NULL;
    }
    return var;
}

int vars_get_prop_i(vars_t *vars, const char *prop_name){
    if(vars->prop_names)for(int i = 0; i < vars->prop_names_len; i++){
        if(!strcmp(vars->prop_names[i], prop_name))return i;
    }
    return -1;
}


bool vars_get_bool(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);
    if(var == NULL)return false;
    return val_get_bool(&var->value);
}
int vars_get_int(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);
    if(var == NULL)return 0;
    return val_get_int(&var->value);
}
const char *vars_get_str(vars_t *vars, const char *key){
    var_t *var = vars_get(vars, key);
    if(var == NULL)return NULL;
    return val_get_str(&var->value);
}

int vars_set_null(vars_t *vars, const char *key){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    val_set_null(&var->value);
    return 0;
}
int vars_set_bool(vars_t *vars, const char *key, bool b){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    val_set_bool(&var->value, b);
    return 0;
}
int vars_set_int(vars_t *vars, const char *key, int i){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    val_set_int(&var->value, i);
    return 0;
}
int vars_set_str(vars_t *vars, const char *key, char *s){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    val_set_str(&var->value, s);
    return 0;
}
int vars_set_const_str(vars_t *vars, const char *key, const char *s){
    var_t *var = vars_get_or_add(vars, key);
    if(var == NULL)return 1;
    val_set_const_str(&var->value, s);
    return 0;
}

int vars_copy(vars_t *vars1, vars_t *vars2){
    /* Copies variables from vars2 to vars1 */
    int err;
    for(int i = 0; i < vars2->vars_len; i++){
        var_t *var2 = vars2->vars[i];

        var_t *var1 = vars_get_or_add(vars1, var2->key);
        if(!var1)return 1;

        err = val_copy(&var1->value, &var2->value);
        if(err)return err;

        var1->props = var2->props;
    }
    return 0;
}

