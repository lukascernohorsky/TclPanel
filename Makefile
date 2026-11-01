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
.PHONY: all clean run test install
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
