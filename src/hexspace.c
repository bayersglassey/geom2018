
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexspace.h"


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


vecspace_t hexspace = {
    HEXSPACE_DIMS,
    HEXSPACE_ROT_MAX,
    hexspace_rot,
    hexspace_flip,
    hexspace_render
};


void hexspace_set(vec_t v, int x, int y){
    v[0] = x;
    v[1] = y;
}

void hexspace_rot1(vec_t v){
    /* RX = X + Y */
    /* RY = -X */
    int x = v[0] - v[1];
    int y = v[0];
    v[0] = x;
    v[1] = y;
}

void hexspace_rot(vec_t v, rot_t r){
    /* Let's be saaafe */
    r = rot_contain(HEXSPACE_ROT_MAX, r);

    for(int i = r; i > 0; i--)hexspace_rot1(v);
}

void hexspace_flip1(vec_t v){
    /* RX = X */
    /* RY = -X - Y */
    int x = v[0] - v[1];
    int y = -v[1];
    v[0] = x;
    v[1] = y;
}

void hexspace_flip(vec_t v, flip_t flip){
    if(flip)hexspace_flip1(v);
}

void hexspace_render(vec_t v, int *x, int *y){
    /* Basically we transform hexspace to vec4, then render that.
    Hexspace to vec4: X -> A, Y -> C - A */

    *x = 2 * v[0] - v[1];
    *y = -2 * v[1];
}

void hexspace_angle(vec_t v, rot_t *rot_ptr, int *dist, int *angle){
    if(v[0] == 0 && v[1] == 0){
        /* Doesn't make sense to get the angle at the origin; but
        we do return dist, which will be zero, and caller can check that,
        ignoring rot and angle unless dist > 0. */
        *rot_ptr = 0;
        *dist = 0;
        *angle = 0;
        return;
    }
    vec_t v_cpy;
    vec_cpy(HEXSPACE_DIMS, v_cpy, v);
    rot_t rot = HEXSPACE_ROT_MAX;
    while(
        v_cpy[0] < 0 ||
        v_cpy[1] < 0 ||
        v_cpy[1] >= v_cpy[0]
    ){
        hexspace_rot1(v_cpy);
        rot--;
    }
    if(rot == HEXSPACE_ROT_MAX)rot = 0;
    *rot_ptr = rot;
    *dist = v_cpy[0];
    *angle = v_cpy[1];
}

int hexspace_dist(vec_t v, vec_t w){
    rot_t rot;
    int angle, dist;
    vec_t diff;
    vec_cpy(HEXSPACE_DIMS, diff, v);
    vec_sub(HEXSPACE_DIMS, diff, w);
    hexspace_angle(diff, &rot, &dist, &angle);
    return dist;
}


void vec4_vec_from_hexspace(vec_t v, vec_t w){
    /* Hexspace to vec4: X -> A, Y -> C - A */
    v[0] = w[0] - w[1];
    v[1] = 0;
    v[2] = w[1];
    v[3] = 0;
}

rot_t vec4_rot_from_hexspace(rot_t r){
    return r * 2;
}

void vec4_coords_from_hexspace(
    vec_t hex_pos, rot_t hex_rot, flip_t hex_flip,
    vec_t vec4_pos, rot_t *vec4_rot, flip_t *vec4_flip
){
    vec4_vec_from_hexspace(vec4_pos, hex_pos);
    *vec4_rot = vec4_rot_from_hexspace(hex_rot);
    *vec4_flip = hex_flip;
}
