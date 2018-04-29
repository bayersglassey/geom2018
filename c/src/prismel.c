
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "prismel.h"

int prismelrenderer_init(prismelrenderer_t *renderer, vecspace_t *space){
    renderer->space = space;
    renderer->bitmap_list = NULL;
    renderer->prismel_list = NULL;
    return 0;
}

void prismelrenderer_dump(prismelrenderer_t *renderer, FILE *f){
    fprintf(f, "prismelrenderer: %p\n", renderer);
    if(renderer == NULL)return;
    fprintf(f, "  space: %p\n", renderer->space);

    fprintf(f, "  prismels:\n");
    for(prismel_t *prismel = renderer->prismel_list;
        prismel != NULL; prismel = prismel->next
    ){
        fprintf(f, "    prismel: %p\n", prismel);
        fprintf(f, "      name: %s\n", prismel->name);
        fprintf(f, "      n_images: %i\n", prismel->n_images);
        fprintf(f, "      images:\n");
        for(int i = 0; i < prismel->n_images; i++){
            prismel_image_t *image = &prismel->images[i];
            fprintf(f, "        image: %p", image);
            for(prismel_image_line_t *line = image->line_list;
                line != NULL; line = line->next
            ){
                fprintf(f, " (%i %i %i)", line->x, line->y, line->w);
            }
            fprintf(f, "\n");
        }
    }

    fprintf(f, "  bitmaps:\n");
    for(prismelrenderer_bitmap_t *bitmap = renderer->bitmap_list;
        bitmap != NULL; bitmap = bitmap->next
    ){
        fprintf(f, "    %p\n", bitmap);
    }
}

int prismelrenderer_push_prismel(prismelrenderer_t *renderer){
    int err;
    prismel_t *prismel = calloc(1, sizeof(prismel_t));
    if(prismel == NULL)return 1;
    prismel->next = renderer->prismel_list;
    renderer->prismel_list = prismel;
    err = prismel_create_images(prismel, renderer->space->rot_max);
    if(err)goto err;
    return 0;
err:
    free(prismel);
    return err;
}

int prismel_create_images(prismel_t *prismel, int n_images){
    prismel_image_t *images = calloc(n_images, sizeof(prismel_image_t));
    if(images == NULL)return 1;
    prismel->n_images = n_images;
    prismel->images = images;
    return 0;
}

int prismel_image_push_line(prismel_image_t *image, int x, int y, int w){
    prismel_image_line_t *line = calloc(1, sizeof(prismel_image_line_t));
    if(line == NULL)return 1;
    line->x = x;
    line->y = y;
    line->w = w;
    line->next = image->line_list;
    image->line_list = line;
    return 0;
}

prismel_t *prismelrenderer_get_prismel(prismelrenderer_t *renderer, char *name){
    prismel_t *prismel = renderer->prismel_list;
    while(prismel != NULL){
        if(strcmp(prismel->name, name) == 0)return prismel;
        prismel = prismel->next;
    }
    return NULL;
}
