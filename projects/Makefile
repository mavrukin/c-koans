# Capstone projects.
#
#   make            build everything
#   make check      run the library self-checks
#   make run        build and start the web server on port 8080
#   make san        run the self-checks under ASan + UBSan
#   make clean
#
# These are complete, working programs, not exercises with the middle removed.
# Read them after you finish the koans: every technique in them came from one.

CC      ?= cc
BUILD   ?= build
CFLAGS  ?= -std=c23 -g -O2 -Wall -Wextra -Wno-unused-parameter
LDLIBS  ?= -lpthread

LIB_SRCS := arena/arena.c containers/containers.c json/json.c
LIB_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(LIB_SRCS))

SERVER := $(BUILD)/koanhttpd
TESTS  := $(BUILD)/tests
CGI    := webserver/www/cgi-bin/env.cgi

.PHONY: all
all: $(SERVER) $(CGI) $(TESTS)

$(SERVER): webserver/server.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

# CGI programs are ordinary executables the server runs per request.
$(CGI): webserver/www/cgi-bin/env.cgi.c
	$(CC) $(CFLAGS) -o $@ $<
	@chmod +x $@

$(TESTS): tests.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

.PHONY: check
check: $(TESTS)
	@./$(TESTS)

.PHONY: san
san:
	@$(MAKE) --no-print-directory check BUILD=$(BUILD)/san \
	    CFLAGS="-std=c23 -g -O1 -Wall -Wextra -Wno-unused-parameter \
	            -fsanitize=address,undefined -fno-omit-frame-pointer" \
	    LDLIBS="-lpthread -fsanitize=address,undefined"

.PHONY: run
run: $(SERVER) $(CGI)
	./$(SERVER) --port 8080 --root webserver/www --verbose

.PHONY: clean
clean:
	@rm -rf $(BUILD) $(CGI)

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
