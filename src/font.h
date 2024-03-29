#ifndef _FONT_H_
#define _FONT_H_

#include <stdarg.h>

#include "lexer.h"


/*******
* FONT *
*******/

/* FONT_N_CHARS: number of characters in each font, hardcoded to
extended ASCII for now because we use chars as array indices... for which
we apologize. */
#define FONT_N_CHARS 256

typedef struct font {
    const char *filename;
    int char_w;
    int char_h;

    /* Font should specify autoupper if it does not contain lowercase
    characters. */
    bool autoupper;

    /* Each char_data[c] is NULL or a char_w*char_h array of "pixel
    values" representing the image of character c.
    Each pixel value is a number in the range 0-255, with 0 being
    transparent, and other values representing colours somehow (e.g.
    indices into a palette). */
    unsigned char *char_data[FONT_N_CHARS];

} font_t;

typedef int font_putc_callback_t(void *data, char c);

void font_cleanup(font_t *font);
int font_load(font_t *font, const char *filename, vars_t *vars);
int font_parse(font_t *font, fus_lexer_t *lexer);

#endif