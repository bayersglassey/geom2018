#ifndef _VEC4_H_
#define _VEC4_H_

#include "geom.h"


void vec4_set(vec_t v, int a, int b, int c, int d);
bool vec4_test(vec_t v, int a, int b, int c, int d);
void vec4_rot(vec_t v, rot_t r);
void vec4_flip(vec_t v, flip_t flip);

extern vecspace_t vec4;


#endif