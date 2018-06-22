
#include <stdio.h>
#include <stdbool.h>

#include "anim.h"
#include "util.h"
#include "hexspace.h"


int mainloop(){
    int err;

    stateset_t stateset;
    const char *stateset_filename = "data/anim.fus";
    err = stateset_load(&stateset, strdup(stateset_filename), &hexspace);
    if(err)return err;

    stateset_dump(&stateset, stdout);
    stateset_cleanup(&stateset);

    hexcollmapset_t hexcollmapset;
    const char *hexcollmapset_filename = "data/map.fus";
    err = hexcollmapset_load(&hexcollmapset, hexcollmapset_filename);
    if(err)return err;

    hexcollmapset_dump(&hexcollmapset, stdout);
    hexcollmapset_cleanup(&hexcollmapset);

    return 0;
}


int main(int n_args, char *args[]){
    int e = mainloop();
    fprintf(stderr, "Exiting with code: %i\n", e);
    return e;
}
