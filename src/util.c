
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "util.h"

bool get_bool_env(const char *name){
    /* Is indicated environment variable defined and nonempty? */
    const char *value = getenv(name);
    return value && strlen(value);
}

int int_min(int x, int y){
    return x < y? x: y;
}

int int_max(int x, int y){
    return x > y? x: y;
}

int randint(int min, int max){
    int diff = max - min;
    return (diff? rand() % diff: 0) + min;
}

int linear_interpolation(int x0, int x1, int t, int t_max){
    int diff = x1 - x0;
    return x0 + diff * t / t_max;
}

void get_spaces(char *spaces, int max_spaces, int n_spaces){
    if(n_spaces > max_spaces){
        fprintf(stderr, "Can't handle %i spaces - max %i\n",
            n_spaces, max_spaces);
        n_spaces = max_spaces;
    }
    for(int i = 0; i < n_spaces; i++)spaces[i] = ' ';
    spaces[n_spaces] = '\0';
}
