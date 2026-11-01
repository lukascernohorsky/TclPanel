# TclPanel

TclPanel is a standalone X11 desktop panel implemented in Tcl/Tk. It is intended to work with EWMH-compatible window managers on FreeBSD and Linux without XFCE, GTK, Qt, or libwnck.

## Build and run

Requirements: Tcl/Tk 8.6+, X11 development headers and libraries, a C compiler, Make, and pkg-config.

```sh
make
make test
make run
```

Install with `make install PREFIX=/usr/local`, then start with `tclpanel`. `DESTDIR` is supported for staged package builds.

## Current implementation

- Tcl/Tk panel shell, user Tcl configuration and loadable module structure.
- Xlib extension with EWMH client-window enumeration, activation, desktop count/current desktop/switching, and dock/strut properties.
- Taskbar and workspace switcher use the Xlib extension when loaded; taskbar/workspace retain limited `wmctrl` fallback.
- Basic launcher currently contains a small fixed application list.

## Known limitations

The Xlib layer currently targets a single X screen and EWMH-compatible window managers. It does not yet implement event-driven updates, monitor-specific struts, XRandR hotplug, XEmbed system tray, full XDG .desktop menu discovery, or platform-specific system monitoring and power management. EWMH support varies by window manager.

The C extension and GUI must be built and tested on the target host; no live-X-server or FreeBSD build result is claimed here.
