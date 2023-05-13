CC=clang
CFLAGS=-Wall -MMD -pthread -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=199309L
LDFLAGS=-lpthread -lncurses

DIR_SRC=src
DIR_BUILD=build
DIR_OBJ=$(DIR_BUILD)/obj
DIR_BIN=$(DIR_BUILD)/bin

TARGET=$(DIR_BIN)/bytenuts

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CFLAGS += -g
else
	CFLAGS += -O2
endif

SRCS=$(wildcard $(DIR_SRC)/*.c)
OBJS=$(addprefix $(DIR_OBJ)/,$(addsuffix .o,$(basename $(SRCS))))
DEPS=$(addprefix $(DIR_OBJ)/,$(addsuffix .d,$(basename $(SRCS))))

.PHONY: all install uninstall clean

all: $(TARGET)

install:
	ln -s $(realpath $(TARGET)) /usr/local/bin/bytenuts

uninstall:
	rm -f /usr/local/bin/bytenuts

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	@echo "link and compile for $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS)

$(DIR_OBJ)/$(DIR_SRC)/%.o: $(DIR_SRC)/%.c
	@mkdir -p $(dir $@)
	@echo "compile $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(DIR_BUILD)

-include $(DEPS)
