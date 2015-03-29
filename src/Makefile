CFLAGS += `pkg-config --cflags sdl`

LIBS += `pkg-config --libs sdl`
LIBS += `pkg-config --libs libavutil`
LIBS += `pkg-config --libs libavformat`
LIBS += `pkg-config --libs libavcodec`
LIBS += `pkg-config --libs libswresample`
LIBS += `pkg-config --libs libswscale`
LIBS += `pkg-config --libs libavfilter`
LIBS += `pkg-config --libs libavdevice`
LIBS += -lm

game: game.o ffplay.o cmdutils.o
	gcc -Wall $(LIBS) $^ -o $@

%.o: %.c
	gcc -Wall $(CFLAGS) -c $<

clean:
	rm -f *.o
	
