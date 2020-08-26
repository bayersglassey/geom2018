
CFLAGS += -g -rdynamic -std=c99 \
 `sdl2-config --libs --cflags` \
 -Wall -Werror -Wno-unused -Wno-missing-braces -Wno-tautological-compare -Wno-parentheses \
 -DGEOM_HEXGAME_DEBUG_FRAMERATE -D_WANT_STRNLEN -D_WANT_STRNDUP

PROGS = collmaptool lexertest frozenstringtest hexpicturetest demo

.PHONY: all clean

all: $(PROGS)

clean:
	rm -f src/*.o $(PROGS)

collmaptool: src/main_collmaptool.o \
 src/hexcollmap.o src/hexcollmap_parse.o src/geom.o src/hexspace.o \
 src/lexer.o src/write.o src/vars.o src/str_utils.o src/file_utils.o
	$(CC) $(CFLAGS) -o $@ $^

lexertest: src/main_lexertest.o \
 src/lexer.o src/vars.o src/str_utils.o
	$(CC) $(CFLAGS) -o $@ $^

frozenstringtest: src/main_frozen_string_test.o src/frozen_string.o
	$(CC) $(CFLAGS) -o $@ $^

hexpicturetest: src/main_hexpicture.o src/hexpicture.o
	$(CC) $(CFLAGS) -o $@ $^

demo: src/main_demo.o \
 src/anim.o src/hexmap.o src/hexcollmap.o src/hexcollmap_parse.o src/hexmap_submap_create_rgraph.o \
 src/util.o src/str_utils.o src/file_utils.o src/lexer.o src/write.o src/stringstore.o src/vars.o \
 src/geom.o src/vec4.o src/hexspace.o src/bounds.o src/location.o \
 src/font.o src/sdlfont.o src/geomfont.o src/console.o \
 src/test_app.o src/test_app_console.o src/test_app_game.o src/test_app_editor.o \
 src/test_app_list.o src/test_app_commands.o src/hexgame.o \
 src/test_app_list_choices.o src/test_app_list_maps.o src/test_app_list_submaps.o \
 src/test_app_list_bodies.o src/test_app_list_players.o src/test_app_list_actors.o \
 src/test_app_list_recording.o src/test_app_list_utils.o \
 src/hexgame_body.o src/hexgame_player.o src/hexgame_recording.o src/hexgame_state.o \
 src/hexgame_actor.o \
 src/prismelrenderer.o src/prismelrenderer_parse.o \
 src/hexpicture.o \
 src/rendergraph.o src/sdl_util.o src/generic_printf.o
	$(CC) $(CFLAGS) -o $@ $^
