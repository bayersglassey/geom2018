#ifndef _VEC4_H_
#define _VEC4_H_

#include "geom.h"

/*

      D
        C
      +
      . +
      .. + B
      ...
      *...+ A

*/


#define VEC4_DIMS 4
#define VEC4_ROT_MAX 12


void vec4_set(vec_t v, int a, int b, int c, int d);
void vec4_rot1(vec_t v);
void vec4_rot(vec_t v, rot_t r);
void vec4_flip1(vec_t v);
void vec4_flip(vec_t v, flip_t flip);
void vec4_render(vec_t v, int *x, int *y);
void vec4_render_alt(vec_t v, int *x, int *y);

extern vecspace_t vec4;
extern vecspace_t vec4_alt;


#endif