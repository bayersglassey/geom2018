#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL2/SDL.h>

#include "array.h"
#include "lexer.h"


typedef Uint16 audio_sample_t;


/****************
 * AUDIO BUFFER *
 ****************/

typedef struct audio_buffer {
    ARRAY_DECL(audio_sample_t, data)
} audio_buffer_t;


void audio_buffer_cleanup(audio_buffer_t *buf);
int audio_buffer_init(audio_buffer_t *buf);
int audio_buffer_extend(audio_buffer_t *buf, int new_data_len);
int audio_buffer_load(audio_buffer_t *buf, const char *filename);



/****************
 * AUDIO PARSER *
 ****************/
typedef struct audio_parser {
    audio_buffer_t *buf;
    int pos;
    int beat_len;
    ARRAY_DECL(struct audio_parser_variable*, vars)
    bool vars_own;

    int rnd;
        /* Random number */

} audio_parser_t;

typedef struct audio_parser_variable {
    char *name;
    fus_lexer_t lexer;
} audio_parser_variable_t;

void audio_parser_cleanup(audio_parser_t *parser);
int audio_parser_init(audio_parser_t *parser, audio_buffer_t *buf);
int audio_parser_copy(audio_parser_t *parser, audio_parser_t *parser2);
int audio_parser_parse(audio_parser_t *parser, fus_lexer_t *lexer);
audio_parser_variable_t *audio_parser_get_var(audio_parser_t *parser,
    const char *name);

void audio_parser_variable_cleanup(audio_parser_variable_t *var);
int audio_parser_variable_init(audio_parser_variable_t *var, char *name);



/*********************
 * BUFFER OPERATIONS *
 *********************/

int audio_buffer_limit(audio_buffer_t *buf,
    int pos, int len, int maxvol);
int gen_square(audio_buffer_t *buf,
    int pos, int len,
    int freq1, int freq2, int vol,
    int offset, int volinc, int freq2inc);
int gen_triangle(audio_buffer_t *buf,
    int pos, int len,
    int add, int vol, int offset,
    int limit, int addinc1, int addinc2);
int gen_noise(audio_buffer_t *buf, int *rnd_ptr,
    int pos, int len, int vol, int volinc, int limit, int step);


#endif