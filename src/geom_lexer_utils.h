#ifndef _GEOM_LEXER_UTILS_H_
#define _GEOM_LEXER_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"
#include "lexer.h"


int fus_lexer_eval_vec(fus_lexer_t *lexer, vecspace_t *space, vec_t vec);
int fus_lexer_get_vec(fus_lexer_t *lexer, vecspace_t *space, vec_t vec);
int fus_lexer_get_trf(fus_lexer_t *lexer, vecspace_t *space, trf_t *trf);

#endif