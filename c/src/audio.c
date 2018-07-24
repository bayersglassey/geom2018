
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

int audio_buffer_extend(audio_buffer_t *buf, int len){
    audio_sample_t silence = 0;
    ARRAY_PUSH_MANY(audio_sample_t, buf->data, silence,
        len - buf->data_len)
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

    err = audio_parser_init(&parser, buf, 250);
    if(err)return err;

    err = audio_parser_parse(&parser, &lexer);
    if(err)return err;

    free(text);
    return 0;
}




/****************
 * AUDIO PARSER *
 ****************/

void audio_parser_cleanup(audio_parser_t *parser){
    /* ain't gotta do nuthin */
}

int audio_parser_init(audio_parser_t *parser,
    audio_buffer_t *buf, int beat_len
){
    parser->buf = buf;
    parser->pos = 0;
    parser->beat_len = beat_len;
    return 0;
}

int audio_parser_parse(audio_parser_t *parser, fus_lexer_t *lexer){
    int err;
    audio_buffer_t *buf = parser->buf;
    while(1){
        if(fus_lexer_done(lexer) || fus_lexer_got(lexer, ")"))break;

        if(fus_lexer_got(lexer, "+")){
            err = fus_lexer_next(lexer);
            if(err)return err;

            int addpos;
            err = fus_lexer_get_int(lexer, &addpos);
            if(err)return err;
            parser->pos += addpos;
        }else if(fus_lexer_got(lexer, ",")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            parser->pos += parser->beat_len;
        }else if(fus_lexer_got(lexer, ",,")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            parser->pos += parser->beat_len * 2;
        }else if(fus_lexer_got(lexer, "@")){
            err = fus_lexer_next(lexer);
            if(err)return err;

            if(fus_lexer_got(lexer, "square")){
                err = fus_lexer_next(lexer);
                if(err)return err;

                int len, freq1, freq2, vol,
                    offset=0, volinc=0, freq2inc=0;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_get_int(lexer, &len);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &freq1);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &freq2);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &vol);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "offset", &offset, true);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "volinc", &volinc, true);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "freq2inc", &freq2inc, true);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;

                err = audio_buffer_extend(buf, parser->pos + len);
                if(err)return err;
                err = gen_square(buf, parser->pos,
                    len, freq1, freq2, vol,
                    offset, volinc, freq2inc);
                if(err)return err;
            }else if(fus_lexer_got(lexer, "triangle")){
                err = fus_lexer_next(lexer);
                if(err)return err;

                int len, add, vol,
                    offset=0, limit=0, addinc1=0, addinc2=0;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_get_int(lexer, &parser->pos);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &len);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &add);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &vol);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;

                err = audio_buffer_extend(buf, parser->pos + len);
                if(err)return err;
                err = gen_triangle(buf, parser->pos, len, add, vol,
                    offset, limit, addinc1, addinc2);
                if(err)return err;
            }else if(fus_lexer_got(lexer, "loop")){
                err = fus_lexer_next(lexer);
                if(err)return err;

                int n_loops;
                err = fus_lexer_get_int(lexer, &n_loops);
                if(err)return err;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                for(int i = 0; i < n_loops; i++){
                    printf("LOOP %i\n", i);
                    fus_lexer_t loop_lexer;
                    fus_lexer_copy(&loop_lexer, lexer);
                    err = audio_parser_parse(parser, &loop_lexer);
                    if(err)return err;

                    if(i == n_loops-1){
                        *lexer = loop_lexer;
                    }else{
                        fus_lexer_cleanup(&loop_lexer);
                    }
                }
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }else{
                return fus_lexer_unexpected(lexer,
                    "square or triangle or loop");
            }
        }else{
            return fus_lexer_unexpected(lexer, NULL);
        }
    }
    return 0;
}





/*********************
 * BUFFER OPERATIONS *
 *********************/

int audio_buffer_limit(audio_buffer_t *buf,
    int pos, int len, int maxvol
){
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
        freq2 = (freq2 + freq2inc) % freq1;
    }
    return 0;
}

int gen_triangle(audio_buffer_t *buf,
    int pos, int len,
    int add, int vol, int offset,
    int limit, int addinc1, int addinc2
){
    AUDIO_BUF_RANGE_CHECK(buf, pos, len)
    int y = offset;
    bool up = true;
    for(int i = 0; i < len; i++){
        buf->data[pos + i] += int_min(y, limit);
        y += up? add: -add;
        add += up? addinc1: addinc2;
        if(y >= vol)up = false;
        if(y <= -vol)up = true;
    }
    return 0;
}
