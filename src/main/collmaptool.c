
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../str_utils.h"
#include "../file_utils.h"
#include "../hexspace.h"
#include "../hexcollmap.h"


static hexcollmap_t *load_collmap(FILE *file, const char *filename,
    bool just_coll,
    hexcollmap_part_t ***parts_ptr, int *parts_len_ptr
){
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

    hexcollmap_init(collmap, &hexspace, strdup(filename));

    {
        fus_lexer_t lexer;
        int err = fus_lexer_init(&lexer, buffer, filename);
        if(err)return NULL;

        err = hexcollmap_parse_with_parts(collmap, &lexer, just_coll,
            parts_ptr, parts_len_ptr);
        if(err)return NULL;

        fus_lexer_cleanup(&lexer);
    }

    free(buffer);
    return collmap;
}


static void print_help(){
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    --just_coll      "
        "Only parse/output the collmap's lines, no leading \"collmap:\" etc\n");
    fprintf(stderr, "    --dump           "
        "Raw dump instead of pretty write\n");
    fprintf(stderr, " -x --extra          "
        "Output extra info as fus comments (e.g. recordings, rendergraphs)\n");
    fprintf(stderr, " -d --nodots         "
        "Invisible verts are displayed as ' ' not '.'\n");
    fprintf(stderr, " -t --show_tiles     "
        "Tiles are shown\n");
    fprintf(stderr, " -e --eol_semicolons "
        "Semicolons written to end of each line marking end of tile data\n");
    fprintf(stderr, " -r --rot ROT        "
        "Rotate collmap by ROT (an integer modulo 6)\n");
    fprintf(stderr, " -h --help           "
        "Show this message\n");
}


int main(int n_args, char **args){
    bool just_coll = false;
    bool dump = false;
    bool extra = false;
    bool nodots = false;
    bool show_tiles = false;
    bool eol_semicolons = false;
    rot_t rot = -1;

    /* Parse args */
    for(int i = 1; i < n_args; i++){
        const char *arg = args[i];
        if(!strcmp(arg, "--just_coll")){
            just_coll = true;
        }else if(!strcmp(arg, "--dump")){
            dump = true;
        }else if(!strcmp(arg, "-x") || !strcmp(arg, "--extra")){
            extra = true;
        }else if(!strcmp(arg, "-d") || !strcmp(arg, "--nodots")){
            nodots = true;
        }else if(!strcmp(arg, "-t") || !strcmp(arg, "--show_tiles")){
            show_tiles = true;
        }else if(!strcmp(arg, "-e") || !strcmp(arg, "--eol_semicolons")){
            eol_semicolons = true;
        }else if(!strcmp(arg, "-r") || !strcmp(arg, "--rot")){
            i++;
            if(i >= n_args)goto arg_missing_value;
            rot = rot_contain(HEXSPACE_ROT_MAX, atoi(args[i]));
        }else if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            return 0;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            print_help();
            return 1;
        }

        continue;
        arg_missing_value:
        fprintf(stderr, "Missing value for option: %s\n", arg);
        print_help();
        return 2;
    }

    hexcollmap_part_t **parts;
    int parts_len;

    /* Load collmap */
    hexcollmap_t *collmap = load_collmap(stdin, "<stdin>", just_coll,
        &parts, &parts_len);
    if(!collmap)return 2;

    /* Transform collmap */
    if(rot != -1){
        hexcollmap_t *from_collmap = collmap;
        collmap = calloc(1, sizeof(*collmap));
        if(!collmap){
            perror("calloc collmap");
            return 1;
        }
        hexcollmap_init_clone(collmap, from_collmap, strdup("<clone>"));
        int err = hexcollmap_clone(collmap, from_collmap, rot);
        if(err)return err;

        hexcollmap_cleanup(from_collmap);
        free(from_collmap);
    }

    /* Write collmap */
    if(dump){
        hexcollmap_dump(collmap, stdout);
    }else{
        hexcollmap_write_with_parts(collmap, stdout,
            just_coll, extra, nodots, show_tiles, eol_semicolons,
            parts, parts_len);
    }

    /* Cleanup */
    for(int i = 0; i < parts_len; i++){
        hexcollmap_part_cleanup(parts[i]);
        free(parts[i]);
    }
    free(parts);
    hexcollmap_cleanup(collmap);
    free(collmap);

    return 0;
}

