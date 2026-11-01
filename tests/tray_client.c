#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    Window parent, icon = None, tray = None;
    Atom opcode, xembed_info;
    unsigned long info[2] = {0, 1};
    XEvent ev;
    XWindowAttributes attr;
    char command[128];

    if (dpy == NULL) {
        fprintf(stderr, "cannot open display\n");
        return 2;
    }
    parent = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 320, 48,
                                 0, 0, 0);
    XMapWindow(dpy, parent);
    XSync(dpy, False);
    printf("%lu\n", (unsigned long)parent);
    fflush(stdout);

    opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    xembed_info = XInternAtom(dpy, "_XEMBED_INFO", False);
    while (fgets(command, sizeof(command), stdin) != NULL) {
        command[strcspn(command, "\r\n")] = '\0';
        if (strncmp(command, "dock ", 5) == 0) {
            tray = (Window)strtoul(command + 5, NULL, 0);
            icon = XCreateSimpleWindow(dpy, parent, 0, 0, 24, 24, 0, 0, 0);
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
        } else if (strcmp(command, "embed") == 0 && icon != None) {
            Atom xembed = XInternAtom(dpy, "_XEMBED", False);
            int found = 0;
            XSync(dpy, False);
            while (XCheckTypedWindowEvent(dpy, icon, ClientMessage, &ev)) {
                if (ev.xclient.message_type == xembed &&
                    ev.xclient.data.l[1] == 0 &&
                    (Window)ev.xclient.data.l[2] == tray)
                    found = 1;
            }
            printf("%d\\n", found);
            fflush(stdout);
        } else if (strcmp(command, "unmap") == 0 && icon != None) {
            info[1] = 0;
            XChangeProperty(dpy, icon, xembed_info, xembed_info, 32,
                            PropModeReplace, (unsigned char *)info, 2);
            XFlush(dpy);
            puts("ok"); fflush(stdout);
        } else if (strcmp(command, "map") == 0 && icon != None) {
            info[1] = 1;
            XChangeProperty(dpy, icon, xembed_info, xembed_info, 32,
                            PropModeReplace, (unsigned char *)info, 2);
            XFlush(dpy);
            puts("ok"); fflush(stdout);
        } else if (strcmp(command, "state") == 0 && icon != None) {
            XSync(dpy, False);
            if (!XGetWindowAttributes(dpy, icon, &attr)) return 3;
            printf("%d\n", attr.map_state);
            fflush(stdout);
        } else if (strcmp(command, "destroy") == 0 && icon != None) {
            XDestroyWindow(dpy, icon);
            XFlush(dpy);
            puts("done"); fflush(stdout);
            icon = None;
        } else if (strcmp(command, "quit") == 0) {
            break;
        } else {
            puts("unknown"); fflush(stdout);
        }
    }
    if (icon != None) XDestroyWindow(dpy, icon);
    XDestroyWindow(dpy, parent);
    XCloseDisplay(dpy);
    return 0;
}
