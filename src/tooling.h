#ifndef _TOOLING_H_
#define _TOOLING_H_

#include "hexgame.h"

void hexgame_cleanup_for_tooling(hexgame_t *game);
int hexgame_init_for_tooling(hexgame_t *game, const char *prend_filename);

#endif
