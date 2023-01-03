
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "minieditor.h"
#include "generic_printf.h"



void minieditor_cleanup(minieditor_t *editor){
    /* Nuthin */
}

void minieditor_init(minieditor_t *editor,
    SDL_Surface *surface, SDL_Palette *sdl_palette,
    const char *prend_filename,
    font_t *font, geomfont_t *geomfont,
    prismelrenderer_t *prend,
    int delay_goal, int scw, int sch
){
    editor->surface = surface;
    editor->sdl_palette = sdl_palette;
    editor->prend_filename = prend_filename;
    editor->font = font;
    editor->geomfont = geomfont;
    editor->prend = prend;

    editor->cur_rgraph_i = 0;
    editor->frame_i = 0;
    editor->delay_goal = delay_goal;
    editor->scw = scw;
    editor->sch = sch;
    editor->x0 = 0;
    editor->y0 = 0;
    editor->rot = 0;
    editor->flip = false;
    editor->zoom = 1;
    editor->show_editor_controls = true;

    editor->keydown_shift = false;
    editor->keydown_ctrl = false;
    editor->keydown_u = 0;
    editor->keydown_d = 0;
    editor->keydown_l = 0;
    editor->keydown_r = 0;
}

rendergraph_t *minieditor_get_rgraph(minieditor_t *editor){
    if(editor->prend->rendergraphs_len == 0)return NULL;
    return editor->prend->rendergraphs[editor->cur_rgraph_i];
}

static int _minieditor_print_controls(minieditor_t *editor, FILE *file,
    int col, int row, const char *msg, ...
){
    /* See: minieditor_print_controls */

    int err = 0;
    va_list vlist;
    va_start(vlist, msg);

    if(file){
        vfprintf(file, msg, vlist);
        fputc('\n', file);
    }else{
        err = minieditor_vprintf(editor, col, row, msg, vlist);
    }

    va_end(vlist);
    return err;
}

static void minieditor_print_controls(minieditor_t *editor,
    FILE *file, int *line_y_ptr
){
    /* If file is provided, fprintf to it.
    Otherwise, "print" to editor->surface.
    NOTE: line_y_ptr is optional. */

    int line_y = line_y_ptr? *line_y_ptr: 0;

    rendergraph_t *rgraph = minieditor_get_rgraph(editor);
    int animated_frame_i = !rgraph? 0: get_animated_frame_i(
        rgraph->animation_type, rgraph->n_frames, editor->frame_i);

    const char *rgraph_name = rgraph? rgraph->name: "(none)";
    int rgraph_n_frames = rgraph? rgraph->n_frames: 0;
    const char *rgraph_animation_type =
        rgraph? rgraph->animation_type: "(none)";
    _minieditor_print_controls(editor, file,
        0, line_y * editor->font->char_h,
        "Controls:\n"
        "  up/down - zoom (hold shift for tap mode)\n"
        "  left/right - rotate (hold shift for tap mode)\n"
        "  control + up/down/left/right - pan (hold shift...)\n"
        "  page up/down - cycle through available rendergraphs\n"
        "  0 - reset rotation\n"
        "  enter - show/hide controls\n"
        "  ctrl+enter - dump controls to stderr\n"
        "  shift+enter - dump current rendergraph to stderr\n"
        "Displaying rendergraphs from file: %s\n"
        "Rendergraph %i / %i: %s\n"
        "  pan=(%i,%i), rot = %i, flip = %c, zoom = %i\n"
        "  frame_i = %i (%i) / %i (%s)",
        editor->prend_filename,
        editor->cur_rgraph_i, editor->prend->rendergraphs_len, rgraph_name,
        editor->x0, editor->y0, editor->rot, editor->flip? 'y': 'n', editor->zoom,
        editor->frame_i, animated_frame_i, rgraph_n_frames, rgraph_animation_type);

    line_y += 13;

    if(line_y_ptr)*line_y_ptr = line_y;
}

int minieditor_render(minieditor_t *editor, int *line_y_ptr){
    int err;

    rendergraph_t *rgraph = minieditor_get_rgraph(editor);
    if(rgraph){
        /* Render rgraph */
        int x0 = editor->scw / 2 + editor->x0;
        int y0 = editor->sch / 2 + editor->y0;
        err = rendergraph_render(rgraph, editor->surface,
            editor->sdl_palette, editor->prend, x0, y0, editor->zoom,
            (vec_t){0}, editor->rot, editor->flip, editor->frame_i, NULL);
        if(err)return err;
    }

    if(editor->show_editor_controls){
        /* Render text */
        minieditor_print_controls(editor, NULL, line_y_ptr);
    }

    return 0;
}

int minieditor_process_event(minieditor_t *editor, SDL_Event *event){
    int err;
    switch(event->type){
        case SDL_KEYDOWN: {
            if(event->key.keysym.sym == SDLK_RETURN){
                if(event->key.keysym.mod & KMOD_CTRL){
                    minieditor_print_controls(editor, stderr, NULL);
                }else if(event->key.keysym.mod & KMOD_SHIFT){
                    rendergraph_t *rgraph = minieditor_get_rgraph(editor);
                    if(rgraph){
                        int n_spaces = 2;
                        int dump_bitmaps = 1;
                        /* dump_bitmaps: if 1, dumps bitmaps.
                        If 2, also dumps their surfaces. */

                        err = rendergraph_calculate_labels(rgraph);
                        if(err)return err;

                        rendergraph_dump(rgraph, stderr, n_spaces, dump_bitmaps);
                    }else{
                        fprintf(stderr, "(No rendergraph)\n");
                    }
                }else{
                    editor->show_editor_controls =
                        !editor->show_editor_controls;
                }
            }

            if(event->key.keysym.sym == SDLK_0){
                editor->x0 = 0; editor->y0 = 0;
                editor->rot = 0; editor->flip = false; editor->zoom = 1;}

            if(event->key.keysym.sym == SDLK_SPACE){
                editor->flip = !editor->flip;}

            #define IF_KEYDOWN(SYM, KEY) \
                if(event->key.keysym.sym == SDLK_##SYM \
                    && editor->keydown_##KEY == 0){ \
                        editor->keydown_##KEY = 2;}
            IF_KEYDOWN(UP, u)
            IF_KEYDOWN(DOWN, d)
            IF_KEYDOWN(LEFT, l)
            IF_KEYDOWN(RIGHT, r)
            #undef IF_KEYDOWN

            if(event->key.keysym.sym == SDLK_LSHIFT
                || event->key.keysym.sym == SDLK_RSHIFT){
                    editor->keydown_shift = true;}
            if(event->key.keysym.sym == SDLK_LCTRL
                || event->key.keysym.sym == SDLK_RCTRL){
                    editor->keydown_ctrl = true;}

            if(event->key.keysym.sym == SDLK_PAGEUP){
                editor->cur_rgraph_i++;
                if(editor->cur_rgraph_i >=
                    editor->prend->rendergraphs_len){
                        editor->cur_rgraph_i = 0;}}

            if(event->key.keysym.sym == SDLK_PAGEDOWN){
                editor->cur_rgraph_i--;
                if(editor->cur_rgraph_i < 0){
                    editor->cur_rgraph_i =
                        editor->prend->rendergraphs_len - 1;}}

            if(event->key.keysym.sym == SDLK_HOME){
                editor->frame_i++;}
            if(event->key.keysym.sym == SDLK_END){
                if(editor->frame_i > 0)editor->frame_i--;}

        } break;
        case SDL_KEYUP: {

            #define IF_KEYUP(SYM, KEY) \
                if(event->key.keysym.sym == SDLK_##SYM){ \
                    editor->keydown_##KEY = 0;}
            IF_KEYUP(UP, u)
            IF_KEYUP(DOWN, d)
            IF_KEYUP(LEFT, l)
            IF_KEYUP(RIGHT, r)
            #undef IF_KEYUP

            if(event->key.keysym.sym == SDLK_LSHIFT
                || event->key.keysym.sym == SDLK_RSHIFT){
                    editor->keydown_shift = false;}
            if(event->key.keysym.sym == SDLK_LCTRL
                || event->key.keysym.sym == SDLK_RCTRL){
                    editor->keydown_ctrl = false;}
        } break;
        default: break;
    }

    #define IF_EDITOR_KEY(KEY, BODY) \
        if(editor->keydown_##KEY >= (editor->keydown_shift? 2: 1)){ \
            editor->keydown_##KEY = 1; \
            BODY}
    IF_EDITOR_KEY(l, if(editor->keydown_ctrl){editor->x0 += 6;}else{editor->rot += 1;})
    IF_EDITOR_KEY(r, if(editor->keydown_ctrl){editor->x0 -= 6;}else{editor->rot -= 1;})
    IF_EDITOR_KEY(u, if(editor->keydown_ctrl){editor->y0 += 6;}else if(editor->zoom < MINIEDITOR_MAX_ZOOM){editor->zoom += 1;})
    IF_EDITOR_KEY(d, if(editor->keydown_ctrl){editor->y0 -= 6;}else if(editor->zoom > 1){editor->zoom -= 1;})
    #undef IF_EDITOR_KEY

    return 0;
}

int minieditor_vprintf(minieditor_t *editor, int col, int row,
    const char *msg, va_list vlist
){
    geomfont_blitter_t blitter;
    geomfont_blitter_render_init(&blitter, editor->geomfont,
        editor->surface, editor->sdl_palette,
        0, 0, col, row, 1, NULL, NULL);
    return generic_vprintf(&geomfont_blitter_putc_callback, &blitter,
        msg, vlist);
}

int minieditor_printf(minieditor_t *editor, int col, int row,
    const char *msg, ...
){
    int err = 0;
    va_list vlist;
    va_start(vlist, msg);

    err = minieditor_vprintf(editor, col, row, msg, vlist);

    va_end(vlist);
    return err;
}
