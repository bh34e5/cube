CC=gcc
CFLAGS+=-Wall
CFLAGS+=-Werror
CFLAGS+=-Wpedantic
CFLAGS+=-glldb

SRC=src
INCLUDE=include
BUILD=build
TARGET=cube

FILES=main.c cube.c tests.c
OBJS=$(patsubst %.c,$(BUILD)/%.o,$(FILES))

.PHONY: all clean build-dir

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(foreach D,$(INCLUDE),-I$(D)) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -f $(TARGET)
	rm -r $(BUILD)
