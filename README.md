# TclPanel

Standalone modular X11 desktop panel written in Tcl/Tk with a small C/Xlib extension. No XFCE, GTK, Qt, or libwnck dependency.

## Source

The complete initial source tree is in [dist/tclpanel-0.2-source.tar.gz](dist/tclpanel-0.2-source.tar.gz). It contains Tcl modules, the X11 extension, tests, Debian packaging metadata, and a FreeBSD ports template.

Extract it with:

```sh
tar -xzf tclpanel-0.2-source.tar.gz
cd tclpanel
```

See the extracted README for build requirements, configuration, APIs, packaging notes, and known limitations. This is an initial implementation; the README explicitly documents functionality that still requires runtime/interoperability testing.
