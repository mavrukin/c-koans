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

# -Wno-reserved-identifier: the `__` blank is a deliberate exception, see koan.h
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wvla \
            -Wno-unused-parameter -Wno-reserved-identifier \
            -Wno-reserved-macro-identifier

CFLAGS  ?= -std=c23 -g -O0 $(WARNINGS) -Iinclude -Ikoans
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

.PHONY: san
san:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory walk \
	    CFLAGS="-std=c23 -g -O1 $(WARNINGS) -Iinclude -Ikoans \
	            -fsanitize=address,undefined -fno-omit-frame-pointer" \
	    LDFLAGS="-fsanitize=address,undefined"

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

.PHONY: clean
clean:
	@rm -rf $(BUILD)
	@$(MAKE) --no-print-directory -C projects clean 2>/dev/null || true

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
