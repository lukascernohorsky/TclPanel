# TclPanel

TclPanel is a standalone X11 desktop panel implemented in Tcl/Tk. It is intended to work with independent EWMH-compatible window managers on FreeBSD and Linux and does not depend on XFCE, GTK, Qt, or libwnck.

## Current source tree

- `src/`: application startup, panel window, configuration and module loader
- `src/modules/`: clock, launcher, workspace indicator and task-list starter modules
- `c/`: optional Xlib/Tcl extension for setting EWMH dock struts
- `tests/`: lightweight Tcl tests
- `themes/`: default color settings

## Build and run

Requirements: Tcl 8.6+, Tk 8.6+, X11 development headers and libraries, a C compiler, Make, and pkg-config.

```sh
make
make test
make run
```

The C extension is optional. Without it the panel runs, but it cannot publish the EWMH strut and window managers may allow maximized windows underneath it.

On Debian/Ubuntu install the Tcl/Tk and X11 development packages (for example `tcl-dev tk-dev libx11-dev pkg-config`). On FreeBSD install the Tcl/Tk and Xorg development packages appropriate to the installed Tcl/Tk version.

## Configuration

The first run creates `~/.config/tclpanel/config.tcl`. It is a Tcl file and is sourced as user configuration. Only edit it if you trust its contents.

## Status and limitations

This repository contains the initial directly browsable source implementation. It is a foundation, not a feature-complete replacement for xfce4-panel. The task list and workspace modules use the optional `wmctrl` command when present. Full XEmbed system-tray management, robust XRandR hotplug/multi-monitor placement, desktop-file menu parsing, power-policy integration and FreeBSD/Linux-specific system monitoring are not implemented yet. The Xlib extension currently publishes a basic full-width top/bottom EWMH strut; it does not implement partial struts or monitor-specific geometry.

No runtime test against a live X server or multiple window managers is claimed.
