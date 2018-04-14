
/*

    Can we derive vec_rot, vec_flip for a given space from a set of
    equations in its dims?

    Or like -- R(A) is always B, R(B) is always C, and all we need to specify
    is R(D) because that's the last dim.

    It should also be possible to derive flip given the definition of flip(B)
    a.k.a. F(R(A)).

    Hmmmm.

*/



#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_VEC_DIMS 4

typedef int vec_t[MAX_VEC_DIMS];
typedef int boundbox_t[MAX_VEC_DIMS*2];
typedef int rot_t;
typedef bool flip_t;
typedef struct trf {
    /* a transformation -- applied in this order: flip, rot, add */
    flip_t flip;
    rot_t rot;
    vec_t add;
} trf_t;


typedef struct vecspace {
    int dims;
    int rot_max;

    void (*vec_rot)(vec_t v, rot_t r);
    void (*vec_flip)(vec_t v, flip_t flip);
} vecspace_t;




#define rot_inv(rot_max, rot) ((rot_max) - (rot))
#define rot_flip(rot_max, rot, flip) ((flip)? rot_inv((rot_max), (rot)): (rot))
#define flip_inv(flip) (!(flip))

rot_t rot_rot(int rot_max, rot_t r1, rot_t r2){
    rot_t r3 = r1 + r2;
    return r3 > rot_max? rot_max - r3: r3;
}


void vec_zero(int dims, vec_t v){
    for(int i = dims - 1; i >= 0; i--){
        v[i] = 0;
    }
}

void vec_neg(int dims, vec_t v){
    for(int i = dims - 1; i >= 0; i--){
        v[i] = -v[i];
    }
}

void vec_fprintf(FILE *f, int dims, vec_t v){
    fprintf(f, "%i", v[0]);
    for(int i = 1; i < dims; i++){
        fprintf(f, " %i", v[i]);
    }
}

void vec_printf(int dims, vec_t v){
    vec_fprintf(stdout, dims, v);
}

void vec_cpy(int dims, vec_t v, vec_t w){
    for(int i = dims - 1; i >= 0; i--){
        v[i] = w[i];
    }
}

void vec_add(int dims, vec_t v, vec_t w){
    for(int i = dims - 1; i >= 0; i--){
        v[i] += w[i];
    }
}

void vec_sub(int dims, vec_t v, vec_t w){
    for(int i = dims - 1; i >= 0; i--){
        v[i] -= w[i];
    }
}

void vec_addn(int dims, vec_t v, vec_t w, int n){
    for(int i = dims - 1; i >= 0; i--){
        v[i] += w[i] * n;
    }
}

bool vec_eq(int dims, vec_t v, vec_t w){
    for(int i = dims - 1; i >= 0; i--){
        if(v[i] != w[i])return false;
    }
    return true;
}

void vec_nmul(int dims, vec_t v, int n){
    for(int i = dims - 1; i >= 0; i--){
        v[i] *= n;
    }
}


void vec_apply(vecspace_t *space, vec_t v, trf_t *t){
    space->vec_flip(v, t->flip);
    space->vec_rot(v, t->rot);
    vec_add(space->dims, v, t->add);
}

void vec_apply_inv(vecspace_t *space, vec_t v, trf_t *t){
    vec_sub(space->dims, v, t->add);
    space->vec_rot(v, rot_inv(space->rot_max, t->rot));
    space->vec_flip(v, flip_inv(t->flip));
}

void vec_mul(vecspace_t *space, vec_t v, vec_t w){
    int dims = space->dims;

    vec_t v0;
    vec_cpy(dims, v0, v);

    vec_nmul(dims, v, w[0]);

    for(int i = 1; i < dims; i++){
        space->vec_rot(v0, 1);
        vec_addn(dims, v, v0, w[i]);
    }
}

bool test_vecs(int dims, vec_t v, vec_t w){
    bool ok = vec_eq(dims, v, w);
    printf("("); vec_printf(dims, v); printf(" == "); vec_printf(dims, w); printf(")? %i\n", ok);
    return ok;
}




void trf_inv(vecspace_t *space, trf_t *t){
    flip_t f = flip_inv(t->flip);
    rot_t r = rot_inv(space->rot_max, t->rot);
    space->vec_rot(t->add, r);
    space->vec_flip(t->add, f);
    vec_neg(space->dims, t->add);
    t->flip = f;
    t->rot = rot_flip(space->rot_max, f, r);
}

void trf_apply(vecspace_t *space, trf_t *t, trf_t *s){
    if(s->flip){
        t->flip = flip_inv(t->flip);
        t->rot = rot_inv(space->rot_max, t->rot);
        space->vec_flip(t->add, s->flip);
    }
    t->rot = rot_rot(space->rot_max, t->rot, s->rot);
    space->vec_rot(t->add, s->rot);
    vec_add(space->dims, t->add, s->add);
}

void trf_apply_inv(vecspace_t *space, trf_t *t, trf_t *s){
    vec_sub(space->dims, t->add, s->add);
    rot_t r = rot_inv(space->rot_max, s->rot);
    space->vec_rot(t->add, r);
    t->rot = rot_rot(space->rot_max, t->rot, r);
    if(s->flip){
        t->flip = flip_inv(t->flip);
        t->rot = rot_inv(space->rot_max, t->rot);
        space->vec_flip(t->add, s->flip);
    }
}






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
