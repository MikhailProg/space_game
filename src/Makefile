CFLAGS := -Wall -Wextra
LDLIBS := -lm
TARGET := game

.PHONE: all clean

ifdef DEBUG
  CFLAGS += -O0 -g
else
  CFLAGS += -O2
  LDFLAGS += -s
endif

all: $(TARGET)

game: CFLAGS += $(shell pkg-config --cflags sdl)
game: LDLIBS += $(shell pkg-config --libs sdl)

clean:
	rm -f *.o $(TARGET)

