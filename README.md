# TclPanel

TclPanel is a standalone X11 desktop panel implemented in Tcl/Tk. It is intended to work with EWMH-compatible window managers on FreeBSD and Linux without XFCE, GTK, Qt, or libwnck.

## Build and run

Requirements: Tcl/Tk 8.6+, X11 development headers and libraries, a C compiler, Make, and pkg-config.

\`\`\`sh
make
make test
make run
\`\`\`

Install with \`make install PREFIX=/usr/local\`, then start with \`tclpanel\`. \`DESTDIR\` is supported for staged package builds.

## Current implementation

- Tcl/Tk panel shell, user Tcl configuration and loadable module structure.
- Xlib extension with EWMH client-window enumeration, activation, desktop count/current desktop/switching, and dock/strut properties.
- Taskbar and workspace switcher use the Xlib extension when loaded; taskbar/workspace retain limited \`wmctrl\` fallback.
- XDG application discovery from standard data directories, parsing of Application desktop entries, visibility filtering, and shell-free Exec argument expansion.
- Tcl tests for configuration, module loading, XDG discovery, entry filtering, quoting, and Exec field-code handling.

## Testing

\`make test\` runs display-independent Tcl tests without an X server. GitHub Actions compiles the Xlib extension on Ubuntu and runs the Tcl test suite.

## Known limitations

The XDG implementation is a practical Desktop Entry subset, not the complete freedesktop menu specification. It does not merge \`.menu\` files, resolve all localized keys, handle MIME actions or launch targets, or provide terminal emulation for \`Terminal=true\` entries. File/URL launch targets are not yet supported.

The Xlib layer targets a single X screen and EWMH-compatible window managers. Event-driven updates, monitor-specific struts, XRandR hotplug, XEmbed system tray, and platform-specific system monitoring and power management are not implemented. No live-X-server or FreeBSD build result is claimed.
