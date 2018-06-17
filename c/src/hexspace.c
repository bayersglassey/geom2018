
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexspace.h"

#define HEXSPACE_DIMS 2
#define HEXSPACE_ROT_MAX 6


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
    fprintf(stderr, "TODO: IMPLEMENT HEXSPACE ROT\n");

    /* Let's be saaafe */
    r = rot_contain(HEXSPACE_ROT_MAX, r);

    for(int i = r; i > 0; i--){
        int x = v[0];
        int y = v[1];
        v[0] = -x;
        v[1] = y;
    }
}

void hexspace_flip(vec_t v, flip_t flip){
    fprintf(stderr, "TODO: IMPLEMENT HEXSPACE FLIP\n");

    if(flip){
        int x = v[0];
        int y = v[1];
        v[0] = x;
        v[1] = -y;
    }
}

void hexspace_render(vec_t v, int *x, int *y){
    fprintf(stderr, "TODO: IMPLEMENT HEXSPACE RENDER\n");

    *x = v[0];
    *y = -v[1];
}

