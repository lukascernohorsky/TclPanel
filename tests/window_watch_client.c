#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
static void set_client_list(Display *dpy, Window root, Atom property, Window *windows, int count) {
    XChangeProperty(dpy, root, property, XA_WINDOW, 32, PropModeReplace, (unsigned char *)windows, count);
    XSync(dpy, False);
}
int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    Window root, win, list[1];
    Atom clients, utf8, net_name;
    char command[128];
    if (dpy == NULL) { fprintf(stderr, "cannot open display\n"); return 2; }
    root = DefaultRootWindow(dpy);
    clients = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    net_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    win = XCreateSimpleWindow(dpy, root, 10, 10, 160, 80, 0, 0, 0);
    XStoreName(dpy, win, "initial title");
    {
        const char title[] = "initial title";
        XChangeProperty(dpy, win, net_name, utf8, 8, PropModeReplace, (const unsigned char *)title, (int)strlen(title));
    }
    list[0] = win;
    set_client_list(dpy, root, clients, list, 1);
    XMapWindow(dpy, win); XSync(dpy, False);
    printf("%lu\n", (unsigned long)win); fflush(stdout);
    while (fgets(command, sizeof(command), stdin) != NULL) {
        command[strcspn(command, "\r\n")] = '\0';
        if (strncmp(command, "title ", 6) == 0) {
            const char *title = command + 6;
            XStoreName(dpy, win, title);
            XChangeProperty(dpy, win, net_name, utf8, 8, PropModeReplace, (const unsigned char *)title, (int)strlen(title));
            XSync(dpy, False); puts("ok"); fflush(stdout);
        } else if (strcmp(command, "unmap") == 0) {
            XUnmapWindow(dpy, win); XSync(dpy, False); puts("ok"); fflush(stdout);
        } else if (strcmp(command, "map") == 0) {
            XMapWindow(dpy, win); XSync(dpy, False); puts("ok"); fflush(stdout);
        } else if (strcmp(command, "remove") == 0) {
            set_client_list(dpy, root, clients, NULL, 0); puts("ok"); fflush(stdout);
        } else if (strcmp(command, "destroy") == 0) {
            XDestroyWindow(dpy, win); XSync(dpy, False); win = None; puts("ok"); fflush(stdout);
        } else if (strcmp(command, "quit") == 0) break;
        else { puts("unknown"); fflush(stdout); }
    }
    if (win != None) XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
