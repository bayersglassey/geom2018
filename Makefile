
CFLAGS += -O2 -g -rdynamic -std=c99 \
 `sdl2-config --libs --cflags` \
 -Wall -Werror -Wno-unused -Wno-missing-braces -Wno-tautological-compare -Wno-parentheses \
 -DGEOM_HEXGAME_DEBUG_FRAMERATE -D_WANT_STRNLEN -D_WANT_STRNDUP

PROGS = \
 bin/collmaptool bin/hexpicturetest bin/sdltest bin/directorytest \
 bin/prendtool bin/demo

TESTS = \
 bin/lexertest bin/frozenstringtest bin/geomtest bin/stringstoretest bin/varstest

PROGS += $(TESTS)

OFILES = \
 src/hexcollmap.o src/hexcollmap_parse.o \
 src/str_utils.o src/file_utils.o src/lexer.o src/write.o src/stringstore.o src/vars.o \
 src/geom.o src/vec4.o src/hexspace.o src/bounds.o src/location.o \
 src/font.o src/console.o src/directory.o src/directory_shell.o \
 src/hexpicture.o src/generic_printf.o

SDL_OFILES = \
 src/prismelrenderer.o src/prismelrenderer_parse.o \
 src/rendergraph.o src/anim.o src/util.o src/sdl_util.o \
 src/sdlfont.o src/geomfont.o \
 src/hexmap.o src/hexmap_submap_create_rgraph.o \
 src/hexgame.o src/hexgame_body.o src/hexgame_player.o src/hexgame_recording.o \
 src/hexgame_state.o src/hexgame_actor.o \
 src/test_app.o src/test_app_console.o src/test_app_game.o src/test_app_editor.o \
 src/test_app_list.o src/test_app_commands.o \
 src/test_app_list_choices.o src/test_app_list_maps.o src/test_app_list_submaps.o \
 src/test_app_list_bodies.o src/test_app_list_players.o src/test_app_list_actors.o \
 src/test_app_list_recording.o src/test_app_list_utils.o

.PHONY: all clean check www

all: $(PROGS)

clean:
	rm -f src/*.o $(PROGS)

check: $(TESTS)
	./runtests.sh $(TESTS)

bin/collmaptool: src/main_collmaptool.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/lexertest: src/main_lexertest.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/frozenstringtest: src/main_frozen_string_test.o src/frozen_string.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/geomtest: src/main_geomtest.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/stringstoretest: src/main_stringstoretest.o src/stringstore.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/varstest: src/main_varstest.o src/vars.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/hexpicturetest: src/main_hexpicture.o src/hexpicture.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/sdltest: src/main_sdltest.o src/util.o src/file_utils.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/directorytest: src/main_directorytest.o src/directory.o src/directory_shell.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/prendtool: src/main_prendtool.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/demo: src/main_demo.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^
