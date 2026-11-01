#include <tcl.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Display *open_display(Tcl_Interp *ip) {
    Display *dpy = XOpenDisplay(NULL);
    if (dpy == NULL)
        Tcl_SetObjResult(ip, Tcl_NewStringObj("cannot open X display", -1));
    return dpy;
}
static Atom atom(Display *dpy, const char *name) {
    return XInternAtom(dpy, name, False);
}
static int get_cardinal(Display *dpy, Window win, const char *name,
                        unsigned long *value) {
    Atom actual;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    int ok = 0;
    if (XGetWindowProperty(dpy, win, atom(dpy, name), 0, 1, False,
                           XA_CARDINAL, &actual, &format, &nitems, &after,
                           &data) == Success && data != NULL &&
        actual == XA_CARDINAL && format == 32 && nitems == 1) {
        *value = *(unsigned long *)data;
        ok = 1;
    }
    if (data != NULL) XFree(data);
    return ok;
}
static Tcl_Obj *window_title(Display *dpy, Window win) {
    Atom utf8 = atom(dpy, "UTF8_STRING"), actual;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    Tcl_Obj *result = NULL;
    if (XGetWindowProperty(dpy, win, atom(dpy, "_NET_WM_NAME"), 0, 4096,
                           False, utf8, &actual, &format, &nitems, &after,
                           &data) == Success && data != NULL &&
        actual == utf8 && format == 8)
        result = Tcl_NewStringObj((const char *)data, (int)nitems);
    if (data != NULL) XFree(data);
    if (result == NULL) {
        char *name = NULL;
        if (XFetchName(dpy, win, &name) && name != NULL) {
            result = Tcl_NewStringObj(name, -1);
            XFree(name);
        }
    }
    if (result == NULL) result = Tcl_NewStringObj("(untitled)", -1);
    return result;
}
static int has_atom(Display *dpy, Window win, const char *property,
                    const char *wanted) {
    Atom actual;
    int format;
    unsigned long nitems, after, i;
    unsigned char *data = NULL;
    int found = 0;
    if (XGetWindowProperty(dpy, win, atom(dpy, property), 0, 64, False,
                           XA_ATOM, &actual, &format, &nitems, &after,
                           &data) == Success && data != NULL &&
        actual == XA_ATOM && format == 32) {
        Atom *items = (Atom *)data, target = atom(dpy, wanted);
        for (i = 0; i < nitems; i++)
            if (items[i] == target) { found = 1; break; }
    }
    if (data != NULL) XFree(data);
    return found;
}
static int WindowsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                      Tcl_Obj *const objv[]) {
    Display *dpy;
    Window root;
    Atom actual;
    int format;
    unsigned long nitems, after, i;
    unsigned char *data = NULL;
    Tcl_Obj *result;
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    dpy = open_display(ip);
    if (dpy == NULL) return TCL_ERROR;
    root = DefaultRootWindow(dpy);
    if (XGetWindowProperty(dpy, root, atom(dpy, "_NET_CLIENT_LIST"), 0,
                           65536, False, XA_WINDOW, &actual, &format,
                           &nitems, &after, &data) != Success ||
        data == NULL || actual != XA_WINDOW || format != 32) {
        if (data != NULL) XFree(data);
        XCloseDisplay(dpy);
        Tcl_SetObjResult(ip, Tcl_NewStringObj("EWMH _NET_CLIENT_LIST unavailable", -1));
        return TCL_ERROR;
    }
    result = Tcl_NewListObj(0, NULL);
    for (i = 0; i < nitems; i++) {
        Window win = ((Window *)data)[i];
        unsigned long desktop = 0;
        Tcl_Obj *entry;
        if (has_atom(dpy, win, "_NET_WM_STATE", "_NET_WM_STATE_SKIP_TASKBAR") ||
            has_atom(dpy, win, "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DOCK") ||
            has_atom(dpy, win, "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DESKTOP"))
            continue;
        (void)get_cardinal(dpy, win, "_NET_WM_DESKTOP", &desktop);
        entry = Tcl_NewListObj(0, NULL);
        Tcl_ListObjAppendElement(ip, entry, Tcl_NewWideIntObj((Tcl_WideInt)win));
        Tcl_ListObjAppendElement(ip, entry, window_title(dpy, win));
        Tcl_ListObjAppendElement(ip, entry, Tcl_NewWideIntObj((Tcl_WideInt)desktop));
        Tcl_ListObjAppendElement(ip, result, entry);
    }
    XFree(data);
    XCloseDisplay(dpy);
    Tcl_SetObjResult(ip, result);
    return TCL_OK;
}
static int send_root_message(Display *dpy, const char *name,
                             long d0, long d1, long d2, long d3, long d4) {
    XEvent ev;
    Window root = DefaultRootWindow(dpy);
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = root;
    ev.xclient.message_type = atom(dpy, name);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0; ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2; ev.xclient.data.l[3] = d3;
    ev.xclient.data.l[4] = d4;
    return XSendEvent(dpy, root, False,
                      SubstructureRedirectMask | SubstructureNotifyMask, &ev);
}
static int ActivateCmd(ClientData cd, Tcl_Interp *ip, int objc,
                       Tcl_Obj *const objv[]) {
    Display *dpy;
    Tcl_WideInt id;
    XEvent ev;
    (void)cd;
    if (objc != 2) { Tcl_WrongNumArgs(ip, 1, objv, "windowId"); return TCL_ERROR; }
    if (Tcl_GetWideIntFromObj(ip, objv[1], &id) != TCL_OK || id <= 0)
        return TCL_ERROR;
    dpy = open_display(ip);
    if (dpy == NULL) return TCL_ERROR;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = (Window)id;
    ev.xclient.message_type = atom(dpy, "_NET_ACTIVE_WINDOW");
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 2;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(dpy);
    XCloseDisplay(dpy);
    return TCL_OK;
}
static int DesktopsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                       Tcl_Obj *const objv[]) {
    Display *dpy;
    unsigned long count = 1;
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    dpy = open_display(ip); if (dpy == NULL) return TCL_ERROR;
    (void)get_cardinal(dpy, DefaultRootWindow(dpy), "_NET_NUMBER_OF_DESKTOPS", &count);
    XCloseDisplay(dpy);
    Tcl_SetObjResult(ip, Tcl_NewWideIntObj((Tcl_WideInt)count));
    return TCL_OK;
}
static int CurrentDesktopCmd(ClientData cd, Tcl_Interp *ip, int objc,
                             Tcl_Obj *const objv[]) {
    Display *dpy;
    unsigned long current = 0;
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    dpy = open_display(ip); if (dpy == NULL) return TCL_ERROR;
    (void)get_cardinal(dpy, DefaultRootWindow(dpy), "_NET_CURRENT_DESKTOP", &current);
    XCloseDisplay(dpy);
    Tcl_SetObjResult(ip, Tcl_NewWideIntObj((Tcl_WideInt)current));
    return TCL_OK;
}
static int SetDesktopCmd(ClientData cd, Tcl_Interp *ip, int objc,
                         Tcl_Obj *const objv[]) {
    Display *dpy;
    Tcl_WideInt desktop;
    (void)cd;
    if (objc != 2) { Tcl_WrongNumArgs(ip, 1, objv, "desktopIndex"); return TCL_ERROR; }
    if (Tcl_GetWideIntFromObj(ip, objv[1], &desktop) != TCL_OK || desktop < 0)
        return TCL_ERROR;
    dpy = open_display(ip); if (dpy == NULL) return TCL_ERROR;
    send_root_message(dpy, "_NET_CURRENT_DESKTOP", (long)desktop, CurrentTime, 0, 0, 0);
    XFlush(dpy); XCloseDisplay(dpy);
    return TCL_OK;
}
static int StrutCmd(ClientData cd, Tcl_Interp *ip, int objc,
                    Tcl_Obj *const objv[]) {
    Display *dpy;
    Window win;
    Atom partial, basic, type, dock, state, state_values[4];
    unsigned long values[12] = {0};
    const char *edge, *window_text;
    int thickness, width, x_start = 0, x_end;
    char *end = NULL;
    unsigned long id;
    (void)cd;
    if (objc != 4 && objc != 6) { Tcl_WrongNumArgs(ip, 1, objv, "windowId top|bottom thickness ?xStart xEnd?"); return TCL_ERROR; }
    window_text = Tcl_GetString(objv[1]);
    id = strtoul(window_text, &end, 0);
    if (end == window_text || end == NULL || *end != '\0') {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("invalid X11 window id", -1)); return TCL_ERROR;
    }
    edge = Tcl_GetString(objv[2]);
    if (Tcl_GetIntFromObj(ip, objv[3], &thickness) != TCL_OK || thickness < 0)
        return TCL_ERROR;
    if (strcmp(edge, "top") != 0 && strcmp(edge, "bottom") != 0) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("edge must be top or bottom", -1)); return TCL_ERROR;
    }
    dpy = open_display(ip); if (dpy == NULL) return TCL_ERROR;
    width = DisplayWidth(dpy, DefaultScreen(dpy)); win = (Window)id;
    x_end = width - 1;
    if (objc == 6) {
        if (Tcl_GetIntFromObj(ip, objv[4], &x_start) != TCL_OK ||
            Tcl_GetIntFromObj(ip, objv[5], &x_end) != TCL_OK) {
            XCloseDisplay(dpy);
            return TCL_ERROR;
        }
        if (x_start < 0 || x_end < x_start || x_end >= width) {
            XCloseDisplay(dpy);
            Tcl_SetObjResult(ip, Tcl_NewStringObj("strut x range is outside the X screen", -1));
            return TCL_ERROR;
        }
    }
    if (strcmp(edge, "top") == 0) {
        values[2] = (unsigned long)thickness; values[8] = (unsigned long)x_start; values[9] = (unsigned long)x_end;
    } else {
        values[3] = (unsigned long)thickness; values[10] = (unsigned long)x_start; values[11] = (unsigned long)x_end;
    }
    partial = atom(dpy, "_NET_WM_STRUT_PARTIAL"); basic = atom(dpy, "_NET_WM_STRUT");
    XChangeProperty(dpy, win, partial, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)values, 12);
    XChangeProperty(dpy, win, basic, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)values, 4);
    type = atom(dpy, "_NET_WM_WINDOW_TYPE"); dock = atom(dpy, "_NET_WM_WINDOW_TYPE_DOCK");
    XChangeProperty(dpy, win, type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&dock, 1);
    state = atom(dpy, "_NET_WM_STATE");
    state_values[0] = atom(dpy, "_NET_WM_STATE_ABOVE");
    state_values[1] = atom(dpy, "_NET_WM_STATE_STICKY");
    state_values[2] = atom(dpy, "_NET_WM_STATE_SKIP_TASKBAR");
    state_values[3] = atom(dpy, "_NET_WM_STATE_SKIP_PAGER");
    XChangeProperty(dpy, win, state, XA_ATOM, 32, PropModeReplace, (unsigned char *)state_values, 4);
    XFlush(dpy); XCloseDisplay(dpy);
    return TCL_OK;
}



/* Minimal freedesktop System Tray manager using XEmbed. */
static Display *tray_display = NULL;
static Tcl_Interp *tray_interp = NULL;
static Window tray_window = None;
static Window tray_parent = None;
static Window tray_icons[128];
static int tray_icon_count = 0;
static int tray_width = 1, tray_height = 24;
static int tray_screen = 0;
static int tray_event_base = 0;
static Tcl_Obj *tray_callback = NULL;
static Atom tray_selection = None;
static Atom tray_opcode = None;
static Atom tray_xembed = None;
static Atom tray_xembed_info = None;

static void tray_notify(void) {
    if (tray_callback == NULL || tray_interp == NULL) return;
    Tcl_IncrRefCount(tray_callback);
    if (Tcl_EvalObjEx(tray_interp, tray_callback, TCL_EVAL_GLOBAL) != TCL_OK)
        Tcl_BackgroundException(tray_interp, TCL_ERROR);
    Tcl_DecrRefCount(tray_callback);
}
static void tray_layout(void) {
    int i, size = tray_height;
    if (size < 1) size = 1;
    for (i = 0; i < tray_icon_count; i++)
        XMoveResizeWindow(tray_display, tray_icons[i], i * size, 0,
                          (unsigned int)size, (unsigned int)size);
    XResizeWindow(tray_display, tray_window,
                  (unsigned int)(tray_icon_count * size > tray_width ?
                                 tray_icon_count * size : tray_width),
                  (unsigned int)tray_height);
    XFlush(tray_display);
}
static void tray_remove_icon(Window win) {
    int i;
    for (i = 0; i < tray_icon_count; i++) {
        if (tray_icons[i] == win) {
            for (; i + 1 < tray_icon_count; i++) tray_icons[i] = tray_icons[i + 1];
            tray_icon_count--;
            tray_layout();
            tray_notify();
            return;
        }
    }
}
static void tray_stop_internal(void) {
    int i;
    if (tray_display != NULL) {
        Tcl_DeleteFileHandler(ConnectionNumber(tray_display));
        for (i = 0; i < tray_icon_count; i++) {
            XUnmapWindow(tray_display, tray_icons[i]);
            XReparentWindow(tray_display, tray_icons[i],
                            DefaultRootWindow(tray_display), 0, 0);
        }
        if (tray_selection != None &&
            XGetSelectionOwner(tray_display, tray_selection) == tray_window)
            XSetSelectionOwner(tray_display, tray_selection, None, CurrentTime);
        if (tray_window != None) XDestroyWindow(tray_display, tray_window);
        XFlush(tray_display);
        XCloseDisplay(tray_display);
    }
    tray_display = NULL; tray_interp = NULL; tray_window = None;
    tray_parent = None; tray_icon_count = 0; tray_selection = None;
    if (tray_callback != NULL) {
        Tcl_DecrRefCount(tray_callback);
        tray_callback = NULL;
    }
}
static void tray_deleted(ClientData cd, Tcl_Interp *ip) {
    (void)cd;
    if (tray_interp == ip) tray_stop_internal();
}
static void tray_readable(ClientData cd, int mask) {
    XEvent ev;
    (void)cd; (void)mask;
    if (tray_display == NULL) return;
    while (XPending(tray_display)) {
        XNextEvent(tray_display, &ev);
        if (ev.type == ClientMessage &&
            ev.xclient.message_type == tray_opcode &&
            ev.xclient.data.l[1] == 0) {
            Window icon = (Window)ev.xclient.data.l[2];
            if (tray_icon_count < 128) {
                int i, exists = 0;
                for (i = 0; i < tray_icon_count; i++)
                    if (tray_icons[i] == icon) exists = 1;
                if (!exists) {
                    XWindowAttributes attr;
                    if (XGetWindowAttributes(tray_display, icon, &attr)) {
                        tray_icons[tray_icon_count++] = icon;
                        XSelectInput(tray_display, icon, StructureNotifyMask);
                        XReparentWindow(tray_display, icon, tray_window, 0, 0);
                        XMapWindow(tray_display, icon);
                        {
                            XEvent embedded;
                            memset(&embedded, 0, sizeof(embedded));
                            embedded.xclient.type = ClientMessage;
                            embedded.xclient.window = icon;
                            embedded.xclient.message_type = tray_xembed;
                            embedded.xclient.format = 32;
                            embedded.xclient.data.l[0] = CurrentTime;
                            embedded.xclient.data.l[1] = 0; /* XEMBED_EMBEDDED_NOTIFY */
                            embedded.xclient.data.l[2] = tray_window;
                            XSendEvent(tray_display, icon, False, NoEventMask, &embedded);
                        }
                        tray_layout();
                        tray_notify();
                    }
                }
            }
        } else if (ev.type == DestroyNotify) {
            tray_remove_icon(ev.xdestroywindow.window);
        } else if (ev.type == ConfigureRequest) {
            XWindowChanges changes;
            changes.x = ev.xconfigurerequest.x;
            changes.y = ev.xconfigurerequest.y;
            changes.width = ev.xconfigurerequest.width;
            changes.height = ev.xconfigurerequest.height;
            changes.border_width = ev.xconfigurerequest.border_width;
            changes.sibling = ev.xconfigurerequest.above;
            changes.stack_mode = ev.xconfigurerequest.detail;
            XConfigureWindow(tray_display, ev.xconfigurerequest.window,
                             ev.xconfigurerequest.value_mask, &changes);
            tray_layout();
        } else if (ev.type == SelectionClear) {
            tray_notify();
        }
    }
}
static int TrayStartCmd(ClientData cd, Tcl_Interp *ip, int objc,
                        Tcl_Obj *const objv[]) {
    Tcl_WideInt parent;
    int width, height;
    char selection_name[64];
    XEvent manager;
    Atom orientation, visual;
    (void)cd;
    if (objc != 5) {
        Tcl_WrongNumArgs(ip, 1, objv, "parentWindowId width height callbackCommand");
        return TCL_ERROR;
    }
    if (Tcl_GetWideIntFromObj(ip, objv[1], &parent) != TCL_OK ||
        Tcl_GetIntFromObj(ip, objv[2], &width) != TCL_OK ||
        Tcl_GetIntFromObj(ip, objv[3], &height) != TCL_OK)
        return TCL_ERROR;
    if (parent <= 0 || width < 1 || height < 1) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("invalid tray parent or geometry", -1));
        return TCL_ERROR;
    }
    tray_stop_internal();
    tray_display = XOpenDisplay(NULL);
    if (tray_display == NULL) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("cannot open X display for system tray", -1));
        return TCL_ERROR;
    }
    tray_interp = ip; tray_parent = (Window)parent;
    tray_width = width; tray_height = height;
    tray_screen = DefaultScreen(tray_display);
    tray_window = XCreateSimpleWindow(tray_display, tray_parent, 0, 0,
                                      (unsigned int)width, (unsigned int)height,
                                      0, 0, 0);
    XSelectInput(tray_display, tray_window,
                 StructureNotifyMask | SubstructureNotifyMask |
                 SubstructureRedirectMask);
    snprintf(selection_name, sizeof(selection_name), "_NET_SYSTEM_TRAY_S%d", tray_screen);
    tray_selection = atom(tray_display, selection_name);
    tray_opcode = atom(tray_display, "_NET_SYSTEM_TRAY_OPCODE");
    tray_xembed = atom(tray_display, "_XEMBED");
    tray_xembed_info = atom(tray_display, "_XEMBED_INFO");
    (void)tray_xembed_info;
    XSetSelectionOwner(tray_display, tray_selection, tray_window, CurrentTime);
    XSync(tray_display, False);
    if (XGetSelectionOwner(tray_display, tray_selection) != tray_window) {
        tray_stop_internal();
        Tcl_SetObjResult(ip, Tcl_NewStringObj("another system tray manager owns the selection", -1));
        return TCL_ERROR;
    }
    orientation = atom(tray_display, "_NET_SYSTEM_TRAY_ORIENTATION");
    {
        unsigned long horizontal = 0;
        XChangeProperty(tray_display, tray_window, orientation, XA_CARDINAL,
                        32, PropModeReplace, (unsigned char *)&horizontal, 1);
    }
    visual = atom(tray_display, "_NET_SYSTEM_TRAY_VISUAL");
    {
        unsigned long visual_id = XVisualIDFromVisual(DefaultVisual(tray_display, tray_screen));
        XChangeProperty(tray_display, tray_window, visual, XA_VISUALID,
                        32, PropModeReplace, (unsigned char *)&visual_id, 1);
    }
    memset(&manager, 0, sizeof(manager));
    manager.xclient.type = ClientMessage;
    manager.xclient.window = DefaultRootWindow(tray_display);
    manager.xclient.message_type = atom(tray_display, "MANAGER");
    manager.xclient.format = 32;
    manager.xclient.data.l[0] = CurrentTime;
    manager.xclient.data.l[1] = tray_selection;
    manager.xclient.data.l[2] = tray_window;
    XSendEvent(tray_display, DefaultRootWindow(tray_display), False,
               StructureNotifyMask, &manager);
    XMapWindow(tray_display, tray_window);
    XFlush(tray_display);
    tray_callback = objv[4]; Tcl_IncrRefCount(tray_callback);
    Tcl_CreateFileHandler(ConnectionNumber(tray_display), TCL_READABLE,
                          tray_readable, NULL);
    Tcl_SetObjResult(ip, Tcl_NewWideIntObj((Tcl_WideInt)tray_window));
    return TCL_OK;
}
static int TrayConfigureCmd(ClientData cd, Tcl_Interp *ip, int objc,
                            Tcl_Obj *const objv[]) {
    int width, height;
    (void)cd;
    if (objc != 3) { Tcl_WrongNumArgs(ip, 1, objv, "width height"); return TCL_ERROR; }
    if (Tcl_GetIntFromObj(ip, objv[1], &width) != TCL_OK ||
        Tcl_GetIntFromObj(ip, objv[2], &height) != TCL_OK) return TCL_ERROR;
    if (tray_display == NULL) return TCL_OK;
    if (width < 1 || height < 1) return TCL_OK;
    tray_width = width; tray_height = height;
    tray_layout();
    return TCL_OK;
}
static int TrayStopCmd(ClientData cd, Tcl_Interp *ip, int objc,
                       Tcl_Obj *const objv[]) {
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    if (tray_interp == ip) tray_stop_internal();
    return TCL_OK;
}
static int TrayCountCmd(ClientData cd, Tcl_Interp *ip, int objc,
                        Tcl_Obj *const objv[]) {
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    Tcl_SetObjResult(ip, Tcl_NewIntObj(tray_icon_count));
    return TCL_OK;
}


static Display *monitor_watch_display = NULL;
static Tcl_Interp *monitor_watch_interp = NULL;
static Tcl_Obj *monitor_watch_callback = NULL;
static int monitor_watch_event_base = 0;

static void stop_monitor_watch(void) {
    if (monitor_watch_display != NULL) {
        Tcl_DeleteFileHandler(ConnectionNumber(monitor_watch_display));
        XCloseDisplay(monitor_watch_display);
        monitor_watch_display = NULL;
    }
    if (monitor_watch_callback != NULL) {
        Tcl_DecrRefCount(monitor_watch_callback);
        monitor_watch_callback = NULL;
    }
    monitor_watch_interp = NULL;
}
static void monitor_watch_deleted(ClientData cd, Tcl_Interp *ip) {
    (void)cd;
    if (monitor_watch_interp == ip) stop_monitor_watch();
}
static void monitor_watch_readable(ClientData cd, int mask) {
    XEvent ev;
    int changed = 0;
    Tcl_Interp *ip = (Tcl_Interp *)cd;
    (void)mask;
    if (monitor_watch_display == NULL || monitor_watch_interp != ip) return;
    while (XPending(monitor_watch_display)) {
        XNextEvent(monitor_watch_display, &ev);
        if (ev.type == monitor_watch_event_base + RRScreenChangeNotify) {
            XRRUpdateConfiguration(&ev);
            changed = 1;
        } else if (ev.type == monitor_watch_event_base + RRNotify) {
            changed = 1;
        }
    }
    if (changed && monitor_watch_callback != NULL) {
        Tcl_IncrRefCount(monitor_watch_callback);
        if (Tcl_EvalObjEx(ip, monitor_watch_callback, TCL_EVAL_GLOBAL) != TCL_OK)
            Tcl_BackgroundException(ip, TCL_ERROR);
        Tcl_DecrRefCount(monitor_watch_callback);
    }
}
static int WatchMonitorsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                            Tcl_Obj *const objv[]) {
    int error_base;
    (void)cd;
    if (objc != 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "callbackCommand");
        return TCL_ERROR;
    }
    stop_monitor_watch();
    monitor_watch_display = XOpenDisplay(NULL);
    if (monitor_watch_display == NULL) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("cannot open X display for monitor events", -1));
        return TCL_ERROR;
    }
    if (!XRRQueryExtension(monitor_watch_display, &monitor_watch_event_base,
                           &error_base)) {
        XCloseDisplay(monitor_watch_display);
        monitor_watch_display = NULL;
        Tcl_SetObjResult(ip, Tcl_NewStringObj("XRandR extension unavailable", -1));
        return TCL_ERROR;
    }
    XRRSelectInput(monitor_watch_display, DefaultRootWindow(monitor_watch_display),
                   RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask |
                   RROutputChangeNotifyMask | RROutputPropertyNotifyMask);
    XFlush(monitor_watch_display);
    monitor_watch_interp = ip;
    monitor_watch_callback = objv[1];
    Tcl_IncrRefCount(monitor_watch_callback);
    Tcl_CreateFileHandler(ConnectionNumber(monitor_watch_display), TCL_READABLE,
                          monitor_watch_readable, ip);
    Tcl_SetObjResult(ip, Tcl_NewBooleanObj(1));
    return TCL_OK;
}
static int UnwatchMonitorsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                              Tcl_Obj *const objv[]) {
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    if (monitor_watch_interp == ip) stop_monitor_watch();
    return TCL_OK;
}

static int MonitorsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                       Tcl_Obj *const objv[]) {
    Display *dpy;
    Window root;
    XRRMonitorInfo *monitors;
    int count = 0, i;
    Tcl_Obj *result;
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    dpy = open_display(ip);
    if (dpy == NULL) return TCL_ERROR;
    root = DefaultRootWindow(dpy);
    monitors = XRRGetMonitors(dpy, root, True, &count);
    result = Tcl_NewListObj(0, NULL);
    if (monitors != NULL && count > 0) {
        for (i = 0; i < count; i++) {
            Tcl_Obj *entry = Tcl_NewDictObj();
            char *name = XGetAtomName(dpy, monitors[i].name);
            Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("name", -1),
                           Tcl_NewStringObj(name != NULL ? name : "monitor", -1));
            if (name != NULL) XFree(name);
            Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("x", -1),
                           Tcl_NewIntObj(monitors[i].x));
            Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("y", -1),
                           Tcl_NewIntObj(monitors[i].y));
            Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("width", -1),
                           Tcl_NewIntObj(monitors[i].width));
            Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("height", -1),
                           Tcl_NewIntObj(monitors[i].height));
            Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("primary", -1),
                           Tcl_NewBooleanObj(monitors[i].primary != False));
            Tcl_ListObjAppendElement(ip, result, entry);
        }
        XRRFreeMonitors(monitors);
    } else {
        Tcl_Obj *entry = Tcl_NewDictObj();
        Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("name", -1),
                       Tcl_NewStringObj("screen", -1));
        Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("x", -1), Tcl_NewIntObj(0));
        Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("y", -1), Tcl_NewIntObj(0));
        Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("width", -1),
                       Tcl_NewIntObj(DisplayWidth(dpy, DefaultScreen(dpy))));
        Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("height", -1),
                       Tcl_NewIntObj(DisplayHeight(dpy, DefaultScreen(dpy))));
        Tcl_DictObjPut(ip, entry, Tcl_NewStringObj("primary", -1),
                       Tcl_NewBooleanObj(1));
        Tcl_ListObjAppendElement(ip, result, entry);
    }
    XCloseDisplay(dpy);
    Tcl_SetObjResult(ip, result);
    return TCL_OK;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int Tclpanelx11_Init(Tcl_Interp *ip) {
    if (Tcl_InitStubs(ip, "8.6", 0) == NULL) return TCL_ERROR;
    if (Tcl_FindNamespace(ip, "::x11", NULL, TCL_GLOBAL_ONLY) == NULL)
        Tcl_CreateNamespace(ip, "::x11", NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::strut", StrutCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::tray_start", TrayStartCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::tray_configure", TrayConfigureCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::tray_stop", TrayStopCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::tray_count", TrayCountCmd, NULL, NULL);
    Tcl_CallWhenDeleted(ip, tray_deleted, NULL);
    Tcl_CreateObjCommand(ip, "::x11::monitors", MonitorsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::watch_monitors", WatchMonitorsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::unwatch_monitors", UnwatchMonitorsCmd, NULL, NULL);
    Tcl_CallWhenDeleted(ip, monitor_watch_deleted, NULL);
    Tcl_CreateObjCommand(ip, "::x11::windows", WindowsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::activate", ActivateCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::desktops", DesktopsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::current_desktop", CurrentDesktopCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::set_desktop", SetDesktopCmd, NULL, NULL);
    Tcl_PkgProvide(ip, "tclpanelx11", "0.6");
    return TCL_OK;
}
