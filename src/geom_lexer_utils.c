
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"
#include "vec4.h"
#include "lexer.h"



static int fus_lexer_get_vec_simple(fus_lexer_t *lexer,
    vecspace_t *space, vec_t vec
){
    int err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    for(int i = 0; i < space->dims; i++){
        err = fus_lexer_get_int_fancy(lexer, &vec[i]);
        if(err)return err;
    }
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    return 0;
}

int fus_lexer_eval_vec(fus_lexer_t *lexer, vecspace_t *space, vec_t vec){
    int err;

    vec_zero(vec);
    bool neg = false;
    while(1){

        if(fus_lexer_got(lexer, "-")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            neg = true;
        }

        vec_t add;
        if(fus_lexer_got(lexer, "0") || fus_lexer_got(lexer, "-0")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            vec_zero(add);
        }else if(fus_lexer_got(lexer, "eval")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_eval_vec(lexer, space, add);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }else if(space == &vec4 && fus_lexer_got_chr(lexer)){
            /* Maybe TODO: have a generic space->get_unit(rot) ?.. */
            char c;
            err = fus_lexer_get_chr(lexer, &c);
            if(err)return err;
            if(c >= 'A' && c <= 'G'){
                rot_t rot = c - 'A';
                vec_cpy(space->dims, add, vec4_units[rot]);
            }else{
                return fus_lexer_unexpected(lexer, "A - G");
            }
        }else{
            err = fus_lexer_get_vec_simple(lexer, space, add);
            if(err)return err;
        }

        if(neg)vec_neg(space->dims, add);
        neg = false;

        while(1){
            if(fus_lexer_got(lexer, "*")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                int n;
                err = fus_lexer_get_int(lexer, &n);
                if(err)return err;

                vec_nmul(space->dims, add, n);
            }else if(fus_lexer_got(lexer, "^")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                int n;
                err = fus_lexer_get_int(lexer, &n);
                if(err)return err;

                rot_t rot = rot_contain(space->rot_max, n);
                space->vec_rot(add, n);
            }else if(fus_lexer_got(lexer, "~")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                space->vec_flip(add, true);
            }else{
                break;
            }
        }

        vec_add(space->dims, vec, add);

        if(fus_lexer_done(lexer) || fus_lexer_got(lexer, ")"))break;

        if(fus_lexer_got(lexer, "-")){
            neg = true;
            err = fus_lexer_next(lexer);
            if(err)return err;
        }else if(fus_lexer_got(lexer, "+")){
            err = fus_lexer_get(lexer, "+");
            if(err)return err;
        }else{
            return fus_lexer_unexpected(lexer, "+ or -");
        }
    }

    return 0;
}

int fus_lexer_get_vec(fus_lexer_t *lexer, vecspace_t *space, vec_t vec){
    int err;

    if(fus_lexer_got(lexer, "(")){
        return fus_lexer_get_vec_simple(lexer, space, vec);}

    err = fus_lexer_get(lexer, "eval");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_eval_vec(lexer, space, vec);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;
    return 0;
}

int fus_lexer_get_trf(fus_lexer_t *lexer, vecspace_t *space, trf_t *trf){
    int err;
    err = fus_lexer_get_vec(lexer, space, trf->add);
    if(err)return err;
    err = fus_lexer_get_int_fancy(lexer, &trf->rot);
    if(err)return err;
    err = fus_lexer_get_bool(lexer, &trf->flip);
    if(err)return err;
    return 0;
}
