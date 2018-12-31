#ifndef _VARS_H_
#define _VARS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "array.h"


typedef struct var {
    char *key;
    char type; /* 'n', 'b', 'i', 's' */
    union {
        bool b;
        int i;
        char *s;
    } value;
} var_t;

typedef struct vars {
    ARRAY_DECL(var_t*, vars)
} vars_t;


void var_cleanup(var_t *var);
int var_init(var_t *var, char *key);

void var_unset(var_t *var);
void var_set_null(var_t *var);
void var_set_bool(var_t *var, bool b);
void var_set_int(var_t *var, int i);
void var_set_str(var_t *var, char *s);

void vars_cleanup(vars_t *vars);
int vars_init(vars_t *vars);
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


#endif