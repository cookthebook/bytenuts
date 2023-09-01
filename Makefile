OS=LINUX

DIR_SRC=src
DIR_BUILD=build
DIR_OBJ=$(DIR_BUILD)/obj
DIR_BIN=$(DIR_BUILD)/bin

CFLAGS=-Wall -MMD -pthread -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=199309L
LIB_DEPS=
LIB_OBJS=

SRCS=$(wildcard $(DIR_SRC)/*.c)
OBJS=$(addprefix $(DIR_OBJ)/,$(addsuffix .o,$(basename $(SRCS))))
DEPS=$(addprefix $(DIR_OBJ)/,$(addsuffix .d,$(basename $(SRCS))))

ifeq ($(OS), LINUX)
	CC=clang
	LDFLAGS=-lncurses -lpthread
	TARGET=$(DIR_BIN)/bytenuts
else
	CC=x86_64-w64-mingw32-gcc
	CFLAGS+=-Ilib/PDCurses/wincon/ -Ilib/PDCurses/
	LDFLAGS=-lpthread -static
	LIB_DEPS+=PDCurses
	LIB_OBJS+=lib/PDCurses/wincon/*.o
	TARGET=$(DIR_BIN)/bytenuts.exe
endif

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CFLAGS += -g
else
	CFLAGS += -O2
endif

.PHONY: all install uninstall clean PDCurses

all: $(TARGET)

install:
	ln -s $(realpath $(TARGET)) /usr/local/bin/bytenuts

uninstall:
	rm -f /usr/local/bin/bytenuts

$(TARGET): $(OBJS) $(LIB_DEPS)
	@mkdir -p $(dir $@)
	@echo "link and compile for $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LIB_OBJS)

$(DIR_OBJ)/$(DIR_SRC)/%.o: $(DIR_SRC)/%.c
	@mkdir -p $(dir $@)
	@echo "compile $<"
	@$(CC) $(CFLAGS) -c $< -o $@

PDCurses:
	cd lib/PDCurses/wincon && $(MAKE) -j CC=x86_64-w64-mingw32-gcc

clean:
	rm -rf $(DIR_BUILD)

-include $(DEPS)
