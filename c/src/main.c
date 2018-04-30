
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "util.h"
#include "prismelrenderer.h"
#include "vec4.h"


int main(int n_args, char *args[]){
    fus_lexer_t lexer;
    prismelrenderer_t prend;
    char *text = "1 2 (3 4) 5";
    int err;

    if(n_args >= 2){
        text = load_file(args[1]);
        if(text == NULL)return 1;
    }

    err = fus_lexer_init(&lexer, text);
    if(err)return err;

    err = prismelrenderer_init(&prend, &vec4);
    if(err)return err;

    err = prismelrenderer_parse(&prend, &lexer);
    if(err)return err;

    prismelrenderer_dump(&prend, stdout);

    return 0;
}
