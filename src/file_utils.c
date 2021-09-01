
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "file_utils.h"


#define CHUNK_SIZE ((size_t)(1024 * 64))



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
        perror("fopen");
        fprintf(stderr, "Could not open file: %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    f_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    f_buffer = calloc(f_size + 1, 1);
    if(f_buffer == NULL){
        perror("calloc");
        fprintf(stderr,
            "Could not allocate buffer for file: %s (%li bytes)\n",
            filename, f_size);
        fclose(f);
        return NULL;
    }
    n_read_bytes = fread(f_buffer, 1, f_size, f);
    if(n_read_bytes < f_size){
        perror("fread");
        fprintf(stderr,
            "Could not read (all of) file: %s (%li bytes)\n",
            filename, f_size);
        free(f_buffer);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return f_buffer;
}

char *read_stream(FILE *file, const char *filename){
    char *buffer = NULL;
    size_t bufsize = 0;

    while(1){
        bufsize += CHUNK_SIZE;
        char *new_buffer = realloc(buffer, bufsize);
        if(!new_buffer){
            perror("realloc");
            fprintf(stderr,
                "Could not allocate %zu-byte buffer for stream: %s\n",
                bufsize, filename);
            free(buffer);
            return NULL;
        }
        buffer = new_buffer;

        char *chunk = buffer + (bufsize - CHUNK_SIZE);

        size_t n_read_bytes = fread(chunk, 1, CHUNK_SIZE, file);
        if(n_read_bytes < CHUNK_SIZE){
            if(ferror(file)){
                free(buffer);
                perror("fread");
                fprintf(stderr,
                    "Failed read (%zu/%zu bytes) from stream: %s\n",
                    n_read_bytes, CHUNK_SIZE, filename);
                return NULL;
            }
            buffer[n_read_bytes] = '\0';
            break;
        }

    }

    return buffer;
}
