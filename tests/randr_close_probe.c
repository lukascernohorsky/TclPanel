#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    Display *dpy;
    Window root;
    int count = 0;
    XRRMonitorInfo *monitors = NULL;
    const char *mode;

    if (argc != 2) {
        fprintf(stderr, "usage: %s open|select|get|select-get\n", argv[0]);
        return 2;
    }
    mode = argv[1];
    if (strcmp(mode, "open") != 0 &&
        strcmp(mode, "select") != 0 &&
        strcmp(mode, "get") != 0 &&
        strcmp(mode, "select-get") != 0) {
        fprintf(stderr, "unknown probe mode: %s\n", mode);
        return 2;
    }

    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        fprintf(stderr, "cannot open X display\n");
        return 1;
    }
    root = DefaultRootWindow(dpy);

    if (strcmp(mode, "select") == 0 ||
        strcmp(mode, "select-get") == 0) {
        int event_base, error_base;
        if (!XRRQueryExtension(dpy, &event_base, &error_base)) {
            fprintf(stderr, "XRandR extension unavailable\n");
            XCloseDisplay(dpy);
            return 1;
        }
        XRRSelectInput(dpy, root,
            RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask |
            RROutputChangeNotifyMask | RROutputPropertyNotifyMask |
            RRResourceChangeNotifyMask);
        XSync(dpy, False);
    }

    if (strcmp(mode, "get") == 0 ||
        strcmp(mode, "select-get") == 0) {
        monitors = XRRGetMonitors(dpy, root, True, &count);
        if (monitors != NULL)
            XRRFreeMonitors(monitors);
        XSync(dpy, False);
    }

    XCloseDisplay(dpy);
    return 0;
}
