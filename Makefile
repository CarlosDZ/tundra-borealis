CC = gcc
PKG_CONFIG ?= pkg-config
VERSION = $(shell grep '^version=' metapkg | cut -d= -f2)

TUNDRA_PKGS = wlroots-0.20 wayland-server xkbcommon libinput pixman-1

TUNDRA_BOREALIS_DIR ?= /etc/tundra/borealis

TUNDRA_STD  = -std=c11
TUNDRA_DEFS = -D_GNU_SOURCE -DWLR_USE_UNSTABLE \
              -DTUNDRA_BOREALIS_VERSION=\"$(VERSION)\" \
              -DTUNDRA_BOREALIS_DIR=\"$(TUNDRA_BOREALIS_DIR)\"
TUNDRA_INC  = -Isrc
TUNDRA_LIBS = -ltundra-draw -ltundra-conf
TUNDRA_WL_CFLAGS = $(shell $(PKG_CONFIG) --cflags $(TUNDRA_PKGS))
TUNDRA_WL_LIBS   = $(shell $(PKG_CONFIG) --libs $(TUNDRA_PKGS))

CONF_DEV ?= $(wildcard ../tundra-libconf)
DRAW_DEV ?= $(wildcard ../tundra-libdraw)
TOA_DEV ?= $(wildcard ../tundra-libtoa)
DEV_BUILDS   = $(foreach d,$(CONF_DEV) $(DRAW_DEV) $(TOA_DEV),$(abspath $(d))/build)
TUNDRA_INC  += $(foreach d,$(CONF_DEV) $(DRAW_DEV),-I$(d)/include)
TUNDRA_LIBS := $(DEV_BUILDS:%=-L%) $(TUNDRA_LIBS)
TEST_RPATH   = $(DEV_BUILDS:%=-Wl,-rpath,%)
empty :=
TEST_ENV = $(if $(DEV_BUILDS),LD_LIBRARY_PATH=$(subst $(empty) ,:,$(strip $(DEV_BUILDS))))

CFLAGS ?= -Wall -Wextra -O2

TARGET = tundra-borealis

BUILD = build
SRCS = $(wildcard src/*.c)
HDRS = $(wildcard src/*.h)
OBJS = $(SRCS:src/%.c=$(BUILD)/%.o)
DEPS = $(OBJS:.o=.d)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(TUNDRA_LIBS) $(TUNDRA_WL_LIBS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(TUNDRA_STD) $(CPPFLAGS) $(CFLAGS) $(TUNDRA_INC) $(TUNDRA_WL_CFLAGS) \
		$(TUNDRA_DEFS) -MMD -MP -c $< -o $@

-include $(DEPS)

$(OBJS): Makefile metapkg

clean:
	rm -rf $(BUILD) $(TARGET)

TEST_BUILD = $(BUILD)/test
TEST_BIN = $(TEST_BUILD)/tundra-borealis
TEST_TOOLS = $(patsubst tests/tools/%.c,$(TEST_BUILD)/tools/%,$(wildcard tests/tools/*.c))
TEST_DEFS = -D_GNU_SOURCE -DWLR_USE_UNSTABLE -DTUNDRA_BOREALIS_TEST \
            -DTUNDRA_BOREALIS_VERSION=\"$(VERSION)-test\" \
            -DTUNDRA_BOREALIS_DIR=\"$(TUNDRA_BOREALIS_DIR)\"

$(TEST_BIN): FORCE
	@mkdir -p $(dir $@)
	$(CC) $(TUNDRA_STD) $(CFLAGS) $(TUNDRA_INC) $(TUNDRA_WL_CFLAGS) $(TEST_DEFS) \
		-o $@ $(SRCS) $(TUNDRA_LIBS) $(TUNDRA_WL_LIBS) $(TEST_RPATH)

WAYLAND_SCANNER ?= wayland-scanner
TEST_PROTO = $(TEST_BUILD)/protocols
XDG_SHELL_XML = $(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml
TOOL_CFLAGS = $(shell $(PKG_CONFIG) --cflags wayland-client)
TOOL_LIBS = $(shell $(PKG_CONFIG) --libs wayland-client)

$(TEST_PROTO)/xdg-shell-client-protocol.h:
	@mkdir -p $(dir $@)
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_XML) $@

$(TEST_PROTO)/xdg-shell-protocol.c:
	@mkdir -p $(dir $@)
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_XML) $@

$(TEST_BUILD)/tools/client: tests/tools/client.c \
		$(TEST_PROTO)/xdg-shell-client-protocol.h $(TEST_PROTO)/xdg-shell-protocol.c
	@mkdir -p $(dir $@)
	$(CC) $(TUNDRA_STD) $(CFLAGS) -I$(TEST_PROTO) $(TOOL_CFLAGS) -o $@ $< \
		$(TEST_PROTO)/xdg-shell-protocol.c $(TOOL_LIBS)

$(TEST_BUILD)/tools/%: tests/tools/%.c
	@mkdir -p $(dir $@)
	$(CC) $(TUNDRA_STD) $(CFLAGS) -D_GNU_SOURCE -o $@ $<

UNIT_FLAGS = -g -fsanitize=address,undefined -fno-omit-frame-pointer
UNIT_TESTS = $(patsubst tests/unit/%.c,$(TEST_BUILD)/unit/%,$(wildcard tests/unit/*.c))

$(TEST_BUILD)/unit/%: tests/unit/%.c tests/unit/test.h $(SRCS) $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(TUNDRA_STD) $(CFLAGS) $(UNIT_FLAGS) $(TUNDRA_INC) -Itests/unit \
		$(TEST_DEFS) -o $@ $< $(TUNDRA_LIBS) $(TEST_RPATH)

check-unit: $(UNIT_TESTS)
	@rc=0; for t in $(UNIT_TESTS); do echo "# $$t"; $(TEST_ENV) $$t || rc=1; done; \
		exit $$rc

check: check-unit $(TEST_BIN) $(TEST_TOOLS)
	@$(TEST_ENV) sh tests/wl/run.sh

FORCE:

.PHONY: install uninstall clean check check-unit FORCE

PREFIX ?= /usr
DESTDIR ?=

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/$(TARGET)/LICENSE

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -rf $(DESTDIR)$(PREFIX)/share/licenses/$(TARGET)
