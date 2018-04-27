
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "prismel.h"

int prismelrenderer_init(prismelrenderer_t *renderer, vecspace_t *space){
    renderer->space = space;
    renderer->bitmap_list = NULL;
    renderer->prismel_list = NULL;
    return 0;
}

int prismelrenderer_push_prismel(prismelrenderer_t *renderer){
    prismel_t *prismel = calloc(1, sizeof(prismel_t));
    if(prismel == NULL)return 1;
    prismel->next = renderer->prismel_list;
    renderer->prismel_list = prismel;
    return 0;
}

int prismel_create_images(prismel_t *prismel, int n_images){
    prismel_image_t *images = calloc(n_images, sizeof(prismel_image_t));
    if(images == NULL)return 1;
    prismel->n_images = n_images;
    prismel->images = images;
    return 0;
}

int prismel_image_push_line(prismel_image_t *image){
    prismel_image_line_t *line = calloc(1, sizeof(prismel_image_line_t));
    if(line == NULL)return 1;
    line->next = image->line_list;
    image->line_list = line;
    return 0;
}

