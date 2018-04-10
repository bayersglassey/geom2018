
/*


    We probably want to have a "declarative" structure which just describes
    patterns; and this can then be "compiled" into other structures
    specialized for e.g. rendering.

    union:
        sq
        add: A D union:
            sq
            rot: 2 tri
            rot: 4 sq
            rot: 7 tri

*/



#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define VEC_DIMS 4
#define MAX_ROT 12

int int_quo(int a, int b){
    div_t d = div(a, b);
    if(a<0 && d.rem != 0){
        d.quot += b<0? 1: -1;
    }
    return d.quot;
}

int int_rem(int a, int b){
    div_t d = div(a, b);
    if(a<0 && d.rem != 0){
        d.rem += b<0? -b: b;
    }
    return d.rem;
}

typedef int vec_t[VEC_DIMS];
typedef int rot_t;
typedef bool flip_t;
typedef struct trf {
    vec_t add;
    rot_t rot;
    flip_t flip;
} trf_t;

rot_t rot_bounded(rot_t r){
    return int_rem(r, MAX_ROT);
}

void vec_neg(vec_t v){
    v[0] = -v[0];
    v[1] = -v[1];
    v[2] = -v[2];
    v[3] = -v[3];
}

void vec_set(vec_t v, int a, int b, int c, int d){
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
}

void vec_fprintf(FILE *f, vec_t v){
    fprintf(f, "%i %i %i %i", v[0], v[1], v[2], v[3]);
}

void vec_printf(vec_t v){
    vec_fprintf(stdout, v);
}

bool vec_test(vec_t v, int a, int b, int c, int d){
    return
        v[0] == a &&
        v[1] == b &&
        v[2] == c &&
        v[3] == d
    ;
}

void vec_cpy(vec_t v, vec_t w){
    v[0] = w[0];
    v[1] = w[1];
    v[2] = w[2];
    v[3] = w[3];
}

void vec_add(vec_t v, vec_t w){
    v[0] += w[0];
    v[1] += w[1];
    v[2] += w[2];
    v[3] += w[3];
}

void vec_sub(vec_t v, vec_t w){
    v[0] -= w[0];
    v[1] -= w[1];
    v[2] -= w[2];
    v[3] -= w[3];
}

void vec_addn(vec_t v, vec_t w, int n){
    v[0] += w[0] * n;
    v[1] += w[1] * n;
    v[2] += w[2] * n;
    v[3] += w[3] * n;
}

bool vec_eq(vec_t v, vec_t w){
    return
        v[0] == w[0] &&
        v[1] == w[1] &&
        v[2] == w[2] &&
        v[3] == w[3]
    ;
}

void vec_nmul(vec_t v, int n){
    v[0] *= n;
    v[1] *= n;
    v[2] *= n;
    v[3] *= n;
}

void vec_rot(vec_t v, rot_t r){
    for(int i = rot_bounded(r); i > 0; i--){
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

void vec_flip(vec_t v, flip_t flip){
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

void vec_apply(vec_t v, trf_t *t){
    vec_flip(v, t->flip);
    vec_rot(v, t->rot);
    vec_add(v, t->add);
}

void vec_apply_inv(vec_t v, trf_t *t){
    vec_sub(v, t->add);
    vec_rot(v, -t->rot);
    vec_flip(v, !t->flip);
}

void vec_mul(vec_t v, vec_t w){
    vec_t v0;
    vec_cpy(v0, v);

    vec_nmul(v, w[0]);

    vec_rot(v0, 1);
    vec_addn(v, v0, w[1]);

    vec_rot(v0, 1);
    vec_addn(v, v0, w[2]);

    vec_rot(v0, 1);
    vec_addn(v, v0, w[3]);
}

void trf_inv(trf_t *t){
    flip_t f = !t->flip;
    rot_t r = -t->rot;
    vec_rot(t->add, r);
    vec_flip(t->add, f);
    vec_neg(t->add);
    t->flip = f;
    t->rot = f? -r: r;
}

void trf_apply(trf_t *t, trf_t *s){
    if(s->flip){
        t->flip ^= s->flip;
        t->rot = -t->rot;
        vec_flip(t->add, s->flip);
    }
    t->rot += s->rot;
    vec_rot(t->add, s->rot);
    vec_add(t->add, s->add);
}

void trf_apply_inv(trf_t *t, trf_t *s){
    vec_sub(t->add, s->add);
    vec_rot(t->add, -s->rot);
    t->rot -= s->rot;
    if(s->flip){
        t->flip ^= s->flip;
        t->rot = -t->rot;
        vec_flip(t->add, s->flip);
    }
}

bool test_vecs(vec_t v, vec_t w){
    bool ok = vec_eq(v, w);
    printf("("); vec_printf(v); printf(" == "); vec_printf(w); printf(")? %i\n", ok);
    return ok;
}

bool run_tests(){
    vec_t A, B, C, D;
    vec_set(A, 1, 0, 0, 0);
    vec_set(B, 0, 1, 0, 0);
    vec_set(C, 0, 0, 1, 0);
    vec_set(D, 0, 0, 0, 1);

    {
        vec_t v;
        vec_cpy(v, A);
        vec_rot(v, 1);
        test_vecs(v, B);
    }

    {
        vec_t v;
        vec_cpy(v, B);
        vec_rot(v, -1);
        test_vecs(v, A);
    }

    {
        vec_t v;
        vec_set(v, -1, 0, 1, 0);
        vec_rot(v, -1);
        test_vecs(v, D);
    }

    return true;
}

int main(int n_args, char *args[]){
    run_tests();
    return 0;
}
