
#include "screen.h"
#include "util.h"


void screen_cleanup(screen_t *screen){
    SDL_FreePalette(screen->palette);
    if(screen->gui){
        SDL_DestroyWindow(screen->window);
    }
}

static int _screen_init(screen_t *screen, int w, int h, bool gui,
    const char *title, Uint32 window_flags
){
    SDL_Window *window = NULL;
    SDL_Palette *palette = NULL;

    Uint32 format = SDL_MasksToPixelFormatEnum(8, 0, 0, 0, 0);
    if (format == SDL_PIXELFORMAT_UNKNOWN) {
        fprintf(stderr, "Couldn't get 8-bit pixel format\n");
        goto err;
    }

    palette = SDL_AllocPalette(256);
    if(!palette){
        fprintf(stderr, "Couldn't allocate 8-bit palette: %s\n", SDL_GetError());
    }

    if(gui){
        window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            w, h, window_flags);
        if(!window){
            fprintf(stderr, "Couldn't create window: %s\n", SDL_GetError());
            goto err;
        }
    }

    screen->w = w;
    screen->h = h;
    screen->gui = gui;
    screen->window = window;
    screen->palette = palette;
    screen->surface = NULL;
    return 0;

err:
    if(palette)SDL_FreePalette(palette);
    if(window)SDL_DestroyWindow(window);
    return 1;
}

int screen_init_gui(screen_t *screen, int w, int h,
    const char *title, Uint32 window_flags
){
    return _screen_init(screen, w, h, true, title, window_flags);
}

int screen_init_no_gui(screen_t *screen, int w, int h){
    return _screen_init(screen, w, h, false, NULL, 0);
}

int screen_begin_render(screen_t *screen){
    if(screen->gui){
        screen->surface = SDL_GetWindowSurface(screen->window);
        RET_IF_SDL_NULL(screen->surface);
    }else{
        screen->surface = surface8_create(screen->w, screen->h, false, false, screen->palette);
        if(!screen->surface)return 1;
    }
    return 0;
}

void screen_finish_render(screen_t *screen){
    if(screen->gui){
        SDL_UpdateWindowSurface(screen->window);
    }else{
        SDL_FreeSurface(screen->surface);
    }
    screen->surface = NULL;
}

Uint32 screen_get_palette_color(screen_t *screen, int i){
    SDL_Color color = screen->palette->colors[i];
    return SDL_MapRGB(screen->surface->format, color.r, color.g, color.b);
}
