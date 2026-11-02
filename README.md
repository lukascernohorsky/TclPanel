# TclPanel

TclPanel is a standalone X11 desktop panel implemented in Tcl/Tk. It is intended to work with EWMH-compatible window managers on FreeBSD and Linux without XFCE, GTK, Qt, or libwnck.

## Build and run

Requirements: Tcl/Tk 8.6+, X11 and XRandR development headers and libraries, a C compiler, Make, and pkg-config. On Debian/Ubuntu install `tcl-dev tk-dev libx11-dev libxrandr-dev pkg-config`. On FreeBSD 15.0 install `tcl86 tk86 libXrandr pkgconf` and build with `make TCLSH=tclsh8.6 TCL_PKGS="tcl86 tk86 x11 xrandr"`.

\`\`\`sh
make
make test
make run
\`\`\`

Install with \`make install PREFIX=/usr/local\`, then start with \`tclpanel\`. \`DESTDIR\` is supported for staged package builds.

## Current implementation

- Tcl/Tk panel shell, user Tcl configuration (including monitor index/name selection) and loadable module structure.
- Xlib extension with EWMH client-window enumeration, active-window lookup/notifications (`::x11::active_window`, `::x11::watch_windows`), desktop count/current desktop/names/switching (`::x11::desktop_names`, `::x11::watch_desktops`), dock/strut properties, XRandR monitor enumeration (`::x11::monitors`), and event-driven monitor-change notifications (`::x11::watch_monitors`).
- Taskbar and workspace switcher use the Xlib extension when loaded; taskbar/workspace retain limited \`wmctrl\` fallback.
- XDG application discovery from standard data directories, parsing of Application desktop entries, visibility filtering, shell-free Exec argument expansion, themed icon lookup, and category-grouped launcher menus.
- Tcl tests for configuration, module loading, XDG discovery, entry filtering, quoting, and Exec field-code handling.

## Testing

\`make test\` runs display-independent Tcl tests without an X server. GitHub Actions compiles the Xlib extension on Ubuntu and runs the Tcl test suite.

## Known limitations

The XDG implementation is a practical Desktop Entry subset, not the complete freedesktop menu specification. It does not merge \`.menu\` files, resolve all localized keys, handle MIME actions or launch targets, or provide terminal emulation for \`Terminal=true\` entries. File/URL launch targets are not yet supported.

The Xlib layer targets EWMH-compatible window managers. Multi-monitor hotplug integration coverage, complete XDG menu-specification support, and platform-specific system monitoring and power management remain future work. The system tray currently implements horizontal XEmbed layout; vertical panel integration and full XEmbed focus navigation are not yet implemented. Linux CI exercises the tray protocol under Xvfb. FreeBSD 15.0 CI currently validates compilation, Tcl loading, and display-independent tests; live desktop-session compatibility is not yet claimed.


### System tray

The `tray` module implements the freedesktop System Tray selection and XEmbed docking path through the X11 extension. It claims `_NET_SYSTEM_TRAY_Sn`, announces the manager, accepts dock requests, reparents client windows into its native X11 container, and tracks icon destruction. A running window manager or desktop environment that already owns the system tray selection must be configured not to provide a competing tray. The tray uses a horizontal layout with configurable icon size (`tray_icon_size`, where `0` means automatic panel-height sizing) and inter-icon spacing (`tray_icon_gap`). Icons are centered vertically and kept square; icon size is clamped to the panel height.


### System tray integration testing

The Linux CI workflow runs an XEmbed protocol integration test under Xvfb. The test creates a separate X11 client and verifies dock requests, the `XEMBED_EMBEDDED_NOTIFY` message, `_XEMBED_INFO` mapped-state transitions, icon destruction, and Tcl change callbacks. Run it locally with `make test-tray` after installing Xvfb and the X11/Tcl development dependencies.
