#ifndef _GEOM_H_
#define _GEOM_H_



#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef MAX_VEC_DIMS
#   define MAX_VEC_DIMS 4
#endif

typedef int vec_t[MAX_VEC_DIMS];
typedef int boundbox_t[MAX_VEC_DIMS*2];
typedef int rot_t;
typedef bool flip_t;
typedef struct trf {
    /* a transformation -- applied in this order: flip, rot, add */
    flip_t flip;
    rot_t rot;
    vec_t add;
} trf_t;


typedef struct vecspace {
    int dims;
    int rot_max;

    void (*vec_rot)(vec_t v, rot_t r);
    void (*vec_flip)(vec_t v, flip_t flip);
    void (*vec_render)(vec_t v, int *x, int *y);
} vecspace_t;




#define rot_inv(rot_max, rot) ((rot) == 0? 0: (rot_max) - (rot))
#define rot_flip(rot_max, rot, flip) ((flip)? rot_inv((rot_max), (rot)): (rot))
#define flip_inv(flip) (!(flip))



rot_t rot_contain(int rot_max, rot_t r);
rot_t rot_rot(int rot_max, rot_t r1, rot_t r2);
bool rot_eq(int rot_max, rot_t r1, rot_t r2);

void vec_zero(int dims, vec_t v);
void vec_neg(int dims, vec_t v);
void vec_fprintf(FILE *f, int dims, vec_t v);
void vec_printf(int dims, vec_t v);
void vec_cpy(int dims, vec_t v, vec_t w);
void vec_add(int dims, vec_t v, vec_t w);
void vec_sub(int dims, vec_t v, vec_t w);
void vec_addn(int dims, vec_t v, vec_t w, int n);
bool vec_eq(int dims, vec_t v, vec_t w);
void vec_nmul(int dims, vec_t v, int n);
void vec_apply(vecspace_t *space, vec_t v, trf_t *t);
void vec_apply_inv(vecspace_t *space, vec_t v, trf_t *t);
void vec_mul(vecspace_t *space, vec_t v, vec_t w);

void boundbox_init(boundbox_t box, int dims);
void boundbox_fprintf(FILE *f, int dims, boundbox_t box);
void boundbox_printf(int dims, boundbox_t box);

void trf_fprintf(FILE *f, int dims, trf_t *trf);
void trf_printf(int dims, trf_t *trf);
bool trf_eq(vecspace_t *space, trf_t *t, trf_t *s);
void trf_inv(vecspace_t *space, trf_t *t);
void trf_apply(vecspace_t *space, trf_t *t, trf_t *s);
void trf_apply_inv(vecspace_t *space, trf_t *t, trf_t *s);

#endif