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
    VAL_TYPE_ARR,
    VAL_TYPES
};

typedef struct val {
    int type; /* enum var_type */
    union {
        bool b;
        int i;
        struct {
            const char *s;
            char *own_s; /* If s == own_s, we own the string */
        } s;
        struct {
            int len;
            struct val *vals;
                /* Must always contain an extra element, which is a val with
                .type = VAL_TYPE_INT, .u.i == len.
                See also: val_get_arr_len_val, which returns that element.
                Why do we do this?.. because currently, valexpr_eval doesn't
                manage give you any way to manage the memory of the returned
                val: it's assumed to be owned by something.
                So it can't just create a fresh val of type VAL_TYPE_INT. */
        } a;
    } u;
} val_t;

/* Constant values, for e.g. valexpr_t, which likes to return pointers
to val_t */
extern val_t val_null;
extern val_t val_true;
extern val_t val_false;

typedef unsigned var_props_t;

typedef struct var {
    char *key;
    val_t value;
    var_props_t props; /* Bit array */
} var_t;

struct vars;
typedef int vars_callback_t(struct vars *vars, var_t *var);

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

    /* Weakrefs: */
    const char **prop_names;
    int prop_names_len;
        /* prop_names: NULL-terminated array of strings representing
        possible variable "properties".
        These can be whatever you like, for example if you're using vars_t
        to model a C-like language, you might have "static", "const", etc.
        The index of each prop in this array corresponds to a bit in the
        var->props bit array.
        (So, for each var, each prop can be either active or not.) */

    /* Callback (called whenever any var in vars is modified): */
    vars_callback_t *callback;
    void *callback_data;

} vars_t;


void var_cleanup(var_t *var);
void var_init(var_t *var, char *key);
void var_set_prop(var_t *var, unsigned i);

void val_cleanup(val_t *val);
void val_init(val_t *val);
void val_fprintf(val_t *val, FILE *file);
const char *val_type_name(int type);

val_t *val_bool(bool b);
val_t *val_safe(val_t *val);

bool val_get_bool(val_t *val);
int val_get_int(val_t *val);
const char *val_get_str(val_t *val);
int val_get_arr_len(val_t *val);
val_t *val_get_arr_len_val(val_t *val);
val_t *val_get_arr_item(val_t *val, int i);

void val_set_null(val_t *val);
void val_set_bool(val_t *val, bool b);
void val_set_int(val_t *val, int i);
void val_set_str(val_t *val, char *s);
void val_set_const_str(val_t *val, const char *s);
int val_set_arr(val_t *val, int len);
int val_resize_arr(val_t *val, int len);

int val_copy(val_t *val1, val_t *val2);

bool val_eq(val_t *val1, val_t *val2);
bool val_ne(val_t *val1, val_t *val2);
bool val_lt(val_t *val1, val_t *val2);
bool val_le(val_t *val1, val_t *val2);
bool val_gt(val_t *val1, val_t *val2);
bool val_ge(val_t *val1, val_t *val2);
bool val_and(val_t *val1, val_t *val2);
bool val_or(val_t *val1, val_t *val2);
bool val_not(val_t *val);


void vars_cleanup(vars_t *vars);
void vars_init(vars_t *vars);
void vars_init_with_props(vars_t *vars, const char **prop_names);
void vars_dump(vars_t *vars);
void vars_dumpvar(vars_t *vars, const char *key);
int vars_add(vars_t *vars, char *key, var_t **var_ptr);
var_t *vars_get(vars_t *vars, const char *key);
var_t *vars_get_or_add(vars_t *vars, const char *key);
int vars_get_prop_i(vars_t *vars, const char *prop_name);

bool vars_get_bool(vars_t *vars, const char *key);
int vars_get_int(vars_t *vars, const char *key);
const char *vars_get_str(vars_t *vars, const char *key);
int vars_get_arr_len(vars_t *vars, const char *key);
val_t *vars_get_arr_item(vars_t *vars, const char *key, int i);

int vars_set_null(vars_t *vars, const char *key);
int vars_set_bool(vars_t *vars, const char *key, bool b);
int vars_set_int(vars_t *vars, const char *key, int i);
int vars_set_str(vars_t *vars, const char *key, char *s);
int vars_set_const_str(vars_t *vars, const char *key, const char *s);

int vars_set_all_null(vars_t *vars);
int vars_copy(vars_t *vars1, vars_t *vars2);
int vars_callback(vars_t *vars, var_t *var);
int vars_callback_all(vars_t *vars);

#endif
