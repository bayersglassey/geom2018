
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "../util.h"
#include "../vec4.h"
#include "../minieditor.h"
#include "../defaults.h"


#define DEFAULT_IMAGE_FILENAME "screen.bmp"

#define DEFAULT_SCW 1024
#define DEFAULT_SCH 768

/* How many milliseconds we want each frame to last */
#define DEFAULT_DELAY_GOAL 30

#define ACTION_DEFAULT          0
#define ACTION_SCREENSHOT       1
#define ACTION_DUMP             2
#define ACTION_DUMP_MAPPER      3
#define ACTION_DUMP_PALMAPPER   4
#define ACTION_LIST             5
#define ACTION_LIST_MAPPERS     6
#define ACTION_LIST_PALMAPPERS  7


typedef struct label_mapping_def {
    const char *label_name;
    const char *rgraph_name;
} label_mapping_def_t;

typedef struct options {
    bool quiet;
    Uint32 window_flags;
    const char *prend_filename;
    const char *rgraph_name;
    const char *image_filename;
    const char *palette_filename;
    const char *mapper_name;
    const char *palmapper_name;
    const char *font_filename;
    const char *fonts_filename;
    int zoom;
    int frame_i;
    rot_t rot;
    flip_t flip;
    bool show_editor_controls;
    bool cache_bitmaps;
    int delay_goal;
    int scw;
    int sch;
    int x0;
    int y0;
    int dump_bitmaps;
    int action; /* See ACTION_* */
    ARRAY_DECL(label_mapping_def_t*, label_mappings)
} options_t;

static void options_cleanup(options_t *opts){
    ARRAY_FREE_PTR(label_mapping_def_t*, opts->label_mappings, (void))
}

static void options_init(options_t *opts){
    opts->window_flags = SDL_WINDOW_SHOWN;
    opts->prend_filename = DEFAULT_PREND_FILENAME;
    opts->palette_filename = DEFAULT_PALETTE_FILENAME;
    opts->mapper_name = NULL;
    opts->palmapper_name = NULL;
    opts->font_filename = DEFAULT_FONT_FILENAME;
    opts->fonts_filename = DEFAULT_FONTS_FILENAME;
    opts->image_filename = DEFAULT_IMAGE_FILENAME;
    opts->zoom = 1;
    opts->show_editor_controls = true;
    opts->cache_bitmaps = true;
    opts->delay_goal = DEFAULT_DELAY_GOAL;
    opts->scw = DEFAULT_SCW;
    opts->sch = DEFAULT_SCH;
    ARRAY_INIT(opts->label_mappings)
}


static void print_help(FILE *file){
    fprintf(file,
        "Options:\n"
        "  -h | --help      Print this message and exit\n"
        "  -F               Fullscreen\n"
        "  -FD              Fullscreen Desktop\n"
        "  -q               Quiet mode (less output to stderr)\n"
        "  -f  FILENAME     Load prend data (default: " DEFAULT_PREND_FILENAME ")\n"
        "  -if FILENAME     Where to save screenshot (default: " DEFAULT_IMAGE_FILENAME ")\n"
        "  -n  NAME         Load rgraph\n"
        "  -i  FRAME        Set frame_i\n"
        "  -z  ZOOM         Set zoom (1 - %i)\n"
        "  -r  ROT          Set rotation\n"
        "  -t  FLIP         Set flip\n"
        "  -l  LABEL RGRAPH Set label (to rgraph name)\n"
        "  --delay TICKS    Set delay goal (default: %i)\n"
        "  -s SCW SCH       Set screen width and height (default: %i %i)\n"
        "  -p X0 Y0         Set screen position\n"
        "  --pal   FILENAME Load palette (default: " DEFAULT_PALETTE_FILENAME ")\n"
        "  --font  FILENAME Load font (default: " DEFAULT_FONT_FILENAME ")\n"
        "  --fonts FILENAME Load fonts (default: " DEFAULT_FONTS_FILENAME ")\n"
        "  --mapper    NAME Applies the given prismel mapper\n"
        "  --palmapper NAME Applies the given palette mapper\n"
        "                   (NOTE: palmapper is applied after mapper)\n"
        "  --nocontrols     Don't show controls initially\n"
        "  --dump-bitmaps   Whether to dump rendergraph's bitmaps (see --dump)\n"
        "                     0: don't dump bitmaps\n"
        "                     1: dump bitmaps\n"
        "                     2: also dump their surfaces\n"
        "  --dont-cache-bitmaps\n"
        "                   Don't cache bitmaps, i.e. re-render rendergraphs\n"
        "                   from scratch every frame\n"
        "Commands which cause the program to run without a GUI window:\n"
        "  screenshot       Save screenshot and exit (see -if)\n"
        "  dump             Dump rgraph's details to stdout and exit\n"
        "                   (see -n, --dump_bitmaps)\n"
        "  dump_mapper      Dump mapper's details to stdout and exit\n"
        "                   (see --mapper)\n"
        "  dump_palmapper      Dump palmapper's details to stdout and exit\n"
        "                   (see --palmapper)\n"
        "  list             Lists prismelrenderer's rgraphs and exit\n"
        "  list_mappers     Lists prismelrenderer's prismelmappers and exit\n"
        "  list_palmappers  Lists prismelrenderer's palettemappers and exit\n"
    , MINIEDITOR_MAX_ZOOM, DEFAULT_DELAY_GOAL, DEFAULT_SCW, DEFAULT_SCH);
}


static int parse_options(options_t *opts,
    int n_args, const char **args, bool *quit
){
    for(int arg_i = 1; arg_i < n_args; arg_i++){
        const char *arg = args[arg_i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help(stdout);
            *quit = true;
            return 0;
        }else if(!strcmp(arg, "-F")){
            opts->window_flags |= SDL_WINDOW_FULLSCREEN;
        }else if(!strcmp(arg, "-FD")){
            opts->window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }else if(!strcmp(arg, "-q")){
            opts->quiet = true;
        }else if(!strcmp(arg, "-f")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            opts->prend_filename = args[arg_i];
        }else if(!strcmp(arg, "-if")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            opts->image_filename = args[arg_i];
        }else if(!strcmp(arg, "-n")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing rgraph name after %s\n", arg);
                return 2;}
            opts->rgraph_name = args[arg_i];
        }else if(!strcmp(arg, "-i")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            opts->frame_i = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-z")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            int zoom = atoi(args[arg_i]);
            if(zoom < 1)zoom = 1;
            if(zoom > MINIEDITOR_MAX_ZOOM)zoom = MINIEDITOR_MAX_ZOOM;
            opts->zoom = zoom;
        }else if(!strcmp(arg, "-r")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            opts->rot = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-t")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            opts->flip = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-l")){
            arg_i += 2;
            if(arg_i >= n_args + 1){
                fprintf(stderr, "Missing 2 values after %s\n", arg);
                return 2;}
            if(arg_i >= n_args){
                fprintf(stderr, "Missing 1 value after %s\n", arg);
                return 2;}
            const char *label_name = args[arg_i - 1];
            const char *rgraph_name = args[arg_i];

            ARRAY_PUSH_NEW(label_mapping_def_t*, opts->label_mappings, mapping)
            mapping->label_name = label_name;
            mapping->rgraph_name = rgraph_name;
        }else if(!strcmp(arg, "--delay")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            opts->delay_goal = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-s")){
            arg_i += 2;
            if(arg_i >= n_args){
                fprintf(stderr, "Need 2 values after %s\n", arg);
                return 2;}
            opts->scw = atoi(args[arg_i - 1]);
            opts->sch = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-p")){
            arg_i += 2;
            if(arg_i >= n_args){
                fprintf(stderr, "Need 2 values after %s\n", arg);
                return 2;}
            opts->x0 = atoi(args[arg_i - 1]);
            opts->y0 = atoi(args[arg_i]);
        }else if(!strcmp(arg, "--pal")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            opts->palette_filename = args[arg_i];
        }else if(!strcmp(arg, "--mapper")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing name after %s\n", arg);
                return 2;}
            opts->mapper_name = args[arg_i];
        }else if(!strcmp(arg, "--palmapper")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing name after %s\n", arg);
                return 2;}
            opts->palmapper_name = args[arg_i];
        }else if(!strcmp(arg, "--font")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            opts->font_filename = args[arg_i];
        }else if(!strcmp(arg, "--fonts")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            opts->fonts_filename = args[arg_i];
        }else if(!strcmp(arg, "--nocontrols")){
            opts->show_editor_controls = false;
        }else if(!strcmp(arg, "--dump-bitmaps")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing integer (0 <= i <= 2) after %s\n", arg);
                return 2;}
            int i = atoi(args[arg_i]);
            if(i < 0)i = 0;
            if(i > 2)i = 2;
            opts->dump_bitmaps = i;
        }else if(!strcmp(arg, "screenshot")){
            opts->action = ACTION_SCREENSHOT;
        }else if(!strcmp(arg, "dump")){
            opts->action = ACTION_DUMP;
        }else if(!strcmp(arg, "dump_mapper")){
            opts->action = ACTION_DUMP_MAPPER;
        }else if(!strcmp(arg, "dump_palmapper")){
            opts->action = ACTION_DUMP_PALMAPPER;
        }else if(!strcmp(arg, "list")){
            opts->action = ACTION_LIST;
        }else if(!strcmp(arg, "list_mappers")){
            opts->action = ACTION_LIST_MAPPERS;
        }else if(!strcmp(arg, "list_palmappers")){
            opts->action = ACTION_LIST_PALMAPPERS;
        }else if(!strcmp(arg, "--dont-cache-bitmaps")){
            opts->cache_bitmaps = false;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            return 2;
        }
    }
    return 0;
}


static void _print_args(minieditor_t *editor, FILE *file){
    /* Print the commandline arguments to recreate current editor state */

    fprintf(file, "-f '%s'", editor->prend_filename);

    rendergraph_t *rgraph = minieditor_get_rgraph(editor);
    if(rgraph)fprintf(file, " -n '%s'", rgraph->name);

    fprintf(file, " -s %i %i -p %i %i -i %i -z %i -r %i -t %i",
        editor->scw, editor->sch, editor->x0, editor->y0,
        editor->frame_i, editor->zoom, editor->rot, editor->flip);

    if(!editor->show_editor_controls)fprintf(file, " --nocontrols");

    fputc('\n', file);
}


static int _render(minieditor_t *editor, SDL_Renderer *renderer){
    int err;

    RET_IF_SDL_NZ(SDL_FillRect(editor->surface, NULL, 0));

    int line_y = 0;
    err = minieditor_render(editor, &line_y);
    if(err)return err;

    {
        SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
            renderer, editor->surface);
        RET_IF_SDL_NULL(render_texture);
        SDL_RenderCopy(renderer, render_texture, NULL, NULL);
        SDL_DestroyTexture(render_texture);
    }

    SDL_RenderPresent(renderer);
    return 0;
}


static int _save_image(minieditor_t *editor, options_t *opts){
    const char *filename = opts->image_filename;
    if(!opts->quiet)fprintf(stderr, "Saving image to: %s\n", filename);
    RET_IF_SDL_NZ(SDL_SaveBMP(editor->surface, filename))
    return 0;
}


static int _reload_editor(minieditor_t *editor, options_t *opts){
    int err;

    prismelrenderer_cleanup(editor->prend);

    err = prismelrenderer_init(editor->prend, &vec4);
    if(err)return err;

    err = prismelrenderer_load(editor->prend, opts->prend_filename, NULL, NULL);
    if(err)return err;
    if(editor->prend->rendergraphs_len < 1){
        fprintf(stderr, "No rendergraphs in %s\n", opts->prend_filename);
        return 2;}
    editor->prend->cache_bitmaps = opts->cache_bitmaps;

    if(opts->rgraph_name){
        int rgraph_i = prismelrenderer_get_rgraph_i(
            editor->prend, opts->rgraph_name);
        if(rgraph_i < 0){
            fprintf(stderr, "Couldn't find rgraph: %s\n",
                opts->rgraph_name);
            return 2;
        }
        editor->cur_rgraph_i = rgraph_i;
    }

    return 0;
}


static int _poll_events(minieditor_t *editor, options_t *opts, bool *loop){
    int err;
    SDL_Event _event, *event = &_event;
    while(SDL_PollEvent(event)){
        if(event->type == SDL_QUIT){
            *loop = false;
            break;
        }

        if(event->type == SDL_KEYDOWN){
            if(event->key.keysym.sym == SDLK_ESCAPE){
                *loop = false;
                break;
            }else if(event->key.keysym.sym == SDLK_F6){
                _print_args(editor, stderr);
            }else if(event->key.keysym.sym == SDLK_F7){
                err = _save_image(editor, opts);
                if(err)return err;
            }else if(event->key.keysym.sym == SDLK_F8){
                err = _reload_editor(editor, opts);
                if(err)return err;
            }
        }

        err = minieditor_process_event(editor, event);
        if(err)return err;
    }
    return 0;
}


static int _mainloop(minieditor_t *editor, options_t *opts,
    palette_t *palette, SDL_Renderer *renderer
){
    int err;

    bool loop = true;
    while(loop){
        Uint32 tick0 = SDL_GetTicks();

        err = palette_update_sdl_palette(palette, editor->sdl_palette);
        if(err)return err;
        err = palette_step(palette);
        if(err)return err;

        err = _render(editor, renderer);
        if(err)return err;

        err = _poll_events(editor, opts, &loop);
        if(err)return err;

        Uint32 tick1 = SDL_GetTicks();
        Uint32 took = tick1 - tick0;
        if(took < editor->delay_goal)SDL_Delay(editor->delay_goal - took);
    }

    return 0;
}

static int _init_and_mainloop(options_t *opts, SDL_Renderer *renderer){
    /* NOTE: renderer may be NULL, in which case we're running without a
    GUI */
    int err;

    SDL_Palette *sdl_palette = SDL_AllocPalette(256);
    RET_IF_SDL_NULL(sdl_palette);

    SDL_Surface *surface = surface8_create(
        opts->scw, opts->sch, false, false, sdl_palette);
    if(surface == NULL)return 1;

    palette_t _palette, *palette = &_palette;
    err = palette_load(palette, opts->palette_filename, NULL);
    if(err)return err;

    font_t _font, *font=&_font;
    err = font_load(font, opts->font_filename, NULL);
    if(err)return err;

    prismelrenderer_t _prend, *prend = &_prend;
    err = prismelrenderer_init(prend, &vec4);
    if(err)return err;

    err = prismelrenderer_load(prend, opts->prend_filename, NULL, NULL);
    if(err)return err;
    if(prend->rendergraphs_len < 1){
        fprintf(stderr, "No rendergraphs in %s\n", opts->prend_filename);
        return 2;}
    prend->cache_bitmaps = opts->cache_bitmaps;

    prismelmapper_t *mapper = NULL;
    if(opts->mapper_name){
        mapper = prismelrenderer_get_mapper(prend, opts->mapper_name);
        if(!mapper){
            fprintf(stderr, "Couldn't find mapper: %s\n", opts->mapper_name);
            return 2;}
    }

    palettemapper_t *palmapper = NULL;
    if(opts->palmapper_name){
        palmapper = prismelrenderer_get_palmapper(prend, opts->palmapper_name);
        if(!palmapper){
            fprintf(stderr, "Couldn't find palmapper: %s\n", opts->palmapper_name);
            return 2;}
    }

    prismelrenderer_t _font_prend, *font_prend = &_font_prend;
    err = prismelrenderer_init(font_prend, &vec4);
    if(err)return err;

    err = prismelrenderer_load(font_prend, opts->fonts_filename, NULL, NULL);
    if(err)return err;

    const char *geomfont_name = "geomfont1";
    geomfont_t *geomfont = prismelrenderer_get_geomfont(
        font_prend, geomfont_name);
    if(geomfont == NULL){
        fprintf(stderr, "Couldn't find geomfont: %s\n", geomfont_name);
        return 2;
    }

    minieditor_t _editor, *editor=&_editor;
    minieditor_init(editor,
        surface, sdl_palette,
        mapper, palmapper,
        opts->prend_filename,
        font, geomfont, prend,
        opts->delay_goal, opts->scw, opts->sch);
    editor->frame_i = opts->frame_i;
    editor->zoom = opts->zoom;
    editor->rot = opts->rot;
    editor->flip = opts->flip;
    editor->show_editor_controls = opts->show_editor_controls;
    editor->x0 = opts->x0;
    editor->y0 = opts->y0;

    if(opts->rgraph_name){
        int rgraph_i = prismelrenderer_get_rgraph_i(
            prend, opts->rgraph_name);
        if(rgraph_i < 0){
            fprintf(stderr, "Couldn't find rgraph: %s\n",
                opts->rgraph_name);
            return 2;
        }
        editor->cur_rgraph_i = rgraph_i;
    }

    for(int i = 0; i < opts->label_mappings_len; i++){
        label_mapping_def_t *def = opts->label_mappings[i];
        ARRAY_PUSH_NEW(label_mapping_t*, editor->label_mappings, mapping)
        mapping->label_name = def->label_name;
        mapping->rgraph = prismelrenderer_get_rgraph(
            prend, def->rgraph_name);
        if(!mapping->rgraph){
            fprintf(stderr, "Couldn't find rgraph: %s\n",
                def->rgraph_name);
            return 2;
        }
    }

    if(renderer){
        /* We're running in a window -- interactive GUI mode, go! */
        err = _mainloop(editor, opts, palette, renderer);
        if(err)return err;

        fprintf(stderr, "Cleaning up...\n");
    }else switch(opts->action){
        /* We're running at the bare commandline, no GUI. */

        case ACTION_SCREENSHOT: {
            err = palette_update_sdl_palette(palette, editor->sdl_palette);
            if(err)return err;

            int line_y = 0;
            err = minieditor_render(editor, &line_y);
            if(err)return err;

            err = _save_image(editor, opts);
            if(err)return err;
        } break;
        case ACTION_DUMP: {
            rendergraph_t *rgraph = minieditor_get_rgraph(editor);
            if(!rgraph){
                fprintf(stderr, "Rgraph not found!\n");
                return 2;
            }

            err = rendergraph_calculate_labels(rgraph);
            if(err)return err;

            int n_spaces = 0;
            int dump_bitmaps = 0;
            rendergraph_dump(rgraph, stdout, n_spaces, dump_bitmaps);
        } break;
        case ACTION_DUMP_MAPPER: {
            if(!editor->mapper){
                fprintf(stderr, "No mapper! Use --mapper to specify one\n");
                return 2;
            }
            int n_spaces = 0;
            prismelmapper_dump(editor->mapper, stderr, n_spaces);
        } break;
        case ACTION_DUMP_PALMAPPER: {
            if(!editor->palmapper){
                fprintf(stderr, "No palmapper! Use --palmapper to specify one\n");
                return 2;
            }
            int n_spaces = 0;
            palettemapper_dump(editor->palmapper, stderr, n_spaces);
        } break;
        case ACTION_LIST: {
            for(int i = 0; i < prend->rendergraphs_len; i++){
                rendergraph_t *rgraph = prend->rendergraphs[i];
                printf("%s\n", rgraph->name);
            }
        } break;
        case ACTION_LIST_MAPPERS: {
            for(int i = 0; i < prend->mappers_len; i++){
                prismelmapper_t *mapper = prend->mappers[i];
                printf("%s\n", mapper->name);
            }
        } break;
        case ACTION_LIST_PALMAPPERS: {
            for(int i = 0; i < prend->palmappers_len; i++){
                palettemapper_t *palmapper = prend->palmappers[i];
                printf("%s\n", palmapper->name);
            }
        } break;
        default: {
            /* This should never happen... */
            fprintf(stderr,
                "Running without a GUI, but no command given...\n");
        } break;
    }

    minieditor_cleanup(editor);
    prismelrenderer_cleanup(font_prend);
    prismelrenderer_cleanup(prend);
    palette_cleanup(palette);
    font_cleanup(font);
    SDL_FreeSurface(surface);
    SDL_FreePalette(sdl_palette);

    return err;
}



int main_gui(options_t *opts){
    /* What happens when we run in a window, as an interactive GUI */

    int err;
    const char *window_title = "Geom Editor";
    if(SDL_Init(SDL_INIT_VIDEO)){
        err = 1;
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    }else{
        SDL_Window *window = SDL_CreateWindow(window_title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            opts->scw, opts->sch, opts->window_flags);

        if(!window){
            err = 1;
            fprintf(stderr, "SDL_CreateWindow error: %s\n",
                SDL_GetError());
        }else{
            SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

            if(!renderer){
                err = 1;
                fprintf(stderr, "SDL_CreateRenderer error: %s\n",
                    SDL_GetError());
            }else{

                err = _init_and_mainloop(opts, renderer);

                SDL_DestroyRenderer(renderer);
            }
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
    fprintf(stderr, "Exiting with code: %i\n", err);
    return err;
}


int main_nogui(options_t *opts){
    /* What happens when we run at the bare commandline */
    return _init_and_mainloop(opts, NULL);
}


int main(int n_args, char **args){
    int e = 0;

    /* The classic */
    srand(time(0));

    options_t opts = {0};
    options_init(&opts);
    {
        bool quit = false;
        int err = parse_options(&opts, n_args, (const char **)args, &quit);
        if(err){
            fputc('\n', stderr);
            print_help(stderr);
            return err;
        }
        if(quit)return 0;
    }

    if(opts.action == ACTION_DEFAULT)e = main_gui(&opts);
    else e = main_nogui(&opts);

    options_cleanup(&opts);
    return e;
}
