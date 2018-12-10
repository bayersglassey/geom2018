

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "hexspace.h"
#include "prismelrenderer.h"
#include "array.h"
#include "util.h"



void actor_cleanup(actor_t *actor){
}

int actor_init(actor_t *actor){
    actor->player = NULL;
    return 0;
}



int actor_step(actor_t *actor, struct hexgame *game){
    return 0;
}
