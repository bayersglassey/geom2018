
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_utils.h"
#include "file_utils.h"
#include "hexspace.h"
#include "hexcollmap.h"


static hexcollmap_t *load_collmap(FILE *file, const char *filename){
    int err;

    char *buffer = read_stream(file, filename);
    if(!buffer){
        fprintf(stderr, "Couldn't read collmap stream: %s\n", filename);
        return NULL;
    }

    hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
    if(!collmap){
        perror("calloc collmap");
        return NULL;
    }

    err = hexcollmap_init(collmap, &hexspace, strdup(filename));
    if(err)return NULL;

    {
        fus_lexer_t lexer;
        int err = fus_lexer_init(&lexer, buffer, filename);
        if(err)return NULL;

        err = hexcollmap_parse(collmap, &lexer, false);
        if(err)return NULL;

        fus_lexer_cleanup(&lexer);
    }

    return collmap;
}


static void print_help(){
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    --just_coll  "
        "Only output the collmap's lines, no loading \"collmap:\" etc\n");
    fprintf(stderr, " -x --extra      "
        "Output extra info as fus comments (e.g. recordings, rendergraphs)\n");
    fprintf(stderr, " -h --help       "
        "Show this message\n");
}


int main(int n_args, char **args){
    bool just_coll = false;
    bool quiet = true;

    /* Parse args */
    for(int i = 1; i < n_args; i++){
        const char *arg = args[i];
        if(!strcmp(arg, "--just_coll")){
            just_coll = true;
        }else if(!strcmp(arg, "-x") || !strcmp(arg, "--extra")){
            quiet = false;
        }else if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            return 0;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            print_help();
            return 1;
        }
    }

    /* Load collmap */
    hexcollmap_t *collmap = load_collmap(stdin, "<stdin>");
    if(!collmap)return 2;

    /* Write collmap */
    hexcollmap_write(collmap, stdout, just_coll, quiet);

    return 0;
}

