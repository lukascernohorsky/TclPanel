#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int set_monitor_while_connected(const char *name) {
    pid_t child = fork();
    int status;
    if (child < 0) return 0;
    if (child == 0) {
        execlp("xrandr", "xrandr", "--setmonitor", name,
               "1280/340x1024/270+0+0", "none", (char *)NULL);
        _exit(127);
    }
    if (waitpid(child, &status, 0) != child)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void read_monitors(Display *dpy, Window root) {
    int count = 0;
    XRRMonitorInfo *monitors = XRRGetMonitors(dpy, root, True, &count);
    if (monitors != NULL)
        XRRFreeMonitors(monitors);
    XSync(dpy, False);
}

int main(int argc, char **argv) {
    Display *dpy;
    Window root;
    const char *mode;
    int watch_mode, select_mode, get_mode;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s open|select|get|select-get|watch ?monitor-name?\n", argv[0]);
        return 2;
    }
    mode = argv[1];
    watch_mode = strcmp(mode, "watch") == 0;
    select_mode = strcmp(mode, "select") == 0 ||
                  strcmp(mode, "select-get") == 0 || watch_mode;
    get_mode = strcmp(mode, "get") == 0 ||
               strcmp(mode, "select-get") == 0;
    if (!watch_mode && argc != 2) {
        fprintf(stderr, "unexpected monitor name for mode %s\n", mode);
        return 2;
    }
    if (watch_mode && argc != 3) {
        fprintf(stderr, "watch mode requires a monitor name\n");
        return 2;
    }
    if (!watch_mode && strcmp(mode, "open") != 0 &&
        strcmp(mode, "select") != 0 && strcmp(mode, "get") != 0 &&
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

    if (select_mode) {
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

    if (watch_mode) {
        struct pollfd descriptor;
        int i;
        if (!set_monitor_while_connected(argv[2])) {
            fprintf(stderr, "xrandr could not create monitor %s\n", argv[2]);
            XCloseDisplay(dpy);
            return 1;
        }
        descriptor.fd = ConnectionNumber(dpy);
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        for (i = 0; i < 4; i++) {
            XEvent event;
            (void)poll(&descriptor, 1, 500);
            while (XPending(dpy)) {
                XNextEvent(dpy, &event);
                if (event.type == event_base + RRScreenChangeNotify)
                    XRRUpdateConfiguration(&event);
            }
            read_monitors(dpy, root);
        }
    } else if (get_mode) {
        read_monitors(dpy, root);
    }

    XCloseDisplay(dpy);
    return 0;
}
