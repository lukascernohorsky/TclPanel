#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    Window root;
    Atom current, count, names, utf8;
    char command[128];
    unsigned long value;
    if (dpy == NULL) { fprintf(stderr, "cannot open display\n"); return 2; }
    root = DefaultRootWindow(dpy);
    current = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    count = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    names = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
    utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    value = 1;
    XChangeProperty(dpy, root, current, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&value, 1);
    value = 4;
    XChangeProperty(dpy, root, count, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&value, 1);
    {
        const char labels[] = "One\0Two\0Three\0Four\0";
        XChangeProperty(dpy, root, names, utf8, 8, PropModeReplace, (const unsigned char *)labels, (int)sizeof(labels) - 1);
    }
    XSync(dpy, False);
    puts("ready"); fflush(stdout);
    while (fgets(command, sizeof(command), stdin) != NULL) {
        char *end = NULL;
        command[strcspn(command, "\r\n")] = '\0';
        if (strncmp(command, "current ", 8) == 0 || strncmp(command, "count ", 6) == 0) {
            int is_current = command[0] == 'c' && command[1] == 'u';
            const char *arg = command + (is_current ? 8 : 6);
            value = strtoul(arg, &end, 10);
            if (end == arg || *end != '\0') { puts("error"); fflush(stdout); continue; }
            XChangeProperty(dpy, root, is_current ? current : count, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&value, 1);
            XSync(dpy, False); puts("ok"); fflush(stdout);
        } else if (strcmp(command, "names") == 0) {
            const char labels[] = "Alpha\0Beta\0Gamma\0";
            XChangeProperty(dpy, root, names, utf8, 8, PropModeReplace, (const unsigned char *)labels, (int)sizeof(labels) - 1);
            XSync(dpy, False); puts("ok"); fflush(stdout);
        } else if (strcmp(command, "quit") == 0) break;
        else { puts("unknown"); fflush(stdout); }
    }
    XCloseDisplay(dpy);
    return 0;
}
