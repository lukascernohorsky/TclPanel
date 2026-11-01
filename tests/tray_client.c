#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    Display *dpy;
    Window tray, icon;
    Atom opcode, xembed_info;
    unsigned long info[2] = {0, 1};
    XEvent ev;
    char command[64];

    if (argc != 2) {
        fprintf(stderr, "usage: %s tray-window-id\n", argv[0]);
        return 2;
    }
    tray = (Window)strtoul(argv[1], NULL, 0);
    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        fprintf(stderr, "cannot open display\n");
        return 2;
    }
    icon = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 24, 24,
                               0, 0, 0);
    xembed_info = XInternAtom(dpy, "_XEMBED_INFO", False);
    opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    XSelectInput(dpy, icon, StructureNotifyMask | PropertyChangeMask);
    XChangeProperty(dpy, icon, xembed_info, xembed_info, 32,
                    PropModeReplace, (unsigned char *)info, 2);
    XMapWindow(dpy, icon);
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = tray;
    ev.xclient.message_type = opcode;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = 0; /* SYSTEM_TRAY_REQUEST_DOCK */
    ev.xclient.data.l[2] = (long)icon;
    XSendEvent(dpy, tray, False, NoEventMask, &ev);
    XFlush(dpy);
    printf("%lu\n", (unsigned long)icon);
    fflush(stdout);

    while (fgets(command, sizeof(command), stdin) != NULL) {
        command[strcspn(command, "\r\n")] = '\0';
        if (strcmp(command, "unmap") == 0) {
            info[1] = 0;
            XChangeProperty(dpy, icon, xembed_info, xembed_info, 32,
                            PropModeReplace, (unsigned char *)info, 2);
            XFlush(dpy);
            puts("ok"); fflush(stdout);
        } else if (strcmp(command, "map") == 0) {
            info[1] = 1;
            XChangeProperty(dpy, icon, xembed_info, xembed_info, 32,
                            PropModeReplace, (unsigned char *)info, 2);
            XFlush(dpy);
            puts("ok"); fflush(stdout);
        } else if (strcmp(command, "destroy") == 0) {
            XDestroyWindow(dpy, icon);
            XFlush(dpy);
            puts("done"); fflush(stdout);
            break;
        } else if (strcmp(command, "quit") == 0) {
            break;
        } else {
            puts("unknown"); fflush(stdout);
        }
    }
    XCloseDisplay(dpy);
    return 0;
}
