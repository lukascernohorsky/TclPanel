#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int property(Display *d, Window w, const char *name, Atom type,
                    unsigned long wanted, unsigned long *out) {
    Atom actual = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char *data = NULL;
    int ok = 0;
    if (XGetWindowProperty(d, w, XInternAtom(d, name, False), 0, 64, False,
            type, &actual, &format, &count, &remaining, &data) == Success &&
        data && actual == type && format == 32 && count == wanted &&
        remaining == 0) {
        memcpy(out, data, count * sizeof(unsigned long));
        ok = 1;
    }
    if (data) XFree(data);
    return ok;
}
static int fail(const char *s) { fprintf(stderr, "FAIL: %s\n", s); return 1; }

int main(int argc, char **argv) {
    Display *d;
    char *end;
    unsigned long id, thickness, first, last;
    unsigned long partial[12] = {0}, basic[4] = {0}, type[1] = {0}, state[4] = {0};
    unsigned long ep[12] = {0}, eb[4] = {0};
    Atom dock, states[4];
    int i;
    if (argc != 6) return fail("usage: probe window-id top|bottom thickness x-start x-end");
    errno = 0; id = strtoul(argv[1], &end, 0);
    if (errno || end == argv[1] || *end) return fail("invalid window id");
    thickness = strtoul(argv[3], &end, 10);
    if (end == argv[3] || *end) return fail("invalid thickness");
    first = strtoul(argv[4], &end, 10);
    if (end == argv[4] || *end) return fail("invalid x-start");
    last = strtoul(argv[5], &end, 10);
    if (end == argv[5] || *end) return fail("invalid x-end");
    if (strcmp(argv[2], "top") && strcmp(argv[2], "bottom")) return fail("invalid edge");
    d = XOpenDisplay(NULL);
    if (!d) return fail("cannot open X display");
    if (!strcmp(argv[2], "top")) {
        ep[2] = eb[2] = thickness; ep[8] = first; ep[9] = last;
    } else {
        ep[3] = eb[3] = thickness; ep[10] = first; ep[11] = last;
    }
    if (!property(d, (Window)id, "_NET_WM_STRUT_PARTIAL", XA_CARDINAL, 12, partial) ||
        !property(d, (Window)id, "_NET_WM_STRUT", XA_CARDINAL, 4, basic)) {
        XCloseDisplay(d); return fail("strut properties missing or malformed");
    }
    for (i = 0; i < 12; i++) if (partial[i] != ep[i]) {
        fprintf(stderr, "FAIL: partial[%d]=%lu expected %lu\n", i, partial[i], ep[i]);
        XCloseDisplay(d); return 1;
    }
    for (i = 0; i < 4; i++) if (basic[i] != eb[i]) {
        fprintf(stderr, "FAIL: basic[%d]=%lu expected %lu\n", i, basic[i], eb[i]);
        XCloseDisplay(d); return 1;
    }
    dock = XInternAtom(d, "_NET_WM_WINDOW_TYPE_DOCK", False);
    if (!property(d, (Window)id, "_NET_WM_WINDOW_TYPE", XA_ATOM, 1, type) || type[0] != dock) {
        XCloseDisplay(d); return fail("window type is not DOCK");
    }
    states[0] = XInternAtom(d, "_NET_WM_STATE_ABOVE", False);
    states[1] = XInternAtom(d, "_NET_WM_STATE_STICKY", False);
    states[2] = XInternAtom(d, "_NET_WM_STATE_SKIP_TASKBAR", False);
    states[3] = XInternAtom(d, "_NET_WM_STATE_SKIP_PAGER", False);
    if (!property(d, (Window)id, "_NET_WM_STATE", XA_ATOM, 4, state) ||
        memcmp(state, states, sizeof(states))) {
        XCloseDisplay(d); return fail("dock state atoms are missing or incorrect");
    }
    XCloseDisplay(d);
    puts("PASS: EWMH strut properties match expected values");
    return 0;
}
