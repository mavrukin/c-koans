# Capstone projects.
#
#   make            build every project
#   make run        build and start the web server on port 8080
#   make clean
#
# These are complete, working programs, not exercises with the middle removed.
# Read them after you finish the koans: everything in them came from one.

CC      ?= cc
BUILD   ?= build
CFLAGS  ?= -std=c23 -g -O2 -Wall -Wextra -Wno-unused-parameter
LDLIBS  ?= -lpthread

SERVER  := $(BUILD)/koanhttpd
CGI     := webserver/www/cgi-bin/env.cgi

.PHONY: all
all: $(SERVER) $(CGI)

$(SERVER): webserver/server.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

# CGI programs are ordinary executables the server runs per request.
$(CGI): webserver/www/cgi-bin/env.cgi.c
	$(CC) $(CFLAGS) -o $@ $<
	@chmod +x $@

.PHONY: run
run: all
	./$(SERVER) --port 8080 --root webserver/www --verbose

.PHONY: clean
clean:
	@rm -rf $(BUILD) $(CGI)
