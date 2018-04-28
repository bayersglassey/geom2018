
#include <stdio.h>
#include "vec4.h"



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



int main(int n_args, char *args[]){
    vec4_tests();
    return 0;
}
