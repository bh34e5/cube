CC=gcc
CFLAGS+=-Wall
CFLAGS+=-Werror
CFLAGS+=-Wpedantic

ifeq ($(UNAME), Darwin)
	CFLAGS+=-glldb
else
	CFLAGS+=-ggdb
endif

SRC=src
INCLUDE=include
BUILD=build
TARGET=cube

FILES=main.c \
			cube.c \
			tests.c \
			graphics.c \
			my_math.c \
			memory.c
OBJS=$(patsubst %.c,$(BUILD)/%.o,$(FILES))

SDL_CONFIG=$(shell sdl2-config --cflags --libs)

.PHONY: all clean build-dir

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDL_CONFIG) -lm -lGLEW -lGLU -lGL

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(foreach D,$(INCLUDE),-I$(D)) -c -o $@ $< $(SDL_CONFIG)

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -f $(TARGET)
	rm -r $(BUILD)
