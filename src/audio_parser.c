
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL2/SDL.h>

#include "util.h"
#include "array.h"
#include "lexer.h"
#include "audio.h"




/****************
 * AUDIO PARSER *
 ****************/

void audio_parser_cleanup(audio_parser_t *parser){
    if(parser->vars_own){
        ARRAY_FREE_PTR(audio_parser_variable_t*, parser->vars,
            audio_parser_variable_cleanup)
    }
}

int audio_parser_init(audio_parser_t *parser, audio_buffer_t *buf){
    parser->buf = buf;
    parser->pos = 0;
    parser->beat_len = 0;
    ARRAY_INIT(parser->vars)
    parser->vars_own = true;

    parser->rnd = 1;
        /* random seed can't be 0, the pseudo-random algorithm maps 0 to 0
        and would therefore produce an endless stream of zeroes */

    return 0;
}

int audio_parser_copy(audio_parser_t *parser, audio_parser_t *parser2){
    parser->buf = parser2->buf;
    parser->pos = parser2->pos;
    parser->beat_len = parser2->beat_len;
    /* WARNING: ARRAY_COPY doesn't actually copy the array, it copies a
    reference to it. So after the following call,
    parser->vars == parser2->vars */
    ARRAY_COPY(parser->vars, parser2->vars)
    parser->vars_own = false;
    parser->rnd = parser2->rnd;
    return 0;
}

int audio_parser_parse(audio_parser_t *parser, fus_lexer_t *lexer){
    int err;
    audio_buffer_t *buf = parser->buf;
    while(1){
        if(fus_lexer_done(lexer) || fus_lexer_got(lexer, ")"))break;

        if(fus_lexer_got(lexer, "(")){
            err = fus_lexer_next(lexer);
            if(err)return err;

            audio_parser_t subparser;
            err = audio_parser_copy(&subparser, parser);
            if(err)return err;

            err = audio_parser_parse(&subparser, lexer);
            if(err)return err;

            audio_parser_cleanup(&subparser);

            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }else if(fus_lexer_got(lexer, "+")){
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

            if(fus_lexer_got(lexer, "silent")){
                /* Can be used to comment out indented blocks,
                which is handy */
                err = fus_lexer_next(lexer);
                if(err)return err;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_parse_silent(lexer);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }else if(fus_lexer_got(lexer, "square")){
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

                if(freq1 == 0){
                    return fus_lexer_unexpected(lexer,
                        "@square: freq1 == 0");}

                err = audio_buffer_extend(buf, parser->pos + len);
                if(err)return err;
                err = gen_square(buf, parser->pos,
                    len, freq1, freq2, vol,
                    offset, volinc, freq2inc);
                if(err)return err;
            }else if(fus_lexer_got(lexer, "triangle")){
                err = fus_lexer_next(lexer);
                if(err)return err;

                int len, add, vol, limit,
                    offset=0, addinc1=0, addinc2=0, plus=0;
                bool wacky=false;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_get_int(lexer, &len);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &add);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &vol);
                if(err)return err;
                limit = vol;
                err = fus_lexer_get_attr_int(lexer,
                    "offset", &offset, true);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "limit", &limit, true);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "addinc1", &addinc1, true);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "addinc2", &addinc2, true);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "plus", &plus, true);
                if(err)return err;
                err = fus_lexer_get_attr_bool(lexer,
                    "wacky", &wacky, true);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;

                err = audio_buffer_extend(buf, parser->pos + len);
                if(err)return err;
                err = wacky?
                    gen_triangle_wacky(buf, parser->pos, len, add, vol,
                        offset, limit, addinc1, addinc2):
                    gen_triangle(buf, parser->pos, len, add, vol,
                        offset, limit, addinc1, addinc2, plus);
                if(err)return err;
            }else if(fus_lexer_got(lexer, "noise")){
                err = fus_lexer_next(lexer);
                if(err)return err;

                int len, vol, volinc=0, limit=0, step=1;
                bool wacky=false;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_get_int(lexer, &len);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &vol);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "volinc", &volinc, true);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "limit", &limit, true);
                if(err)return err;
                err = fus_lexer_get_attr_int(lexer,
                    "step", &step, true);
                if(err)return err;
                err = fus_lexer_get_attr_bool(lexer,
                    "wacky", &wacky, true);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;

                if(step == 0){
                    return fus_lexer_unexpected(lexer,
                        "@noise: step == 0");}

                err = audio_buffer_extend(buf, parser->pos + len);
                if(err)return err;
                err = gen_noise(buf, &parser->rnd, parser->pos,
                    len, vol, volinc, limit, step, wacky);
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
                    fus_lexer_t sublexer;
                    err = fus_lexer_copy(&sublexer, lexer);
                    if(err)return err;
                    err = audio_parser_parse(parser, &sublexer);
                    if(err)return err;
                    if(i == n_loops-1){
                        *lexer = sublexer;
                    }else{
                        fus_lexer_cleanup(&sublexer);
                    }
                }
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }else if(fus_lexer_got(lexer, "dump_vars")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                printf("VARS:\n");
                for(int i = 0; i < parser->vars_len; i++){
                    audio_parser_variable_t *var = parser->vars[i];
                    printf("  %s\n", var->name);
                }
            }else if(fus_lexer_got(lexer, "dump_rnd")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                printf("RND: %i\n", parser->rnd);
            }else if(fus_lexer_got(lexer, "dump_data")){
                err = fus_lexer_next(lexer);
                if(err)return err;

                int addpos, len;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_get_int(lexer, &addpos);
                if(err)return err;
                err = fus_lexer_get_int(lexer, &len);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;

                int pos = parser->pos + addpos;
                printf("DATA (%i samples starting at %i):\n", len, pos);
                for(int i = 0; i < len; i++){
                    audio_sample_t sample = buf->data[pos + i];
                    printf("  %i\n", sample);
                }
            }else if(fus_lexer_got(lexer, "def")){
                err = fus_lexer_next(lexer);
                if(err)return err;

                char *name;
                err = fus_lexer_get_name(lexer, &name);
                if(err)return err;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                {
                    ARRAY_PUSH_NEW(audio_parser_variable_t*,
                        parser->vars, var)
                    err = audio_parser_variable_init(var, name);
                    if(err)return err;
                    err = fus_lexer_copy(&var->lexer, lexer);
                    if(err)return err;
                    err = fus_lexer_parse_silent(lexer);
                    if(err)return err;
                }
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }else if(fus_lexer_got(lexer, "print")){
                err = fus_lexer_next(lexer);
                if(err)return err;

                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                while(1){
                    if(fus_lexer_got(lexer, ")"))break;

                    char *text;
                    err = fus_lexer_get_str(lexer, &text);
                    if(err)return err;
                    printf("[audio parser] %s\n", text);
                    free(text);
                }
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }else if(fus_lexer_got(lexer, "beat")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_get_int(lexer, &parser->beat_len);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }else{
                return fus_lexer_unexpected(lexer,
                    "audio operation (name of built-in following \"@\")");
            }
        }else if(fus_lexer_got_name(lexer)){
            char *name;
            err = fus_lexer_get_name(lexer, &name);
            if(err)return err;

            audio_parser_variable_t *var =
                audio_parser_get_var(parser, name);
            if(var == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find variable %s\n", name);
                free(name); return 2;}
            free(name);

            fus_lexer_t sublexer;
            err = fus_lexer_copy(&sublexer, &var->lexer);
            if(err)return err;
            err = audio_parser_parse(parser, &sublexer);
            if(err)return err;
            fus_lexer_cleanup(&sublexer);
        }else{
            return fus_lexer_unexpected(lexer,
                "audio operation");
        }
    }
    return 0;
}

audio_parser_variable_t *audio_parser_get_var(audio_parser_t *parser,
    const char *name
){
    /* iterate backwards, so freshly-added variables override old ones
    with same name */
    for(int i = parser->vars_len - 1; i >= 0; i--){
        audio_parser_variable_t *var = parser->vars[i];
        if(!strcmp(var->name, name))return var;
    }
    return NULL;
}


void audio_parser_variable_cleanup(audio_parser_variable_t *var){
    free(var->name);
}

int audio_parser_variable_init(audio_parser_variable_t *var, char *name){
    var->name = name;
    return 0;
}

