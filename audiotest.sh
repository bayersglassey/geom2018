#!/bin/bash
set -euo pipefail

cfile=~/.music.c
binfile=~/.music

echo "
#include \"stdio.h\"
#include \"stdlib.h\"
typedef unsigned int I;
int getint(const char *name, int default_value){
    const char *value = getenv(name);
    return value? atoi(value): default_value;
}
int main(){
    int quiet = getenv(\"QUIET\")? 1: 0;
    int skip = getint(\"SKIP\", 32);
    for(I t=0;;t++){
        I a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,u,v,w,x,y,z;
        unsigned char _c = (c=0, ($1));
        unsigned char _c2 = (c=1, ($1));
        putchar(_c);
        putchar(_c2);
        if (quiet) continue;
        if (t % skip) continue;
        fprintf(stderr, \"%3i \", _c);
        for (int i = 0; i < _c/2; i++) fputs(\"█\", stderr);
        if (_c%2) fputs(\"▌\", stderr);
        fputc('\n', stderr);
    }
    return 0;
}" >"$cfile" &&
gcc "$cfile" -o "$binfile" &&
"$binfile" | aplay -t raw -c "${CHANNELS:-2}" -f "${FORMAT:-U8}" -r "${RATE:-8000}"
