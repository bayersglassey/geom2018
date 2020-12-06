#ifndef _VARS_H_
#define _VARS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "array.h"


enum var_type {
    VAL_TYPE_NULL,
    VAL_TYPE_BOOL,
    VAL_TYPE_INT,
    VAL_TYPE_STR,
    VAL_TYPE_CONST_STR,
    VAL_TYPES
};

typedef struct val {
    int type; /* enum var_type */
    union {
        bool b;
        int i;
        char *s;
        const char *cs;
    } u;
} val_t;

typedef struct var {
    char *key;
    val_t value;
} var_t;

typedef struct vars {
    /* NOTE: vars_t must guarantee that:

        * It can be freely copied (that is, using C assignment operator)
        * A vars_t which has been initialized, but has had no variables set,
          has allocated no memory and may be freely overwritten

    Example:

        vars_t vars_original;
        vars_t vars_copy;

        vars_init(&vars_original);
        vars_set_int(&vars_original, "x", 3);

        // The "copy" is initialized, but has no variables set...
        vars_init(&vars_copy);

        // ...so we can overwrite it.
        vars_copy = vars_original;

        // We can now use vars_copy...

        // ...and clean it up when done.
        // DO NOT cleanup the original!.. because it was copied.
        vars_cleanup(&vars_copy);
    */

    ARRAY_DECL(var_t*, vars)
} vars_t;


void var_cleanup(var_t *var);
void var_init(var_t *var, char *key);

void val_cleanup(val_t *val);
void val_init(val_t *val);
void val_fprintf(val_t *val, FILE *file);
const char *val_type_name(int type);

void val_unset(val_t *val);
bool val_get_bool(val_t *val);
int val_get_int(val_t *val);
const char *val_get_str(val_t *val);
void val_set_null(val_t *val);
void val_set_bool(val_t *val, bool b);
void val_set_int(val_t *val, int i);
void val_set_str(val_t *val, char *s);
void val_set_const_str(val_t *val, const char *cs);

int val_copy(val_t *val1, val_t *val2);


void vars_cleanup(vars_t *vars);
void vars_init(vars_t *vars);
void vars_dump(vars_t *vars);
void vars_dumpvar(vars_t *vars, const char *key);
int vars_add(vars_t *vars, char *key, var_t **var_ptr);
var_t *vars_get(vars_t *vars, const char *key);
var_t *vars_get_or_add(vars_t *vars, const char *key);

bool vars_get_bool(vars_t *vars, const char *key);
int vars_get_int(vars_t *vars, const char *key);
const char *vars_get_str(vars_t *vars, const char *key);

int vars_set_null(vars_t *vars, const char *key);
int vars_set_bool(vars_t *vars, const char *key, bool b);
int vars_set_int(vars_t *vars, const char *key, int i);
int vars_set_str(vars_t *vars, const char *key, char *s);
int vars_set_const_str(vars_t *vars, const char *key, const char *s);

int vars_copy(vars_t *vars1, vars_t *vars2);


#endif