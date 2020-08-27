
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "prismelrenderer.h"
#include "vec4.h"



void print_help(){
    fprintf(stderr, "Usage: prendtool FILENAME [OPTIONS...]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h|--help      This message\n");
}


static int main2(prismelrenderer_t *prend){
    prismelrenderer_dump(prend, stderr, 0);
    fprintf(stderr, "--------------------------\n");
    fprintf(stderr, "-- STATS\n");
    prismelrenderer_dump_stats(prend, stderr);
    return 0;
}


int main(int n_args, char *args[]){
    if(n_args < 2){
        print_help();
        return 2;
    }

    const char *prend_filename = args[1];

    for(int arg_i = 2; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            return 0;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            return 2;
        }
    }

    {
        int err;

        prismelrenderer_t prend;

        err = prismelrenderer_init(&prend, &vec4);
        if(err)return err;

        err = prismelrenderer_load(&prend, prend_filename, NULL);
        if(err)return err;

        err = main2(&prend);
        if(err)return err;

        prismelrenderer_cleanup(&prend);
    }

    return 0;
}
