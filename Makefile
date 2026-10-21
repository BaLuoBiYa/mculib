CC     := gcc
CFLAGS := -flto -Wall -Wextra -Wundef -std=c11 -Og -g3 -I.
BUILD  := build/Debug
OBJDIR := $(BUILD)/obj
BIN    := $(BUILD)/hello_uart

SRCS := $(wildcard common/*.c subsystems/*.c app/src/*.c adapter/*/src/*.c)
OBJS := $(addprefix $(OBJDIR)/,$(SRCS:.c=.o))
DEPS := $(OBJS:.o=.d)

all: $(BIN) compdb

$(BIN): $(OBJS)
	$(CC) $^ -o $@

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(BIN)
	./$(BIN)

# clangd 用的编译数据库（.clangd 的 CompilationDatabase 指向 build/Debug）
compdb:
	@python3 tools/gen_compdb.py $(BUILD)/compile_commands.json \
	    "$(CC)" "$(filter-out -MMD -MP,$(CFLAGS))" $(SRCS)
	@ln -sfn Debug/compile_commands.json build/compile_commands.json

clean:
	rm -rf build

.PHONY: all run compdb clean

-include $(DEPS)