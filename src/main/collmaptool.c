
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../str_utils.h"
#include "../file_utils.h"
#include "../stringstore.h"
#include "../hexspace.h"
#include "../hexcollmap.h"
#include "../mapeditor.h"



static hexcollmap_t *load_collmap(FILE *file, const char *filename,
    bool just_coll,
    hexcollmap_part_t ***parts_ptr, int *parts_len_ptr,
    stringstore_t *name_store, stringstore_t *filename_store
){
    int err;

    /* NOTE: this function is basically like hexcollmap_load, except that it
    dynamically allocates a hexcollmap, and accepts a FILE* instead of a
    filename (so that e.g. we can accept stdin as our input file).
    We probably allocate a hexcollmap so that caller can easily replace
    it with a rotated version (by allocating a rotated clone, then freeing
    the original). */

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

    {
        fus_lexer_t lexer;
        int err = fus_lexer_init(&lexer, buffer, filename);
        if(err)return NULL;

        err = hexcollmap_parse_with_parts(collmap, &lexer,
            &hexspace, filename, just_coll,
            parts_ptr, parts_len_ptr,
            name_store, filename_store);
        if(err)return NULL;

        fus_lexer_cleanup(&lexer);
    }

    free(buffer);
    return collmap;
}


static void print_help(){
    fprintf(stderr, "Usage: collmaptool [OPTION ...] [--] [FILENAME]\n");
    fprintf(stderr, "If FILENAME is missing or \"-\", stdin is used.\n");
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
    fprintf(stderr, " -E --editor         "
        "Start interactive editor\n");
    fprintf(stderr, " -h --help           "
        "Show this message\n");
}


int main(int n_args, char **args){
    hexcollmap_write_options_t opts = {0};
    bool dump = false;
    rot_t rot = -1;
    bool use_editor = false;

    /* Parse args */
    int arg_i = 1;
    for(; arg_i < n_args; arg_i++){
        const char *arg = args[arg_i];
        if(!strcmp(arg, "--just_coll")){
            opts.just_coll = true;
        }else if(!strcmp(arg, "--dump")){
            dump = true;
        }else if(!strcmp(arg, "-x") || !strcmp(arg, "--extra")){
            opts.extra = true;
        }else if(!strcmp(arg, "-d") || !strcmp(arg, "--nodots")){
            opts.nodots = true;
        }else if(!strcmp(arg, "-t") || !strcmp(arg, "--show_tiles")){
            opts.show_tiles = true;
        }else if(!strcmp(arg, "-e") || !strcmp(arg, "--eol_semicolons")){
            opts.eol_semicolons = true;
        }else if(!strcmp(arg, "-r") || !strcmp(arg, "--rot")){
            arg_i++;
            if(arg_i >= n_args)goto arg_missing_value;
            rot = rot_contain(HEXSPACE_ROT_MAX, atoi(args[arg_i]));
        }else if(!strcmp(arg, "-E") || !strcmp(arg, "--editor")){
            use_editor = true;
        }else if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            return 0;
        }else if(!strcmp(arg, "--")){
            arg_i++;
            break;
        }else if(arg[0] == '-' && arg[1] != '\0'){
            fprintf(stderr, "ERROR: Unrecognized option: %s\n", arg);
            print_help();
            return 1;
        }else{
            /* Hopefully the filename... */
            break;
        }

        continue;
        arg_missing_value:
        fprintf(stderr, "ERROR: Missing value for option: %s\n", arg);
        print_help();
        return 2;
    }

    const char *collmap_filename = "-";
    if(arg_i < n_args - 1){
        fprintf(stderr, "ERROR: extra arguments after filename\n");
        print_help();
        return 2;
    }else if(arg_i == n_args - 1){
        collmap_filename = args[arg_i];
    }

    stringstore_t name_store;
    stringstore_t filename_store;
    stringstore_init(&name_store);
    stringstore_init(&filename_store);

    hexcollmap_part_t **parts;
    int parts_len;

    /* Load collmap */
    hexcollmap_t *collmap;
    {
        FILE *collmap_file = stdin;
        if(!strcmp(collmap_filename, "-")){
            if(use_editor){
                fprintf(stderr, "Interactive editor requires a filename.\n");
                return 1;
            }
            collmap_filename = "<stdin>";
        }else{
            collmap_file = fopen(collmap_filename, "r");
            if(!collmap_file){
                perror("fopen");
                return 1;
            }
        }

        collmap = load_collmap(collmap_file,
            collmap_filename, opts.just_coll,
            &parts, &parts_len,
            &name_store, &filename_store);
        if(!collmap)return 2;

        if(collmap_file != stdin){
            if(fclose(collmap_file) == EOF){
                perror("fclose");
                return 1;
            }
        }
    }

    /* Transform collmap */
    if(rot != -1){
        hexcollmap_t *from_collmap = collmap;
        collmap = calloc(1, sizeof(*collmap));
        if(!collmap){
            perror("calloc collmap");
            return 1;
        }
        hexcollmap_init_clone(collmap, from_collmap, from_collmap->filename);
        int err = hexcollmap_clone(collmap, from_collmap, rot);
        if(err)return err;

        hexcollmap_cleanup(from_collmap);
        free(from_collmap);
    }

    /* Write collmap */
    if(use_editor){
        int err = mapeditor(collmap_filename, &opts, collmap,
            parts, parts_len);
        if(err)return err;
    }else if(dump){
        hexcollmap_dump(collmap, stdout);
    }else{
        hexcollmap_write_with_parts(collmap, stdout, &opts,
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

    stringstore_cleanup(&name_store);
    stringstore_cleanup(&filename_store);

    return 0;
}

