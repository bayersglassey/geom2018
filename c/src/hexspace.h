#ifndef _HEXSPACE_H_
#define _HEXSPACE_H_

#include "geom.h"


void hexspace_set(vec_t v, int x, int y);
void hexspace_rot(vec_t v, rot_t r);
void hexspace_flip(vec_t v, flip_t flip);
void hexspace_render(vec_t v, int *x, int *y);

void vec4_vec_from_hexspace(vec_t v, vec_t w);
rot_t vec4_rot_from_hexspace(rot_t r);


extern vecspace_t hexspace;


#endif