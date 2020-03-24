#ifndef _VARS_H_
#define _VARS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "array.h"


enum var_type {
    VAR_TYPE_NULL,
    VAR_TYPE_BOOL,
    VAR_TYPE_INT,
    VAR_TYPE_STR,
    VAR_TYPE_CONST_STR,
    VAR_TYPE_PTR,
    VAR_TYPES
};

typedef struct var {
    char *key;
    int type; /* enum var_type */
    union {
        bool b;
        int i;
        char *s;
        const char *cs;
        void *p;
    } value;
} var_t;

typedef struct vars {
    ARRAY_DECL(var_t*, vars)
} vars_t;


void var_cleanup(var_t *var);
void var_init(var_t *var, char *key);
void var_fprintf(var_t *var, FILE *file);

void var_unset(var_t *var);
void var_set_null(var_t *var);
void var_set_bool(var_t *var, bool b);
void var_set_int(var_t *var, int i);
void var_set_str(var_t *var, char *s);
void var_set_const_str(var_t *var, const char *cs);
void var_set_ptr(var_t *var, void *p);

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
void *vars_get_ptr(vars_t *vars, const char *key);

int vars_set_null(vars_t *vars, const char *key);
int vars_set_bool(vars_t *vars, const char *key, bool b);
int vars_set_int(vars_t *vars, const char *key, int i);
int vars_set_str(vars_t *vars, const char *key, char *s);
int vars_set_const_str(vars_t *vars, const char *key, const char *s);
int vars_set_ptr(vars_t *vars, const char *key, void *p);


#endif