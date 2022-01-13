
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../prismelrenderer.h"
#include "../geomfont.h"
#include "../directory.h"
#include "../directory_shell.h"
#include "../vec4.h"


const char *DEFAULT_PREND_FILENAME = "data/test.fus";


static int _max(int a, int b){return a > b? a: b;}
static int _min(int a, int b){return a < b? a: b;}


#define DECL_DIRECTORY_ENTRY_LIST_CLASS(SUFFIX) \
static void prend_list##SUFFIX( \
    directory_entry_t *root, struct directory_list *list, int page); \
directory_entry_class_t prend_class##SUFFIX = { \
    .list = &prend_list##SUFFIX};

/* NOTE: CURRENTLY UNUSED... */
#define DECL_DIRECTORY_ENTRY_CAT_CLASS(SUFFIX) \
static void prend_cat##SUFFIX( \
    directory_entry_t *entry, FILE *file); \
directory_entry_class_t prend_class##SUFFIX = { \
    .cat = &prend_cat##SUFFIX};

DECL_DIRECTORY_ENTRY_LIST_CLASS()
DECL_DIRECTORY_ENTRY_LIST_CLASS(_fonts)
DECL_DIRECTORY_ENTRY_LIST_CLASS(_palmappers)
DECL_DIRECTORY_ENTRY_LIST_CLASS(_prismels)
DECL_DIRECTORY_ENTRY_LIST_CLASS(_shapes)
DECL_DIRECTORY_ENTRY_LIST_CLASS(_mappers)
DECL_DIRECTORY_ENTRY_LIST_CLASS(_geomfonts)

#define SET_PREND_LIST_ENTRY(ENTRY, NAME, SELF) \
    directory_entry_init(&(ENTRY), &prend_class_##NAME, #NAME, SELF);

#define GET_ENTRIES_LEN(TOTAL_LEN, PAGE) \
    _max(0, _min(DIRECTORY_LIST_ENTRIES, \
        (TOTAL_LEN) - (PAGE) * DIRECTORY_LIST_ENTRIES))

#define GET_ENTRY_I(ENTRY_I, PAGE) \
    ((PAGE) * DIRECTORY_LIST_ENTRIES + (ENTRY_I))



static void prend_list(directory_entry_t *root, struct directory_list *list, int page){
    prismelrenderer_t *prend = root->self;

    if(page > 0){
        list->entries_len = 0;
        return;
    }

    SET_PREND_LIST_ENTRY(list->entries[0], fonts, root->self)
    SET_PREND_LIST_ENTRY(list->entries[1], palmappers, root->self)
    SET_PREND_LIST_ENTRY(list->entries[2], prismels, root->self)
    SET_PREND_LIST_ENTRY(list->entries[3], shapes, root->self)
    SET_PREND_LIST_ENTRY(list->entries[4], mappers, root->self)
    SET_PREND_LIST_ENTRY(list->entries[5], geomfonts, root->self)
    list->entries_len = 6;
}

static void prend_list_fonts(directory_entry_t *root, struct directory_list *list, int page){
    //ARRAY_DECL(struct font*, fonts)
    prismelrenderer_t *prend = root->self;
    list->entries_len = GET_ENTRIES_LEN(prend->fonts_len, page);
    for(int i = 0; i < list->entries_len; i++){
        font_t *font = prend->fonts[GET_ENTRY_I(i, page)];
        directory_entry_init(&list->entries[i],
            &directory_entry_class_dummy, font->filename, font);
    }
}

static void prend_list_palmappers(directory_entry_t *root, struct directory_list *list, int page){
    //ARRAY_DECL(struct palettemapper*, palmappers)
    prismelrenderer_t *prend = root->self;
    list->entries_len = GET_ENTRIES_LEN(prend->palmappers_len, page);
    for(int i = 0; i < list->entries_len; i++){
        palettemapper_t *palmapper = prend->palmappers[GET_ENTRY_I(i, page)];
        directory_entry_init(&list->entries[i],
            &directory_entry_class_dummy, palmapper->name, palmapper);
    }
}

static void prend_list_prismels(directory_entry_t *root, struct directory_list *list, int page){
    //ARRAY_DECL(struct prismel*, prismels)
    prismelrenderer_t *prend = root->self;
    list->entries_len = GET_ENTRIES_LEN(prend->prismels_len, page);
    for(int i = 0; i < list->entries_len; i++){
        prismel_t *prismel = prend->prismels[GET_ENTRY_I(i, page)];
        directory_entry_init(&list->entries[i],
            &directory_entry_class_dummy, prismel->name, prismel);
    }
}

static void prend_list_shapes(directory_entry_t *root, struct directory_list *list, int page){
    //ARRAY_DECL(struct rendergraph*, rendergraphs)
    prismelrenderer_t *prend = root->self;
    list->entries_len = GET_ENTRIES_LEN(prend->rendergraphs_len, page);
    for(int i = 0; i < list->entries_len; i++){
        rendergraph_t *rendergraph = prend->rendergraphs[GET_ENTRY_I(i, page)];
        directory_entry_init(&list->entries[i],
            &directory_entry_class_dummy, rendergraph->name, rendergraph);
    }
}

static void prend_list_mappers(directory_entry_t *root, struct directory_list *list, int page){
    //ARRAY_DECL(struct prismelmapper*, mappers)
    prismelrenderer_t *prend = root->self;
    list->entries_len = GET_ENTRIES_LEN(prend->mappers_len, page);
    for(int i = 0; i < list->entries_len; i++){
        prismelmapper_t *mapper = prend->mappers[GET_ENTRY_I(i, page)];
        directory_entry_init(&list->entries[i],
            &directory_entry_class_dummy, mapper->name, mapper);
    }
}

static void prend_list_geomfonts(directory_entry_t *root, struct directory_list *list, int page){
    //ARRAY_DECL(struct geomfont*, geomfonts)
    prismelrenderer_t *prend = root->self;
    list->entries_len = GET_ENTRIES_LEN(prend->geomfonts_len, page);
    for(int i = 0; i < list->entries_len; i++){
        geomfont_t *geomfont = prend->geomfonts[GET_ENTRY_I(i, page)];
        directory_entry_init(&list->entries[i],
            &directory_entry_class_dummy, geomfont->name, geomfont);
    }
}



static int mainloop(prismelrenderer_t *prend, const char *prend_filename){
    int err;

    directory_entry_t _root = {
        .name = prend_filename,
        .class = &prend_class,
        .self = prend,
    }, *root=&_root;

    directory_shell_t _shell = {
        .root = root,
    }, *shell=&_shell;

    while(1){
        err = directory_shell_get_line(shell);
        if(err)return err;

        if(!strcmp(shell->line, "exit")){
            break;
        }else if(!strcmp(shell->line, "dump")){
            prismelrenderer_dump(prend, stdout, 0);
        }else if(!strcmp(shell->line, "stats")){
            prismelrenderer_dump_stats(prend, stdout);
        }else{
            err = directory_shell_process_line(shell);
            if(err)return err;
        }
    }
    return 0;
}




static void print_help(){
    fprintf(stderr,
        "Options:\n");
    fprintf(stderr,
        "  -h|--help      This message\n");
    fprintf(stderr,
        "  -f             Load prismelrenderer from file (default: %s)\n",
        DEFAULT_PREND_FILENAME);
}

int main(int n_args, char *args[]){

    const char *prend_filename = DEFAULT_PREND_FILENAME;

    for(int arg_i = 1; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            return 0;
        }else if(!strcmp(arg, "-f")){
            if(++arg_i >= n_args){
                fprintf(stderr, "Missing value for option: %s\n", arg);
                return 2;
            }
            prend_filename = args[arg_i];
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

        err = mainloop(&prend, prend_filename);
        if(err)return err;

        prismelrenderer_cleanup(&prend);
    }

    return 0;
}
