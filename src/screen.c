
#include "screen.h"
#include "util.h"


void screen_cleanup(screen_t *screen){
    SDL_FreePalette(screen->palette);
    if(screen->gui){
        SDL_DestroyTexture(screen->texture);
        SDL_DestroyRenderer(screen->renderer);
        SDL_DestroyWindow(screen->window);
    }
}

static int _screen_init(screen_t *screen, int w, int h, bool gui,
    const char *title, Uint32 window_flags, Uint32 renderer_flags
){
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
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

        renderer = SDL_CreateRenderer(window, -1, renderer_flags);
        if(!renderer){
            fprintf(stderr, "Couldn't create renderer: %s\n", SDL_GetError());
            goto err;
        }

        texture = SDL_CreateTexture(renderer, format,
            SDL_TEXTUREACCESS_STREAMING, w, h);
        if(!texture){
            fprintf(stderr, "Couldn't create texture: %s\n", SDL_GetError());
            goto err;
        }
    }

    screen->w = w;
    screen->h = h;
    screen->gui = gui;
    screen->window = window;
    screen->renderer = renderer;
    screen->texture = texture;
    screen->palette = palette;
    screen->surface = NULL;
    return 0;

err:
    if(palette)SDL_FreePalette(palette);
    if(texture)SDL_DestroyTexture(texture);
    if(renderer)SDL_DestroyRenderer(renderer);
    if(window)SDL_DestroyWindow(window);
    return 1;
}

int screen_init_gui(screen_t *screen, int w, int h,
    const char *title, Uint32 window_flags, Uint32 renderer_flags
){
    return _screen_init(screen, w, h, true, title,
        window_flags, renderer_flags);
}

int screen_init_no_gui(screen_t *screen, int w, int h){
    return _screen_init(screen, w, h, false, NULL, 0, 0);
}

int screen_begin_render(screen_t *screen){
    if(screen->gui){
        RET_IF_SDL_NZ(SDL_LockTextureToSurface(screen->texture, NULL, &screen->surface));
        RET_IF_SDL_NZ(SDL_SetSurfacePalette(screen->surface, screen->palette));
    }else{
        screen->surface = surface8_create(screen->w, screen->h, false, false, screen->palette);
        if(!screen->surface)return 1;
    }
    return 0;
}

void screen_finish_render(screen_t *screen){
    if(screen->gui){
        SDL_UnlockTexture(screen->texture);
        SDL_RenderCopy(screen->renderer, screen->texture, NULL, NULL);
        SDL_RenderPresent(screen->renderer);
    }else{
        SDL_FreeSurface(screen->surface);
    }
    screen->surface = NULL;
}
