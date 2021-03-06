
#include <stdio.h>
#include <string.h>

#include "write.h"


static void _fus_putc(FILE *f, char c){
    if(c == '\n'){
        fputs("\\n", f);
    }else if(c == '\"' || c == '\\'){
        fputc('\\', f);
        fputc(c, f);
    }else{
        fputc(c, f);
    }
}

void fus_nwrite_str(FILE *f, const char *s, int n){
    /* Writes given string s to file f, up to a max of n chars */
    fputc('\"', f);
    char c;
    int i = 0;
    while(i < n && (c = *s, c != '\0')){
        _fus_putc(f, c);
        s++;
        i++;
    }
    fputc('\"', f);
}

void fus_write_str(FILE *f, const char *s){
    /* Writes given string s to file f */
    fputc('\"', f);
    char c;
    while(c = *s, c != '\0'){
        _fus_putc(f, c);
        s++;
    }
    fputc('\"', f);
}

void fus_write_str_padded(FILE *f, const char *s, int w){
    /* Writes given string s to file f, left-padded with spaces so number of
    characters ultimately written to f is at least w */
    int s_len = strlen(s);
    if(s_len + 2 < w){
        for(int i = 0; i < w - s_len - 2; i++)putc(' ', f);
    }
    fus_write_str(f, s);
}

