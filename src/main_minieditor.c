
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "util.h"
#include "vec4.h"
#include "minieditor.h"


#define DEFAULT_SCW 1024
#define DEFAULT_SCH 768

/* How many milliseconds we want each frame to last */
#define DEFAULT_DELAY_GOAL 30


static void _print_args(minieditor_t *editor, FILE *file){
    /* Print the commandline arguments to recreate current image */

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


static int _poll_events(minieditor_t *editor, bool *loop){
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
            }
        }

        err = minieditor_process_event(editor, event);
        if(err)return err;
    }
    return 0;
}


static int _mainloop(minieditor_t *editor, palette_t *palette,
    SDL_Renderer *renderer
){
    int err;

    SDL_StartTextInput();

    bool loop = true;
    while(loop){
        Uint32 tick0 = SDL_GetTicks();

        err = palette_update_sdl_palette(palette, editor->sdl_palette);
        if(err)return err;
        err = palette_step(palette);
        if(err)return err;

        err = _render(editor, renderer);
        if(err)return err;

        err = _poll_events(editor, &loop);
        if(err)return err;

        Uint32 tick1 = SDL_GetTicks();
        Uint32 took = tick1 - tick0;
        if(took < editor->delay_goal)SDL_Delay(editor->delay_goal - took);
#ifdef GEOM_HEXGAME_DEBUG_FRAMERATE
        if(took > editor->delay_goal){
            fprintf(stderr, "WARNING: Frame rendered in %i ms "
                "(aiming for sub-%i ms)\n",
                took, editor->delay_goal);
        }
#endif
    }

    SDL_StopTextInput();
    return 0;
}

#define DEFAULT_PREND_FILENAME "data/test.fus"


static void print_help(){
    fprintf(stderr,
        "Options:\n"
        "  -h | --help      Print this message and exit\n"
        "  -F               Fullscreen\n"
        "  -FD              Fullscreen Desktop\n"
        "  -f  FILENAME     Load prend data (default: " DEFAULT_PREND_FILENAME ")\n"
        "  -n  NAME         Load rgraph\n"
        "  -i  FRAME        Set frame_i\n"
        "  -z  ZOOM         Set zoom (1 - %i)\n"
        "  -r  ROT          Set rotation\n"
        "  -t  FLIP         Set flip\n"
        "  --delay TICKS    Set delay goal (default: %i)\n"
        "  -s SCW SCH       Set screen width and height (default: %i %i)\n"
        "  -p X0 Y0         Set screen position\n"
        "  --nocontrols     Don't show controls initially\n"
    , MINIEDITOR_MAX_ZOOM, DEFAULT_DELAY_GOAL, DEFAULT_SCW, DEFAULT_SCH);
}



int main(int n_args, char *args[]){
    int e = 0;
    Uint32 window_flags = SDL_WINDOW_SHOWN;
    const char *prend_filename = DEFAULT_PREND_FILENAME;
    const char *rgraph_name = NULL;
    int zoom = 1;
    int frame_i = 0;
    rot_t rot = 0;
    flip_t flip = false;
    bool show_editor_controls = true;
    bool cache_bitmaps = true;
    int delay_goal = DEFAULT_DELAY_GOAL;
    int scw = DEFAULT_SCW;
    int sch = DEFAULT_SCH;
    int x0 = 0;
    int y0 = 0;

    /* The classic */
    srand(time(0));

    for(int arg_i = 1; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            return 0;
        }else if(!strcmp(arg, "-F")){
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }else if(!strcmp(arg, "-FD")){
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }else if(!strcmp(arg, "-f")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 2;}
            prend_filename = args[arg_i];
        }else if(!strcmp(arg, "-n")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing rgraph name after %s\n", arg);
                return 2;}
            rgraph_name = args[arg_i];
        }else if(!strcmp(arg, "-i")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            frame_i = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-z")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            zoom = atoi(args[arg_i]);
            if(zoom < 1)zoom = 1;
            if(zoom > MINIEDITOR_MAX_ZOOM)zoom = MINIEDITOR_MAX_ZOOM;
        }else if(!strcmp(arg, "-r")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            rot = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-t")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            flip = atoi(args[arg_i]);
        }else if(!strcmp(arg, "--delay")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing value after %s\n", arg);
                return 2;}
            delay_goal = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-s")){
            arg_i += 2;
            if(arg_i >= n_args){
                fprintf(stderr, "Need 2 values after %s\n", arg);
                return 2;}
            scw = atoi(args[arg_i - 1]);
            sch = atoi(args[arg_i]);
        }else if(!strcmp(arg, "-p")){
            arg_i += 2;
            if(arg_i >= n_args){
                fprintf(stderr, "Need 2 values after %s\n", arg);
                return 2;}
            x0 = atoi(args[arg_i - 1]);
            y0 = atoi(args[arg_i]);
        }else if(!strcmp(arg, "--nocontrols")){
            show_editor_controls = false;
        }else if(!strcmp(arg, "--dont_cache_bitmaps")){
            cache_bitmaps = false;
        }else{
            print_help();
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            return 2;
        }
    }

    const char *window_title = "Geom Editor";

    if(SDL_Init(SDL_INIT_VIDEO)){
        e = 1;
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    }else{
        SDL_Window *window = SDL_CreateWindow(window_title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            scw, sch, window_flags);

        if(!window){
            e = 1;
            fprintf(stderr, "SDL_CreateWindow error: %s\n",
                SDL_GetError());
        }else{
            SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

            if(!renderer){
                e = 1;
                fprintf(stderr, "SDL_CreateRenderer error: %s\n",
                    SDL_GetError());
            }else{
                int err;

                SDL_Palette *sdl_palette = SDL_AllocPalette(256);
                RET_IF_SDL_NULL(sdl_palette);

                SDL_Surface *surface = surface8_create(
                    scw, sch, false, false, sdl_palette);
                if(surface == NULL)return 1;

                palette_t _palette, *palette = &_palette;
                err = palette_load(palette, "data/pal1.fus", NULL);
                if(err)return err;

                font_t _font, *font=&_font;
                err = font_load(font, strdup("data/font.fus"), NULL);
                if(err)return err;

                prismelrenderer_t _prend, *prend = &_prend;
                err = prismelrenderer_init(prend, &vec4);
                if(err)return err;

                err = prismelrenderer_load(prend, prend_filename, NULL);
                if(err)return err;
                if(prend->rendergraphs_len < 1){
                    fprintf(stderr, "No rendergraphs in %s\n", prend_filename);
                    return 2;}
                prend->cache_bitmaps = cache_bitmaps;

                prismelrenderer_t _font_prend, *font_prend = &_font_prend;
                err = prismelrenderer_init(font_prend, &vec4);
                if(err)return err;

                err = prismelrenderer_load(font_prend, "data/fonts.fus", NULL);
                if(err)return err;

                const char *geomfont_name = "geomfont1";
                geomfont_t *geomfont = prismelrenderer_get_geomfont(
                    font_prend, geomfont_name);
                if(geomfont == NULL){
                    fprintf(stderr, "Couldn't find geomfont: %s\n", geomfont_name);
                    return 2;
                }

                minieditor_t editor;
                minieditor_init(&editor,
                    surface, sdl_palette, prend_filename,
                    font, geomfont, prend,
                    delay_goal, scw, sch);
                editor.frame_i = frame_i;
                editor.zoom = zoom;
                editor.rot = rot;
                editor.flip = flip;
                editor.show_editor_controls = show_editor_controls;
                editor.x0 = x0;
                editor.y0 = y0;

                if(rgraph_name){
                    int rgraph_i = prismelrenderer_get_rgraph_i(
                        prend, rgraph_name);
                    if(rgraph_i < 0){
                        fprintf(stderr, "Couldn't find rgraph: %s\n", rgraph_name);
                        return 2;
                    }
                    editor.cur_rgraph_i = rgraph_i;
                }

                e = _mainloop(&editor, palette, renderer);

                fprintf(stderr, "Cleaning up...\n");
                minieditor_cleanup(&editor);
                prismelrenderer_cleanup(font_prend);
                prismelrenderer_cleanup(prend);
                font_cleanup(font);
                SDL_FreeSurface(surface);
                SDL_FreePalette(sdl_palette);

                SDL_DestroyRenderer(renderer);
            }
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
    fprintf(stderr, "Exiting with code: %i\n", e);
    return e;
}
