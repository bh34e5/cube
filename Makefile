CC=gcc
CFLAGS+=-Wall
CFLAGS+=-Werror
CFLAGS+=-Wpedantic
CFLAGS+=-glldb

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
	$(CC) -o $@ $^ $(SDL_CONFIG) -lm

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(foreach D,$(INCLUDE),-I$(D)) -c -o $@ $< $(SDL_CONFIG)

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -f $(TARGET)
	rm -r $(BUILD)
