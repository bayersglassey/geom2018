
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL2/SDL.h>

#include "util.h"
#include "array.h"
#include "lexer.h"
#include "audio.h"



/****************
 * AUDIO BUFFER *
 ****************/

#define AUDIO_BUF_RANGE_CHECK(buf, pos, len) \
    if(pos < 0){ \
        ERR_INFO(); \
        fprintf(stderr, "Out of range: %i < 0\n", pos); \
        return 2; \
    } \
    if(pos + len > buf->data_len){ \
        ERR_INFO(); \
        fprintf(stderr, "Out of range: %i+%i = %i > %i\n", \
            pos, len, pos+len, buf->data_len); \
        return 2; \
    }


void audio_buffer_cleanup(audio_buffer_t *buf){
    ARRAY_FREE(audio_sample_t, buf->data, (void))
}

int audio_buffer_init(audio_buffer_t *buf){
    ARRAY_INIT(buf->data)
    return 0;
}

int audio_buffer_extend(audio_buffer_t *buf, int new_data_len){
    if(new_data_len <= buf->data_len)return 0;
    audio_sample_t silence = 0;
    ARRAY_PUSH_MANY(audio_sample_t, buf->data, silence,
        new_data_len - buf->data_len)
    return 0;
}


int audio_buffer_load(audio_buffer_t *buf, const char *filename){
    int err;
    fus_lexer_t lexer;
    audio_parser_t parser;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = audio_parser_init(&parser, buf);
    if(err)return err;

    err = audio_parser_parse(&parser, &lexer);
    if(err)return err;

    audio_parser_cleanup(&parser);
    free(text);
    return 0;
}



/*********************
 * BUFFER OPERATIONS *
 *********************/

int audio_buffer_limit(audio_buffer_t *buf,
    int pos, int len, int maxvol
){
    /* CURRENTLY UNUSED... */
    AUDIO_BUF_RANGE_CHECK(buf, pos, len)
    for(int i = 0; i < len; i++){
        buf->data[pos + i] = int_min(buf->data[pos + i], maxvol);
    }
    return 0;
}

int gen_square(audio_buffer_t *buf,
    int pos, int len,
    int freq1, int freq2, int vol,
    int offset, int volinc, int freq2inc
){
    AUDIO_BUF_RANGE_CHECK(buf, pos, len)
    for(int i = 0; i < len; i++){
        int val = (i + offset) % freq1 < freq2? vol: 0;
        buf->data[pos + i] += val;
        vol += volinc;
        if(vol < 0)vol = 0;
        freq2 = (freq2 + freq2inc) % freq1;
    }
    return 0;
}

int gen_triangle(audio_buffer_t *buf,
    int pos, int len,
    int add, int vol, int offset,
    int limit, int addinc1, int addinc2, int plus
){
    AUDIO_BUF_RANGE_CHECK(buf, pos, len)
    int y = offset;
    bool up = true;
    for(int i = 0; i < len; i++){
        int value = int_min(y, limit) + plus;
        buf->data[pos + i] += value;
        y += up? add: -add;
        add += up? addinc1: addinc2;
        if(y >= vol){y = vol; up = false;}
        if(y <= 0){y = 0; up = true;}
    }
    return 0;
}

int gen_triangle_wacky(audio_buffer_t *buf,
    int pos, int len,
    int add, int vol, int offset,
    int limit, int addinc1, int addinc2
){
    AUDIO_BUF_RANGE_CHECK(buf, pos, len)
    int y = offset;
    bool up = true;
    for(int i = 0; i < len; i++){
        int value = int_min(y, limit) - limit;
        buf->data[pos + i] += value;
        y += up? add: -add;
        add += up? addinc1: addinc2;
        if(y >= vol){y = vol; up = false;}
        if(y <= -vol){y = -vol; up = true;}
    }
    return 0;
}

static int rnd_next(int rnd){
    /* Galois linear feedback shift register */
    int bit = rnd & 1;
    rnd >>= 1;
    if(bit)rnd ^= 0xB400;
    return rnd;
}

int gen_noise(audio_buffer_t *buf, int *rnd_ptr,
    int pos, int len, int vol, int volinc, int limit, int step,
    bool wacky
){
    AUDIO_BUF_RANGE_CHECK(buf, pos, len)
    int rnd = *rnd_ptr;

    if(rnd == 0){
        /* In case random seed ever hits 0, bump it to 1 so we don't
        get stuck with a stream of zeroes */
        rnd = 1;
        fprintf(stderr, "gen_noise: bumping random seed from 0 to 1\n");
    }

    for(int i = 0; i < len; i++){
        if(vol <= 0)break;
            /* Avoid modulus-by-zero... */

        int y = wacky?
            int_max(rnd % vol, limit) - limit:
            int_min(rnd % vol, limit);
        buf->data[pos + i] += y;
        vol += volinc;
        if(i % step == 0)rnd = rnd_next(rnd);
    }
    *rnd_ptr = rnd;
    return 0;
}

