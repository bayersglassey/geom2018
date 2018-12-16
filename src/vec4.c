
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "vec4.h"


vecspace_t vec4 = {
    VEC4_DIMS,
    VEC4_ROT_MAX,
    vec4_rot,
    vec4_flip,
    vec4_render
};


void vec4_set(vec_t v, int a, int b, int c, int d){
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
}

void vec4_rot1(vec_t v){
    int a = v[0];
    int b = v[1];
    int c = v[2];
    int d = v[3];
    v[0] = -d;
    v[1] = a;
    v[2] = b + d;
    v[3] = c;
}

void vec4_rot(vec_t v, rot_t r){
    /* Let's be saaafe */
    r = rot_contain(VEC4_ROT_MAX, r);
    for(int i = r; i > 0; i--)vec4_rot1(v);
}

void vec4_flip1(vec_t v){
    int a = v[0];
    int b = v[1];
    int c = v[2];
    int d = v[3];
    v[0] = a + c;
    v[2] = -c;
    v[3] = -b - d;
}

void vec4_flip(vec_t v, flip_t flip){
    if(flip)vec4_flip1(v);
}

void vec4_render(vec_t v, int *x, int *y){
    *x =  (v[0] + v[1]) * 2 + v[2];
    *y = -(v[3] + v[2]) * 2 - v[1];
}
