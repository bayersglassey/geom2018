
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "file_utils.h"


int getln(char buf[], int buf_len, FILE *file){
    if(!fgets(buf, buf_len, file)){
        perror("getln: fgets");
        return 1;
    }
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

char *load_file(const char *filename){
    FILE *f = fopen(filename, "r");
    long f_size;
    char *f_buffer;
    size_t n_read_bytes;
    if(f == NULL){
        fprintf(stderr, "Could not open file: %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    f_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    f_buffer = calloc(f_size + 1, 1);
    if(f_buffer == NULL){
        fprintf(stderr, "Could not allocate buffer for file: %s (%li bytes)\n", filename, f_size);
        fclose(f);
        return NULL;
    }
    n_read_bytes = fread(f_buffer, 1, f_size, f);
    fclose(f);
    return f_buffer;
}
