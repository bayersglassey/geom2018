#ifndef _LEXER_VAR_UTILS_H_
#define _LEXER_VAR_UTILS_H_

#include <stdio.h>

#include "lexer.h"
#include "vars.h"

int val_parse(val_t *val, fus_lexer_t *lexer);

void vars_write(vars_t *vars, FILE *file, int indent);
void vars_write_simple(vars_t *vars, FILE *file);
int vars_parse(vars_t *vars, fus_lexer_t *lexer);
int vars_load(vars_t *vars, const char *filename);

#endif