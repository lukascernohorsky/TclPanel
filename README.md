# TclPanel

TclPanel is a standalone X11 desktop panel implemented in Tcl/Tk. It is intended to work with independent EWMH-compatible window managers on FreeBSD and Linux and does not depend on XFCE, GTK, Qt, or libwnck.

## Build and run

Requirements: Tcl/Tk 8.6+, X11 development headers and libraries, a C compiler, Make, and pkg-config.

```sh
make
make test
make run
```

The launcher uses `wish`; `TCLSH` is used only for non-GUI tests. Override `WISH`, `TCLSH`, `CC`, `PKG_CONFIG`, or `TCL_PKGS` when required by the platform packaging.

Install with `make install PREFIX=/usr/local`, then start with `tclpanel`. `DESTDIR` is supported for staged package builds.

On Debian/Ubuntu install Tcl/Tk and X11 development packages (for example `tcl-dev tk-dev libx11-dev pkg-config`). On FreeBSD install the Tcl/Tk and Xorg development packages appropriate to the installed Tcl/Tk version.

## Configuration

The first run creates `~/.config/tclpanel/config.tcl`. It is executable Tcl code; only use a configuration file you control.

## Status and limitations

This is an early source implementation, not a feature-complete replacement for xfce4-panel. The task list and workspace module use `wmctrl` when available. The C extension publishes a basic full-width top/bottom EWMH strut and dock window properties. It does not implement partial struts, monitor-specific geometry, XRandR hotplug/multi-monitor placement, XEmbed system-tray management, full XDG desktop-file menu parsing, platform power-policy integration, or FreeBSD/Linux-specific system monitoring.

No runtime test against a live X server or multiple window managers is claimed.
