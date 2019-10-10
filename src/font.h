#ifndef _FONT_H_
#define _FONT_H_

#include <stdarg.h>

#include "lexer.h"

/* FONT_N_COLOR_VALUES: the number of valid "pixel values" for
font->char_data[i][j].
These values can used treated as e.g. palette indices, so this
could e.g. be used as the number of colours in such a paletter. */
#define FONT_N_COLOR_VALUES 4


/*******
* FONT *
*******/

/* FONT_N_CHARS: number of characters in each font, hardcoded to
ASCII for now because we use chars as array indices... for which
we apologize. */
#define FONT_N_CHARS 128

typedef struct font {
    int char_w;
    int char_h;

    /* Each char_data[c] is NULL or a char_w*char_h array of "pixel
    values" representing the image of character c.
    Each pixel value is a number in the range 0-255, with 0 being
    transparent, and other values representing colours somehow (e.g.
    indices into a palette). */
    unsigned char *char_data[FONT_N_CHARS];

} font_t;

typedef int font_putc_callback_t(void *data, char c);

void font_cleanup(font_t *font);
int font_load(font_t *font, const char *filename);
int font_parse(font_t *font, fus_lexer_t *lexer);
int font_printf(font_putc_callback_t *callback, void *callback_data,
    const char *msg, ...);
int font_vprintf(font_putc_callback_t *callback, void *callback_data,
    const char *msg, va_list vlist);

#endif