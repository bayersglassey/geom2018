
/*

    Can we derive vec_rot, vec_flip for a given space from a set of
    equations in its dims?

    Or like -- R(A) is always B, R(B) is always C, and all we need to specify
    is R(D) because that's the last dim.

    It should also be possible to derive flip given the definition of flip(B)
    a.k.a. F(R(A)).

    Hmmmm.

*/



#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geom.h"


/* For theoretical speed boost, but possible errors, turn this off */
#ifndef GEOM_ROT_SAFE
#define GEOM_ROT_SAFE true
#endif


rot_t rot_contain(int rot_max, rot_t r){
#   if GEOM_ROT_SAFE
        while(r < 0)r += rot_max;
        while(r >= rot_max)r -= rot_max;
        return r;
#   else
        return r > rot_max? rot_max - r: r;
#   endif
}

rot_t rot_rot(int rot_max, rot_t r1, rot_t r2){
    return rot_contain(rot_max, r1 + r2);
}

bool rot_eq(int rot_max, rot_t r1, rot_t r2){
    return rot_contain(rot_max, r1) == rot_contain(rot_max, r2);
}



void vec_zero(vec_t v){
    for(int i = MAX_VEC_DIMS - 1; i >= 0; i--){
        v[i] = 0;
    }
}

void vec_neg(int dims, vec_t v){
    for(int i = dims - 1; i >= 0; i--){
        v[i] = -v[i];
    }
}

void vec_fprintf(FILE *f, int dims, vec_t v){
    fprintf(f, "(% i", v[0]);
    for(int i = 1; i < dims; i++){
        fprintf(f, " % i", v[i]);
    }
    fprintf(f, ")");
}

void vec_printf(int dims, vec_t v){
    vec_fprintf(stdout, dims, v);
}

void vec_cpy(int dims, vec_t v, vec_t w){
    for(int i = dims - 1; i >= 0; i--){
        v[i] = w[i];
    }
}

void vec_add(int dims, vec_t v, vec_t w){
    for(int i = dims - 1; i >= 0; i--){
        v[i] += w[i];
    }
}

void vec_sub(int dims, vec_t v, vec_t w){
    for(int i = dims - 1; i >= 0; i--){
        v[i] -= w[i];
    }
}

void vec_addn(int dims, vec_t v, vec_t w, int n){
    for(int i = dims - 1; i >= 0; i--){
        v[i] += w[i] * n;
    }
}

bool vec_eq(int dims, vec_t v, vec_t w){
    for(int i = dims - 1; i >= 0; i--){
        if(v[i] != w[i])return false;
    }
    return true;
}

void vec_nmul(int dims, vec_t v, int n){
    for(int i = dims - 1; i >= 0; i--){
        v[i] *= n;
    }
}


void vec_apply(const vecspace_t *space, vec_t v, trf_t *t){
    space->vec_flip(v, t->flip);
    space->vec_rot(v, t->rot);
    vec_add(space->dims, v, t->add);
}

void vec_apply_inv(const vecspace_t *space, vec_t v, trf_t *t){
    vec_sub(space->dims, v, t->add);
    space->vec_rot(v, rot_inv(space->rot_max, t->rot));
    space->vec_flip(v, t->flip);
}

void vec_mul(const vecspace_t *space, vec_t v, vec_t w){
    int dims = space->dims;

    vec_t v0;
    vec_cpy(dims, v0, v);

    vec_nmul(dims, v, w[0]);

    for(int i = 1; i < dims; i++){
        space->vec_rot(v0, 1);
        vec_addn(dims, v, v0, w[i]);
    }
}



void boundbox_init(boundbox_t box, int dims){
    for(int i = 0; i < dims; i++){
        box[i*2] = 0;
        box[i*2+1] = 0;
    }
}

void boundbox_fprintf(FILE *f, int dims, boundbox_t box){
    fprintf(f, "[% i % i", box[0], box[1]);
    for(int i = 1; i < dims; i++){
        fprintf(f, " | % i % i", box[i*2], box[i*2+1]);
    }
    fprintf(f, "]");
}

void boundbox_printf(int dims, boundbox_t box){
    boundbox_fprintf(stdout, dims, box);
}



void trf_zero(trf_t *trf){
    vec_zero(trf->add);
    trf->rot = 0;
    trf->flip = false;
}

void trf_cpy(const vecspace_t *space, trf_t *trf1, trf_t *trf2){
    vec_cpy(space->dims, trf1->add, trf2->add);
    trf1->rot = trf2->rot;
    trf1->flip = trf2->flip;
}

void trf_fprintf(FILE *f, int dims, trf_t *trf){
    vec_fprintf(f, dims, trf->add);
    fprintf(f, " %2i %c", trf->rot, trf->flip? 't': 'f');
}

void trf_printf(int dims, trf_t *trf){
    trf_fprintf(stdout, dims, trf);
}

bool trf_eq(const vecspace_t *space, trf_t *t, trf_t *s){
    return
        !!t->flip == !!s->flip &&
        rot_eq(space->rot_max, t->rot, s->rot) &&
        vec_eq(space->dims, t->add, s->add);
}

void trf_inv(const vecspace_t *space, trf_t *t){
    flip_t f = t->flip;
    rot_t r = rot_inv(space->rot_max, t->rot);
    space->vec_rot(t->add, r);
    space->vec_flip(t->add, f);
    vec_neg(space->dims, t->add);
    t->flip = f;
    t->rot = rot_flip(space->rot_max, f, r);
}

void trf_rot(const vecspace_t *space, trf_t *t, rot_t rot){
    t->rot = rot_rot(space->rot_max, t->rot, rot);
    space->vec_rot(t->add, rot);
}

void trf_rot_inv(const vecspace_t *space, trf_t *t, rot_t rot){
    rot_t r = rot_inv(space->rot_max, rot);
    space->vec_rot(t->add, r);
    t->rot = rot_rot(space->rot_max, t->rot, r);
}

void trf_flip1(const vecspace_t *space, trf_t *t){
    t->flip = flip_inv(t->flip);
    t->rot = rot_inv(space->rot_max, t->rot);
    space->vec_flip(t->add, true);
}

void trf_flip(const vecspace_t *space, trf_t *t, flip_t f){
    if(f)trf_flip1(space, t);
}

void trf_apply(const vecspace_t *space, trf_t *t, trf_t *s){
    trf_flip(space, t, s->flip);
    trf_rot(space, t, s->rot);
    vec_add(space->dims, t->add, s->add);
}

void trf_apply_inv(const vecspace_t *space, trf_t *t, trf_t *s){
    vec_sub(space->dims, t->add, s->add);
    trf_rot_inv(space, t, s->rot);
    trf_flip(space, t, s->flip);
}

void trf_apply_separately(const vecspace_t *space, trf_t *t,
    vec_t add, rot_t rot, flip_t flip
){
    trf_flip(space, t, flip);
    trf_rot(space, t, rot);
    vec_add(space->dims, t->add, add);
}
