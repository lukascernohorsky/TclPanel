PREFIX ?= /usr/local
TCLSH ?= tclsh
WISH ?= wish
PKG_CONFIG ?= pkg-config
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
BUILD_DIR = build
EXT = $(BUILD_DIR)/libtclpanelx11.so
TCL_PKGS ?= tcl tk x11 xrandr
TCL_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(TCL_PKGS) 2>/dev/null)
TCL_LIBS := $(shell $(PKG_CONFIG) --libs $(TCL_PKGS) 2>/dev/null)
.PHONY: all clean run test test-tray test-window-watch test-desktop-watch install
all: $(EXT)
$(BUILD_DIR):
	mkdir -p $@
$(EXT): c/x11.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC -shared $(TCL_CFLAGS) -o $@ $< $(TCL_LIBS)
run: all
	TCLPANEL_LIB=$(CURDIR)/$(EXT) $(WISH) src/main.tcl
test:
	$(TCLSH) tests/smoke.tcl
	$(TCLSH) tests/xdg_apps.tcl
	$(TCLSH) tests/modules.tcl
$(BUILD_DIR)/tray-test-client: tests/tray_client.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(shell $(PKG_CONFIG) --libs x11)
test-tray: all $(BUILD_DIR)/tray-test-client
	xvfb-run -a $(TCLSH) tests/tray_integration.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/tray-test-client
$(BUILD_DIR)/window-watch-client: tests/window_watch_client.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(shell $(PKG_CONFIG) --libs x11)
test-window-watch: all $(BUILD_DIR)/window-watch-client
	xvfb-run -a $(TCLSH) tests/window_watch.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/window-watch-client
$(BUILD_DIR)/desktop-watch-client: tests/desktop_watch_client.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(shell $(PKG_CONFIG) --libs x11)
test-desktop-watch: all $(BUILD_DIR)/desktop-watch-client
	xvfb-run -a $(TCLSH) tests/desktop_watch.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/desktop-watch-client
install: all
	install -d $(DESTDIR)$(PREFIX)/bin \
		$(DESTDIR)$(PREFIX)/share/tclpanel/src/modules \
		$(DESTDIR)$(PREFIX)/share/tclpanel/src \
		$(DESTDIR)$(PREFIX)/lib/tclpanel
	sed 's|@PREFIX@|$(PREFIX)|g' bin/tclpanel > $(DESTDIR)$(PREFIX)/bin/tclpanel
	chmod 755 $(DESTDIR)$(PREFIX)/bin/tclpanel
	install -m 644 src/*.tcl $(DESTDIR)$(PREFIX)/share/tclpanel/src/
	install -m 644 src/modules/*.tcl $(DESTDIR)$(PREFIX)/share/tclpanel/src/modules/
	install -m 755 $(EXT) $(DESTDIR)$(PREFIX)/lib/tclpanel/
clean:
	rm -rf $(BUILD_DIR)
