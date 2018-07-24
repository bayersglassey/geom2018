
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexspace.h"

#define HEXSPACE_DIMS 2
#define HEXSPACE_ROT_MAX 6


/*

  Y +
     \
     (+)- +
          X

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

void hexspace_rot(vec_t v, rot_t r){
    /* Let's be saaafe */
    r = rot_contain(HEXSPACE_ROT_MAX, r);

    for(int i = r; i > 0; i--){
        /* RX = X + Y */
        /* RY = -X */
        int x = v[0] - v[1];
        int y = v[0];
        v[0] = x;
        v[1] = y;
    }
}

void hexspace_flip(vec_t v, flip_t flip){
    if(flip){
        /* RX = X */
        /* RY = -X - Y */
        int x = v[0] - v[1];
        int y = -v[1];
        v[0] = x;
        v[1] = y;
    }
}

void hexspace_render(vec_t v, int *x, int *y){
    /* Basically we transform hexspace to vec4, then render that.
    Hexspace to vec4: X -> A, Y -> C - A */

    *x = 2 * v[0] - v[1];
    *y = -2 * v[1];
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

