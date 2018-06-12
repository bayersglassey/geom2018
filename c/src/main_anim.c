
#include <stdio.h>
#include <stdbool.h>

#include "anim.h"


int mainloop(){
    const char *filename = "data/anim.fus";
    return 0;
}


int main(int n_args, char *args[]){
    int e = mainloop();
    fprintf(stderr, "Exiting with code: %i\n", e);
    return e;
}
