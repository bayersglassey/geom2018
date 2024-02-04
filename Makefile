
# Used to be able to print stacktraces (with GNU backtrace) if I included
# these in CFLAGS: -g -rdynamic
# ...but it doesn't seem to work anymore.
CFLAGS += -O2 -std=c99 \
 $(shell sdl2-config --cflags) \
 -Wall -Werror -Wno-unused -Wno-missing-braces -Wno-tautological-compare -Wno-parentheses

LIBS += $(shell sdl2-config --libs) -lm

PROGS = \
 bin/lexertool bin/collmaptool bin/hexpicturetest bin/sdltest bin/directorytest \
 bin/prendtool bin/demo bin/minieditor bin/geomtool bin/animtool bin/audiotool

TESTS = \
 bin/lexertest bin/frozenstringtest bin/geomtest bin/stringstoretest bin/varstest

PROGS += $(TESTS)

OFILES = \
 src/hexcollmap.o src/hexcollmap_parse.o src/hexcollmap_write.o \
 src/str_utils.o src/file_utils.o src/lexer.o src/write.o src/stringstore.o \
 src/geom_lexer_utils.o src/vars.o src/var_utils.o src/valexpr.c \
 src/geom.o src/vec4.o src/hexspace.o src/bounds.o src/hexbox.o src/hexgame_location.o \
 src/font.o src/console.o src/directory.o src/directory_shell.o \
 src/hexpicture.o src/generic_printf.o src/hexgame_vars_props.o

SDL_OFILES = \
 src/prismelrenderer.o src/prismelrenderer_parse.o \
 src/rendergraph.o src/anim.o src/util.o src/sdl_util.o \
 src/sdlfont.o src/geomfont.o src/minieditor.o \
 src/hexmap.o src/hexmap_submap_create_rgraph.o \
 src/hexgame.o src/hexgame_body.o src/hexgame_player.o src/hexgame_recording.o \
 src/hexgame_state.o src/hexgame_actor.o src/hexgame_audio.o \
 src/hexgame_savelocation.o src/save_slots.o

TEST_APP_OFILES = \
 src/test_app.o src/test_app_console.o src/test_app_game.o src/test_app_editor.o \
 src/test_app_list.o src/test_app_commands.o \
 src/test_app_list_choices.o src/test_app_list_maps.o src/test_app_list_submaps.o \
 src/test_app_list_bodies.o src/test_app_list_players.o src/test_app_list_actors.o \
 src/test_app_list_recording.o src/test_app_list_utils.o src/test_app_menu.o

.PHONY: all clean check www

all: $(PROGS)

clean:
	rm -f src/*.o src/main/*.o $(PROGS)

check: $(TESTS)
	./runtests.sh $(TESTS) tests/*.sh

bin/collmaptool: src/main/collmaptool.o $(OFILES) src/mapeditor.o src/rawinput.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/lexertest: src/main/lexertest.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/lexertool: src/main/lexertool.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/frozenstringtest: src/main/frozen_string_test.o src/frozen_string.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/geomtest: src/main/geomtest.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/stringstoretest: src/main/stringstoretest.o src/stringstore.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/varstest: src/main/varstest.o src/vars.o src/valexpr.o src/lexer.o src/write.o src/str_utils.o src/file_utils.o src/var_utils.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/hexpicturetest: src/main/hexpicture.o src/hexpicture.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/sdltest: src/main/sdltest.o src/util.o src/file_utils.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/directorytest: src/main/directorytest.o src/directory.o src/directory_shell.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/prendtool: src/main/prendtool.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/demo: src/main/demo.o $(OFILES) $(SDL_OFILES) $(TEST_APP_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/minieditor: src/main/minieditor.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/geomtool: src/main/geomtool.o $(OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/animtool: src/main/animtool.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

bin/audiotool: src/main/audiotool.o $(OFILES) $(SDL_OFILES)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
