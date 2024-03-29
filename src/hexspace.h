#ifndef _HEXSPACE_H_
#define _HEXSPACE_H_

#include "geom.h"

/*

  Y +
     \
     (+)- +
     /    X
  Z +

  X + Y + Z = 0

  ...so, coords are given as (x y)
  And if you need it, you've got z = -x -y

*/


#define HEXSPACE_DIMS 2
#define HEXSPACE_ROT_MAX 6


void hexspace_set(vec_t v, int x, int y);
void hexspace_rot1(vec_t v);
void hexspace_rot(vec_t v, rot_t r);
void hexspace_flip1(vec_t v);
void hexspace_flip(vec_t v, flip_t flip);
void hexspace_render(vec_t v, int *x, int *y);
void hexspace_angle(vec_t v, rot_t *rot_ptr, int *dist, int *angle);
int hexspace_dist(vec_t v, vec_t w);

void vec4_vec_from_hexspace(vec_t v, vec_t w);
rot_t vec4_rot_from_hexspace(rot_t r);
void vec4_coords_from_hexspace(
    vec_t hex_pos, rot_t hex_rot, flip_t hex_flip,
    vec_t vec4_pos, rot_t *vec4_rot, flip_t *vec4_flip);


extern vecspace_t hexspace;


#endif