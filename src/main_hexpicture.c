
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexpicture.h"


#define LINE_LEN 200
#define LINES_LEN 400

bool verbose = false;


int read_lines(FILE *file, char **lines, size_t *lines_len_ptr){
    int err;

    size_t lines_len = 0;
    while(1){
        char *line = lines[lines_len];
        if(!fgets(line, LINE_LEN, file)){
            if(ferror(file)){
                perror("fgets");
                return 1;
            }
            break;
        }
        line[strcspn(line, "\n")] = 0;
        lines_len++;
        if(lines_len > LINES_LEN){
            fprintf(stderr, "Max lines (%i) exceeded!\n", LINES_LEN);
            return 2;
        }
    }

    *lines_len_ptr = lines_len;
    return 0;
}

void print_help(){
    printf("Options:\n");
    printf("  -h --help   This help message\n");
    printf("  -v          Verbose mode\n");
}

int parse_args(int n_args, char **args, bool *early_exit_ptr){
    for(int i = 0; i < n_args; i++){
        char *arg = args[i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            *early_exit_ptr = true;
        }else if(!strcmp(arg, "-v")){
            verbose = true;
        }
    }
    return 0;
}


int main(int n_args, char **args){
    int err;

    bool early_exit = false;
    err = parse_args(n_args, args, &early_exit);
    if(err)return err;
    if(early_exit)return 0;

    char *lines[LINES_LEN];
    size_t lines_len = 0;

    for(int i = 0; i < LINES_LEN; i++){
        lines[i] = malloc(LINE_LEN);
        if(lines[i] == NULL){
            perror("malloc");
            return 1;
        }
    }

    err = read_lines(stdin, lines, &lines_len);
    if(err)return err;

    if(verbose){
        for(int i = 0; i < lines_len; i++){
            fprintf(stderr, "LINE %i: [%s]\n", i, lines[i]);
        }
    }

    hexpicture_t _pic = {0}, *pic = &_pic;
    hexpicture_init(pic);
    err = hexpicture_parse(pic,
        (const char **)lines, lines_len,
        verbose);
    if(err)return err;

    hexpicture_cleanup(pic);

    return 0;
}
