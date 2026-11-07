PREFIX ?= /usr/local
TCLSH ?= tclsh
WISH ?= wish
PKG_CONFIG ?= pkg-config
CURDIR ?= ${.CURDIR}
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
BUILD_DIR = build
EXT = $(BUILD_DIR)/libtclpanelx11.so
TCL_PKGS ?= tcl tk x11 xrandr
.PHONY: all clean run test test-tray test-window-watch test-desktop-watch test-taskbar test-workspace test-monitors test-x11-load install
all: $(EXT)
$(BUILD_DIR):
	mkdir -p $@
$(EXT): c/x11.c $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC -shared `$(PKG_CONFIG) --cflags $(TCL_PKGS)` -o $@ c/x11.c `$(PKG_CONFIG) --libs $(TCL_PKGS)`
test-x11-load: all
	$(TCLSH) tests/x11_load.tcl $(CURDIR)/$(EXT)
run: all
	TCLPANEL_LIB=$(CURDIR)/$(EXT) $(WISH) src/main.tcl
test: all
	$(TCLSH) tests/smoke.tcl
	$(TCLSH) tests/xdg_apps.tcl
	$(TCLSH) tests/modules.tcl
	$(TCLSH) tests/x11_load.tcl $(CURDIR)/$(EXT)
$(BUILD_DIR)/tray-test-client: tests/tray_client.c $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ tests/tray_client.c `$(PKG_CONFIG) --libs x11`
test-tray: all $(BUILD_DIR)/tray-test-client
	@if command -v xvfb-run >/dev/null 2>&1; then xvfb-run -a $(TCLSH) tests/tray_integration.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/tray-test-client; else $(TCLSH) tests/tray_integration.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/tray-test-client; fi
$(BUILD_DIR)/window-watch-client: tests/window_watch_client.c $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ tests/window_watch_client.c `$(PKG_CONFIG) --libs x11`
test-window-watch: all $(BUILD_DIR)/window-watch-client
	@if command -v xvfb-run >/dev/null 2>&1; then xvfb-run -a $(TCLSH) tests/window_watch.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/window-watch-client; else $(TCLSH) tests/window_watch.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/window-watch-client; fi
$(BUILD_DIR)/desktop-watch-client: tests/desktop_watch_client.c $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ tests/desktop_watch_client.c `$(PKG_CONFIG) --libs x11`
test-desktop-watch: all $(BUILD_DIR)/desktop-watch-client
	@if command -v xvfb-run >/dev/null 2>&1; then xvfb-run -a $(TCLSH) tests/desktop_watch.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/desktop-watch-client; else $(TCLSH) tests/desktop_watch.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/desktop-watch-client; fi
test-taskbar:
	@if command -v xvfb-run >/dev/null 2>&1; then xvfb-run -a $(TCLSH) tests/taskbar_integration.tcl; else $(TCLSH) tests/taskbar_integration.tcl; fi
test-workspace:
	@if command -v xvfb-run >/dev/null 2>&1; then xvfb-run -a $(TCLSH) tests/workspace_integration.tcl; else $(TCLSH) tests/workspace_integration.tcl; fi
$(BUILD_DIR)/x11-hold-open: tests/x11_hold_open.c $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ tests/x11_hold_open.c `$(PKG_CONFIG) --cflags --libs x11`
$(BUILD_DIR)/randr-close-probe: tests/randr_close_probe.c $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ tests/randr_close_probe.c `$(PKG_CONFIG) --cflags --libs x11 xrandr`
test-monitors: all $(BUILD_DIR)/randr-close-probe $(BUILD_DIR)/x11-hold-open
	@if command -v xvfb-run >/dev/null 2>&1; then xvfb-run -a -s "-screen 0 1280x1024x24" $(TCLSH) tests/monitor_integration.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/randr-close-probe $(CURDIR)/$(BUILD_DIR)/x11-hold-open; else $(TCLSH) tests/monitor_integration.tcl $(CURDIR)/$(EXT) $(CURDIR)/$(BUILD_DIR)/randr-close-probe $(CURDIR)/$(BUILD_DIR)/x11-hold-open; fi
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
