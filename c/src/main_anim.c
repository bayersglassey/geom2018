
#include <stdio.h>
#include <stdbool.h>

#include "anim.h"


int mainloop(){
    int err;
    stateset_t stateset;
    const char *filename = "data/anim.fus";

    err = stateset_load(&stateset, filename);
    if(err)return err;

    stateset_dump(&stateset, stdout);

    return 0;
}


int main(int n_args, char *args[]){
    int e = mainloop();
    fprintf(stderr, "Exiting with code: %i\n", e);
    return e;
}
