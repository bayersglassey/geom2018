
#include "geom.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>




void vec4_set(vec_t v, int a, int b, int c, int d){
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
}

bool vec4_test(vec_t v, int a, int b, int c, int d){
    return
        v[0] == a &&
        v[1] == b &&
        v[2] == c &&
        v[3] == d
    ;
}

void vec4_rot(vec_t v, rot_t r){
    for(int i = r; i > 0; i--){
        int a = v[0];
        int b = v[1];
        int c = v[2];
        int d = v[3];
        v[0] = -d;
        v[1] = a;
        v[2] = b + d;
        v[3] = c;
    }
}

void vec4_flip(vec_t v, flip_t flip){
    if(flip){
        int a = v[0];
        int b = v[1];
        int c = v[2];
        int d = v[3];
        v[0] = a + c;
        v[2] = -c;
        v[3] = -b - d;
    }
}

vecspace_t vec4 = {4, 12, vec4_rot, vec4_flip};

bool vec4_tests(){
    vec_t A, B, C, D;
    vec4_set(A, 1, 0, 0, 0);
    vec4_set(B, 0, 1, 0, 0);
    vec4_set(C, 0, 0, 1, 0);
    vec4_set(D, 0, 0, 0, 1);

    {
        vec_t v;
        vec_cpy(4, v, A);
        vec4_rot(v, 1);
        test_vecs(4, v, B);
    }

    {
        vec_t v;
        vec_cpy(4, v, B);
        vec4_rot(v, vec4.rot_max-1);
        test_vecs(4, v, A);
    }

    {
        vec_t v;
        vec4_set(v, -1, 0, 1, 0);
        vec4_rot(v, vec4.rot_max-1);
        test_vecs(4, v, D);
    }

    return true;
}

void press_a_key(){
    printf("Hit enter...");
    getchar();
}

int main(int n_args, char *args[]){
    vec4_tests();
    press_a_key();
    return 0;
}
