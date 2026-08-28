# C Koans — build and run
#
#   make              build and walk the path until a koan stops you
#   make list         show every lesson in order
#   make KOAN=name    run a single lesson
#   make FROM=name    start partway along the path
#   make all-koans    do not stop at the first failure
#   make solutions    run the reference answers (they should all pass)
#   make san          rebuild with the address and UB sanitizers
#   make clean        remove build artifacts
#
# Requires a C23 compiler: GCC 14+ or Clang 18+.

CC      ?= cc
BUILD   ?= build

WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wvla -Wno-unused-parameter

# The `__` blank is a deliberate exception (see koan.h), but only Clang
# diagnoses reserved identifiers — passing these to GCC just makes it complain
# about flags it does not recognise.
CC_IS_CLANG := $(shell $(CC) --version 2>/dev/null | grep -ci clang)
ifneq ($(CC_IS_CLANG),0)
WARNINGS += -Wno-reserved-identifier -Wno-reserved-macro-identifier
endif

# Feature-test macros. `-std=c23` (rather than `gnu23`) defines __STRICT_ANSI__,
# and glibc responds by hiding every POSIX declaration — sigaction, fork, socket
# and the rest. The macOS SDK is permissive by default, which is why this only
# shows up on Linux.
#
# _DEFAULT_SOURCE exposes POSIX.1-2008 on glibc; _DARWIN_C_SOURCE does the same
# on Apple's SDK. Each is ignored by the platform it does not belong to, so
# defining both is portable and needs no conditionals.
POSIX_DEFS := -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE

CFLAGS  ?= -std=c23 -g -O0 $(WARNINGS) $(POSIX_DEFS) -Iinclude -Ikoans
LDFLAGS ?=
LDLIBS  ?= -lm -lpthread

KOAN_SRCS := $(shell find koans -name '*.c' 2>/dev/null | sort)
SOLN_SRCS := $(shell find solutions -name '*.c' 2>/dev/null | sort)
CORE_SRCS := src/koan.c src/runner.c

KOAN_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(KOAN_SRCS))
SOLN_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SOLN_SRCS))
CORE_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(CORE_SRCS))

TARGET  := $(BUILD)/bin/koans
SOLNBIN := $(BUILD)/bin/solutions

RUNARGS :=
ifdef KOAN
RUNARGS += --only $(KOAN)
endif
ifdef FROM
RUNARGS += --from $(FROM)
endif

.PHONY: walk
walk: $(TARGET)
	@./$(TARGET) $(RUNARGS)

.PHONY: list
list: $(TARGET)
	@./$(TARGET) --list

.PHONY: all-koans
all-koans: $(TARGET)
	@./$(TARGET) --all $(RUNARGS)

.PHONY: solutions
solutions: $(SOLNBIN)
	@./$(SOLNBIN) $(RUNARGS)

$(TARGET): $(CORE_OBJS) $(KOAN_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# The solutions binary reuses the same runner and manifest, but compiles the
# reference sources instead of the ones you are editing.
$(SOLNBIN): $(CORE_OBJS) $(SOLN_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# Sanitized builds go in their own tree. Object files compiled with
# -fsanitize cannot be linked without it, so sharing a directory with the
# normal build produces baffling "undefined __asan_*" link errors.
SAN_CFLAGS  := -std=c23 -g -O1 $(WARNINGS) $(POSIX_DEFS) -Iinclude -Ikoans \
               -fsanitize=address,undefined -fno-omit-frame-pointer
SAN_LDFLAGS := -fsanitize=address,undefined

.PHONY: san
san:
	@$(MAKE) --no-print-directory walk BUILD=$(BUILD)/san \
	    CFLAGS="$(SAN_CFLAGS)" LDFLAGS="$(SAN_LDFLAGS)"

.PHONY: san-check
san-check:
	@$(MAKE) --no-print-directory check BUILD=$(BUILD)/san \
	    CFLAGS="$(SAN_CFLAGS)" LDFLAGS="$(SAN_LDFLAGS)"

.PHONY: projects
projects:
	@$(MAKE) --no-print-directory -C projects

.PHONY: check
check: $(SOLNBIN)
	@./$(SOLNBIN) --all >/dev/null && echo "solutions: all koans pass"

# The koans must COMPILE and then FAIL. Building first is the whole point:
# a compile error would otherwise look identical to "the koans are unsolved",
# which would let a broken generator ship koans that are already answered.
.PHONY: check-unsolved
check-unsolved: $(TARGET)
	@if ./$(TARGET) --no-color >/dev/null 2>&1; then \
	    echo "ERROR: koans/ passed without being filled in"; exit 1; \
	else \
	    echo "koans: compile, and fail as expected"; \
	fi

.PHONY: check-all
check-all: check check-unsolved
	@python3 tools/genkoans.py --check
	@$(MAKE) --no-print-directory -C projects >/dev/null && echo "projects: build"

# Generate compile_commands.json so clangd, VS Code, CLion and Emacs
# understand the project. Pure shell, so the koans keep their zero-dependency
# promise — python3 is only needed by maintainers regenerating koans/.
.PHONY: compiledb
compiledb:
	@printf '[\n' > compile_commands.json
	@first=1; for src in $(CORE_SRCS) $(KOAN_SRCS); do \
	    if [ $$first -eq 0 ]; then printf ',\n' >> compile_commands.json; fi; \
	    first=0; \
	    printf '  {\n    "directory": "%s",\n    "file": "%s",\n    "command": "%s %s -c %s"\n  }' \
	        "$(CURDIR)" "$(CURDIR)/$$src" "$(CC)" "$(CFLAGS)" "$$src" \
	        >> compile_commands.json; \
	done
	@printf '\n]\n' >> compile_commands.json
	@echo "wrote compile_commands.json ($$(grep -c '"file"' compile_commands.json) entries)"

.PHONY: clean
clean:
	@rm -rf $(BUILD) compile_commands.json
	@$(MAKE) --no-print-directory -C projects clean 2>/dev/null || true

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
