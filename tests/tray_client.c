#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    Window parent, icon = None, icon2 = None, tray = None, takeover = None;
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
        } else if (strncmp(command, "dock2 ", 6) == 0) {
            tray = (Window)strtoul(command + 6, NULL, 0);
            icon2 = XCreateSimpleWindow(dpy, parent, 0, 0, 24, 24, 0, 0, 0);
            XSelectInput(dpy, icon2, StructureNotifyMask | PropertyChangeMask);
            XChangeProperty(dpy, icon2, xembed_info, xembed_info, 32,
                            PropModeReplace, (unsigned char *)info, 2);
            XMapWindow(dpy, icon2);
            memset(&ev, 0, sizeof(ev));
            ev.xclient.type = ClientMessage;
            ev.xclient.window = tray;
            ev.xclient.message_type = opcode;
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = CurrentTime;
            ev.xclient.data.l[1] = 0;
            ev.xclient.data.l[2] = (long)icon2;
            XSendEvent(dpy, tray, False, NoEventMask, &ev);
            XFlush(dpy);
            printf("%lu\n", (unsigned long)icon2);
            fflush(stdout);
        } else if (strcmp(command, "redock") == 0 && icon != None && tray != None) {
            memset(&ev, 0, sizeof(ev));
            ev.xclient.type = ClientMessage;
            ev.xclient.window = tray;
            ev.xclient.message_type = opcode;
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = CurrentTime;
            ev.xclient.data.l[1] = 0;
            ev.xclient.data.l[2] = (long)icon;
            XSendEvent(dpy, tray, False, NoEventMask, &ev);
            XFlush(dpy);
            puts("ok"); fflush(stdout);
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
            printf("%d\n", found);
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
        } else if (strcmp(command, "geometry") == 0 && icon != None) {
            XSync(dpy, False);
            if (!XGetWindowAttributes(dpy, icon, &attr)) return 3;
            {
                Window child;
                int x, y;
                XTranslateCoordinates(dpy, icon, tray, 0, 0, &x, &y, &child);
                printf("%d %d %d %d\n", x, y, attr.width, attr.height);
                fflush(stdout);
            }
        } else if (strcmp(command, "geometry2") == 0 && icon2 != None) {
            XSync(dpy, False);
            if (!XGetWindowAttributes(dpy, icon2, &attr)) return 3;
            {
                Window child;
                int x, y;
                XTranslateCoordinates(dpy, icon2, tray, 0, 0, &x, &y, &child);
                printf("%d %d %d %d\n", x, y, attr.width, attr.height);
                fflush(stdout);
            }
        } else if (strcmp(command, "destroy") == 0 && icon != None) {
            XDestroyWindow(dpy, icon);
            XFlush(dpy);
            puts("done"); fflush(stdout);
            icon = None;
        } else if (strcmp(command, "destroy2") == 0 && icon2 != None) {
            XDestroyWindow(dpy, icon2);
            XFlush(dpy);
            puts("done"); fflush(stdout);
            icon2 = None;
        } else if (strcmp(command, "destroyparent") == 0 && parent != None) {
            XDestroyWindow(dpy, parent);
            XFlush(dpy);
            parent = None;
            puts("ok"); fflush(stdout);
        } else if (strcmp(command, "newparent") == 0) {
            parent = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                         320, 48, 0, 0, 0);
            XMapWindow(dpy, parent);
            XSync(dpy, False);
            printf("%lu\n", (unsigned long)parent);
            fflush(stdout);
        } else if (strcmp(command, "steal") == 0) {
            char selection_name[64];
            Atom selection;
            snprintf(selection_name, sizeof(selection_name), "_NET_SYSTEM_TRAY_S%d", DefaultScreen(dpy));
            selection = XInternAtom(dpy, selection_name, False);
            takeover = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
            XSetSelectionOwner(dpy, selection, takeover, CurrentTime);
            XSync(dpy, False);
            puts("ok"); fflush(stdout);
        } else if (strcmp(command, "quit") == 0) {
            break;
        } else {
            puts("unknown"); fflush(stdout);
        }
    }
    if (icon != None) XDestroyWindow(dpy, icon);
    if (icon2 != None) XDestroyWindow(dpy, icon2);
    if (takeover != None) {
        char selection_name[64];
        Atom selection;
        snprintf(selection_name, sizeof(selection_name), "_NET_SYSTEM_TRAY_S%d", DefaultScreen(dpy));
        selection = XInternAtom(dpy, selection_name, False);
        if (XGetSelectionOwner(dpy, selection) == takeover)
            XSetSelectionOwner(dpy, selection, None, CurrentTime);
        XDestroyWindow(dpy, takeover);
    }
    XDestroyWindow(dpy, parent);
    XCloseDisplay(dpy);
    return 0;
}
