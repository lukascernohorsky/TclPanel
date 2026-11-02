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

/*
 * Xlib reports invalid resource IDs through its asynchronous error handler;
 * XGetWindowAttributes() alone does not protect the process from BadWindow.
 * The display is newly opened here, so synchronizing around this one request
 * safely scopes the temporary handler to parent-window validation.
 */
static int trapped_x_error = 0;
static int tray_validation_error_handler(Display *dpy, XErrorEvent *event) {
    (void)dpy;
    trapped_x_error = event->error_code;
    return 0;
}
static int valid_tray_parent(Display *dpy, Window parent) {
    XWindowAttributes attributes;
    XErrorHandler previous_handler;
    int status;
    trapped_x_error = 0;
    previous_handler = XSetErrorHandler(tray_validation_error_handler);
    status = XGetWindowAttributes(dpy, parent, &attributes);
    XSync(dpy, False);
    XSetErrorHandler(previous_handler);
    return status != 0 && trapped_x_error == 0 &&
           attributes.class == InputOutput;
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
static int ActiveWindowCmd(ClientData cd, Tcl_Interp *ip, int objc,
                            Tcl_Obj *const objv[]) {
    Display *dpy;
    Atom actual;
    int format;
    unsigned long count, after, active = 0;
    unsigned char *data = NULL;
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    dpy = open_display(ip);
    if (dpy == NULL) return TCL_ERROR;
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy),
            atom(dpy, "_NET_ACTIVE_WINDOW"), 0, 1, False, XA_WINDOW,
            &actual, &format, &count, &after, &data) == Success &&
        data != NULL && actual == XA_WINDOW && format == 32 && count == 1)
        active = *(unsigned long *)data;
    if (data != NULL) XFree(data);
    XCloseDisplay(dpy);
    Tcl_SetObjResult(ip, Tcl_NewWideIntObj((Tcl_WideInt)active));
    return TCL_OK;
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
static int DesktopNamesCmd(ClientData cd, Tcl_Interp *ip, int objc,
                             Tcl_Obj *const objv[]) {
    Display *dpy;
    Atom actual, utf8;
    int format;
    unsigned long count, after, start = 0, i;
    unsigned char *data = NULL;
    Tcl_Obj *result = Tcl_NewListObj(0, NULL);
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    dpy = open_display(ip);
    if (dpy == NULL) return TCL_ERROR;
    utf8 = atom(dpy, "UTF8_STRING");
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy),
            atom(dpy, "_NET_DESKTOP_NAMES"), 0, 65536, False, utf8,
            &actual, &format, &count, &after, &data) == Success &&
        data != NULL && actual == utf8 && format == 8) {
        for (i = 0; i < count; i++) {
            if (data[i] == '\0') {
                Tcl_ListObjAppendElement(ip, result,
                    Tcl_NewStringObj((const char *)data + start,
                                     (int)(i - start)));
                start = i + 1;
            }
        }
        if (start < count)
            Tcl_ListObjAppendElement(ip, result,
                Tcl_NewStringObj((const char *)data + start,
                                 (int)(count - start)));
    }
    if (data != NULL) XFree(data);
    XCloseDisplay(dpy);
    Tcl_SetObjResult(ip, result);
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
static int tray_icon_size = 0, tray_icon_gap = 0;
static int tray_screen = 0;
static Tcl_Obj *tray_callback = NULL;
static Atom tray_selection = None;
static Atom tray_opcode = None;
static Atom tray_xembed = None;
static Atom tray_xembed_info = None;

static void tray_stop_internal(void);

static void tray_notify(void) {
    Tcl_Obj *callback = tray_callback;
    if (callback == NULL || tray_interp == NULL) return;
    Tcl_IncrRefCount(callback);
    if (Tcl_EvalObjEx(tray_interp, callback, TCL_EVAL_GLOBAL) != TCL_OK)
        Tcl_BackgroundException(tray_interp, TCL_ERROR);
    Tcl_DecrRefCount(callback);
}
static int tray_icon_is_mapped(Window win) {
    Atom actual;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    int mapped = 1; /* Legacy clients without _XEMBED_INFO are mapped by default. */
    if (XGetWindowProperty(tray_display, win, tray_xembed_info, 0, 2, False,
                           tray_xembed_info, &actual, &format, &nitems,
                           &after, &data) == Success && data != NULL &&
        actual == tray_xembed_info && format == 32 && nitems >= 2) {
        mapped = ((((unsigned long *)data)[1] & 1UL) != 0);
    }
    if (data != NULL) XFree(data);
    return mapped;
}
/*
 * A dock request can outlive its icon: the client may destroy the window
 * before this connection processes the ClientMessage. Keep every request
 * against that untrusted XID inside one synchronized error-trap scope.
 */
static int tray_dock_icon(Window win) {
    XWindowAttributes attributes;
    XEvent embedded;
    XErrorHandler previous_handler;
    int status, mapped;
    XSync(tray_display, False);
    trapped_x_error = 0;
    previous_handler = XSetErrorHandler(tray_validation_error_handler);
    status = XGetWindowAttributes(tray_display, win, &attributes);
    if (status && trapped_x_error == 0)
        XSelectInput(tray_display, win,
                     StructureNotifyMask | PropertyChangeMask);
    if (status && trapped_x_error == 0)
        XReparentWindow(tray_display, win, tray_window, 0, 0);
    if (status && trapped_x_error == 0) {
        memset(&embedded, 0, sizeof(embedded));
        embedded.xclient.type = ClientMessage;
        embedded.xclient.window = win;
        embedded.xclient.message_type = tray_xembed;
        embedded.xclient.format = 32;
        embedded.xclient.data.l[0] = CurrentTime;
        embedded.xclient.data.l[1] = 0; /* XEMBED_EMBEDDED_NOTIFY */
        embedded.xclient.data.l[2] = tray_window;
        XSendEvent(tray_display, win, False, NoEventMask, &embedded);
        mapped = tray_icon_is_mapped(win);
        if (mapped) XMapWindow(tray_display, win);
        else XUnmapWindow(tray_display, win);
    }
    XSync(tray_display, False);
    XSetErrorHandler(previous_handler);
    return status && trapped_x_error == 0;
}
static void tray_parent_lost_idle(ClientData cd) {
    (void)cd;
    tray_stop_internal();
}
static void tray_parent_lost(void) {
    tray_icon_count = 0;
    tray_window = None;
    tray_parent = None;
    tray_notify();
    Tcl_DoWhenIdle(tray_parent_lost_idle, NULL);
}
/* Run one request against a client-owned XID without allowing BadWindow
 * to reach Xlib's process-wide default handler. */
static int tray_move_icon(Window win, int x, int y, unsigned int w,
                          unsigned int h) {
    XErrorHandler previous;
    trapped_x_error = 0;
    XSync(tray_display, False);
    previous = XSetErrorHandler(tray_validation_error_handler);
    XMoveResizeWindow(tray_display, win, x, y, w, h);
    XSync(tray_display, False);
    XSetErrorHandler(previous);
    return trapped_x_error == 0;
}
static int tray_apply_icon_state(Window win) {
    Atom actual;
    int format, mapped = 1, ok = 1;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    XErrorHandler previous;
    XSync(tray_display, False);
    trapped_x_error = 0;
    previous = XSetErrorHandler(tray_validation_error_handler);
    if (XGetWindowProperty(tray_display, win, tray_xembed_info, 0, 2,
                           False, tray_xembed_info, &actual, &format,
                           &nitems, &after, &data) == Success &&
        data != NULL && actual == tray_xembed_info && format == 32 &&
        nitems >= 2)
        mapped = ((((unsigned long *)data)[1] & 1UL) != 0);
    if (data != NULL) XFree(data);
    if (trapped_x_error == 0) {
        if (mapped) XMapWindow(tray_display, win);
        else XUnmapWindow(tray_display, win);
    }
    XSync(tray_display, False);
    XSetErrorHandler(previous);
    ok = trapped_x_error == 0;
    return ok;
}
static int tray_configure_icon(const XConfigureRequestEvent *request) {
    XWindowChanges changes;
    XErrorHandler previous;
    changes.x = request->x; changes.y = request->y;
    changes.width = request->width; changes.height = request->height;
    changes.border_width = request->border_width;
    changes.sibling = request->above; changes.stack_mode = request->detail;
    XSync(tray_display, False);
    trapped_x_error = 0;
    previous = XSetErrorHandler(tray_validation_error_handler);
    XConfigureWindow(tray_display, request->window, request->value_mask,
                     &changes);
    XSync(tray_display, False);
    XSetErrorHandler(previous);
    return trapped_x_error == 0;
}
static void tray_layout(void) {
    int i = 0, size = tray_icon_size > 0 ? tray_icon_size : tray_height;
    int gap = tray_icon_gap;
    int content_width;
    if (tray_display == NULL || tray_window == None) return;
    if (!valid_tray_parent(tray_display, tray_window)) {
        tray_parent_lost();
        return;
    }
    if (size < 1) size = 1;
    if (size > tray_height) size = tray_height;
    if (gap < 0) gap = 0;
    content_width = tray_icon_count > 0 ?
        tray_icon_count * size + (tray_icon_count - 1) * gap : 0;
    while (i < tray_icon_count) {
        if (!tray_move_icon(tray_icons[i], i * (size + gap),
                            (tray_height - size) / 2,
                            (unsigned int)size, (unsigned int)size)) {
            int j;
            for (j = i; j + 1 < tray_icon_count; j++)
                tray_icons[j] = tray_icons[j + 1];
            tray_icon_count--;
            tray_notify();
            content_width = tray_icon_count > 0 ?
                tray_icon_count * size + (tray_icon_count - 1) * gap : 0;
            continue;
        }
        i++;
    }
    XResizeWindow(tray_display, tray_window,
                  (unsigned int)(content_width > tray_width ?
                                 content_width : tray_width),
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
            XErrorHandler previous;
            XSync(tray_display, False);
            trapped_x_error = 0;
            previous = XSetErrorHandler(tray_validation_error_handler);
            XUnmapWindow(tray_display, tray_icons[i]);
            XSync(tray_display, False);
            if (trapped_x_error == 0) {
                XReparentWindow(tray_display, tray_icons[i],
                                DefaultRootWindow(tray_display), 0, 0);
                XSync(tray_display, False);
            }
            XSetErrorHandler(previous);
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
static void tray_selection_lost_idle(ClientData cd) {
    (void)cd;
    tray_stop_internal();
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
                if (!exists && tray_dock_icon(icon)) {
                    tray_icons[tray_icon_count++] = icon;
                    tray_layout();
                    tray_notify();
                }
            }
        } else if (ev.type == PropertyNotify && ev.xproperty.atom == tray_xembed_info) {
            if (!tray_apply_icon_state(ev.xproperty.window))
                tray_remove_icon(ev.xproperty.window);
            else
                tray_layout();
        } else if (ev.type == DestroyNotify) {
            if (ev.xdestroywindow.window == tray_parent ||
                ev.xdestroywindow.window == tray_window) {
                /*
                 * The embedding hierarchy is gone. Its child tray window and
                 * any docked icons have already been destroyed by the server;
                 * do not issue cleanup requests against those stale XIDs.
                 */
                tray_parent_lost();
                return;
            }
            tray_remove_icon(ev.xdestroywindow.window);
        } else if (ev.type == ConfigureRequest) {
            if (!tray_configure_icon(&ev.xconfigurerequest))
                tray_remove_icon(ev.xconfigurerequest.window);
            else
                tray_layout();
        } else if (ev.type == SelectionClear) {
            tray_selection = None;
            tray_notify();
            Tcl_DoWhenIdle(tray_selection_lost_idle, NULL);
            return;
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
    {
        Display *new_display = XOpenDisplay(NULL);
        if (new_display == NULL) {
            Tcl_SetObjResult(ip, Tcl_NewStringObj("cannot open X display for system tray", -1));
            return TCL_ERROR;
        }
        /*
         * Validate the embedding window before taking down an existing tray.
         * Xlib reports BadWindow asynchronously, so validation uses a
         * temporary error handler and XSync rather than relying on the
         * synchronous return value of XGetWindowAttributes().
         */
        if (!valid_tray_parent(new_display, (Window)parent)) {
            XCloseDisplay(new_display);
            Tcl_SetObjResult(ip, Tcl_NewStringObj(
                "tray parent must be an existing InputOutput X11 window", -1));
            return TCL_ERROR;
        }
        tray_stop_internal();
        tray_display = new_display;
    }
    tray_interp = ip; tray_parent = (Window)parent;
    tray_width = width; tray_height = height;
    tray_icon_size = 0; tray_icon_gap = 0;
    tray_screen = DefaultScreen(tray_display);
    XSelectInput(tray_display, tray_parent, StructureNotifyMask);
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
    int width, height, icon_size = 0, gap = 0;
    (void)cd;
    if (objc != 3 && objc != 5) {
        Tcl_WrongNumArgs(ip, 1, objv, "width height ?iconSize gap?");
        return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(ip, objv[1], &width) != TCL_OK ||
        Tcl_GetIntFromObj(ip, objv[2], &height) != TCL_OK) return TCL_ERROR;
    if (objc == 5 &&
        (Tcl_GetIntFromObj(ip, objv[3], &icon_size) != TCL_OK ||
         Tcl_GetIntFromObj(ip, objv[4], &gap) != TCL_OK)) return TCL_ERROR;
    if (width < 1 || height < 1 || icon_size < 0 || gap < 0) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("tray dimensions must be positive and icon size/gap non-negative", -1));
        return TCL_ERROR;
    }
    if (tray_display == NULL) return TCL_OK;
    tray_width = width; tray_height = height;
    tray_icon_size = icon_size; tray_icon_gap = gap;
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
static int TrayActiveCmd(ClientData cd, Tcl_Interp *ip, int objc,
                         Tcl_Obj *const objv[]) {
    int active;
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    active = tray_display != NULL && tray_selection != None &&
             XGetSelectionOwner(tray_display, tray_selection) == tray_window;
    Tcl_SetObjResult(ip, Tcl_NewBooleanObj(active));
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
        Tcl_Obj *callback = monitor_watch_callback;
        Tcl_IncrRefCount(callback);
        if (Tcl_EvalObjEx(ip, callback, TCL_EVAL_GLOBAL) != TCL_OK)
            Tcl_BackgroundException(ip, TCL_ERROR);
        Tcl_DecrRefCount(callback);
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


/* EWMH window-list watcher. Root properties track membership; per-client
 * events track title, state, desktop and mapping changes. */
static Display *window_watch_display = NULL;
static Tcl_Interp *window_watch_interp = NULL;
static Tcl_Obj *window_watch_callback = NULL;
static Atom window_watch_client_list = None;
static Atom window_watch_client_stacking = None;
static Atom window_watch_name = None;
static Atom window_watch_utf8_name = None;
static Atom window_watch_state = None;
static Atom window_watch_desktop = None;
static Atom window_watch_active = None;

static void stop_window_watch(void) {
    if (window_watch_display != NULL) {
        Tcl_DeleteFileHandler(ConnectionNumber(window_watch_display));
        XCloseDisplay(window_watch_display);
        window_watch_display = NULL;
    }
    if (window_watch_callback != NULL) {
        Tcl_DecrRefCount(window_watch_callback);
        window_watch_callback = NULL;
    }
    window_watch_interp = NULL;
}
static void window_watch_deleted(ClientData cd, Tcl_Interp *ip) {
    (void)cd;
    if (window_watch_interp == ip) stop_window_watch();
}
static void window_watch_notify(void) {
    Tcl_Obj *callback = window_watch_callback;
    Tcl_Interp *ip = window_watch_interp;
    if (callback == NULL || ip == NULL) return;
    Tcl_IncrRefCount(callback);
    if (Tcl_EvalObjEx(ip, callback, TCL_EVAL_GLOBAL) != TCL_OK)
        Tcl_BackgroundException(ip, TCL_ERROR);
    Tcl_DecrRefCount(callback);
}
static void window_watch_select_clients(void) {
    Window root = DefaultRootWindow(window_watch_display);
    Atom actual;
    int format;
    unsigned long count, after, i;
    unsigned char *data = NULL;
    XErrorHandler previous;
    if (XGetWindowProperty(window_watch_display, root,
            window_watch_client_list, 0, 65536, False, XA_WINDOW,
            &actual, &format, &count, &after, &data) != Success ||
        data == NULL || actual != XA_WINDOW || format != 32) {
        if (data != NULL) XFree(data);
        return;
    }
    XSync(window_watch_display, False);
    trapped_x_error = 0;
    previous = XSetErrorHandler(tray_validation_error_handler);
    for (i = 0; i < count; i++)
        XSelectInput(window_watch_display, ((Window *)data)[i],
                     PropertyChangeMask | StructureNotifyMask);
    XSync(window_watch_display, False);
    XSetErrorHandler(previous);
    XFree(data);
}
static void window_watch_readable(ClientData cd, int mask) {
    XEvent ev;
    Window root;
    int changed = 0;
    (void)cd; (void)mask;
    if (window_watch_display == NULL) return;
    root = DefaultRootWindow(window_watch_display);
    while (XPending(window_watch_display)) {
        XNextEvent(window_watch_display, &ev);
        if (ev.type == PropertyNotify) {
            if (ev.xproperty.window == root &&
                (ev.xproperty.atom == window_watch_client_list ||
                 ev.xproperty.atom == window_watch_client_stacking ||
                 ev.xproperty.atom == window_watch_active)) {
                window_watch_select_clients();
                changed = 1;
            } else if (ev.xproperty.atom == window_watch_name ||
                       ev.xproperty.atom == window_watch_utf8_name ||
                       ev.xproperty.atom == window_watch_state ||
                       ev.xproperty.atom == window_watch_desktop) {
                changed = 1;
            }
        } else if (ev.type == MapNotify || ev.type == UnmapNotify ||
                   ev.type == DestroyNotify) {
            changed = 1;
        }
    }
    if (changed) window_watch_notify();
}
static int WatchWindowsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                           Tcl_Obj *const objv[]) {
    (void)cd;
    if (objc != 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "callbackCommand");
        return TCL_ERROR;
    }
    stop_window_watch();
    window_watch_display = XOpenDisplay(NULL);
    if (window_watch_display == NULL) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj(
            "cannot open X display for window events", -1));
        return TCL_ERROR;
    }
    window_watch_interp = ip;
    window_watch_callback = objv[1];
    Tcl_IncrRefCount(window_watch_callback);
    window_watch_client_list = atom(window_watch_display, "_NET_CLIENT_LIST");
    window_watch_client_stacking = atom(window_watch_display,
                                         "_NET_CLIENT_LIST_STACKING");
    window_watch_name = atom(window_watch_display, "WM_NAME");
    window_watch_utf8_name = atom(window_watch_display, "_NET_WM_NAME");
    window_watch_state = atom(window_watch_display, "_NET_WM_STATE");
    window_watch_desktop = atom(window_watch_display, "_NET_WM_DESKTOP");
    window_watch_active = atom(window_watch_display, "_NET_ACTIVE_WINDOW");
    XSelectInput(window_watch_display,
                 DefaultRootWindow(window_watch_display), PropertyChangeMask);
    window_watch_select_clients();
    XFlush(window_watch_display);
    Tcl_CreateFileHandler(ConnectionNumber(window_watch_display), TCL_READABLE,
                          window_watch_readable, NULL);
    Tcl_SetObjResult(ip, Tcl_NewBooleanObj(1));
    return TCL_OK;
}
static int UnwatchWindowsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                             Tcl_Obj *const objv[]) {
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    if (window_watch_interp == ip) stop_window_watch();
    return TCL_OK;
}


/* EWMH virtual-desktop watcher. */
static Display *desktop_watch_display = NULL;
static Tcl_Interp *desktop_watch_interp = NULL;
static Tcl_Obj *desktop_watch_callback = NULL;
static Atom desktop_watch_current = None;
static Atom desktop_watch_count = None;
static Atom desktop_watch_names = None;

static void stop_desktop_watch(void) {
    if (desktop_watch_display != NULL) {
        Tcl_DeleteFileHandler(ConnectionNumber(desktop_watch_display));
        XCloseDisplay(desktop_watch_display);
        desktop_watch_display = NULL;
    }
    if (desktop_watch_callback != NULL) {
        Tcl_DecrRefCount(desktop_watch_callback);
        desktop_watch_callback = NULL;
    }
    desktop_watch_interp = NULL;
}
static void desktop_watch_deleted(ClientData cd, Tcl_Interp *ip) {
    (void)cd;
    if (desktop_watch_interp == ip) stop_desktop_watch();
}
static void desktop_watch_readable(ClientData cd, int mask) {
    XEvent ev;
    int changed = 0;
    (void)cd; (void)mask;
    if (desktop_watch_display == NULL) return;
    while (XPending(desktop_watch_display)) {
        XNextEvent(desktop_watch_display, &ev);
        if (ev.type == PropertyNotify &&
            ev.xproperty.window == DefaultRootWindow(desktop_watch_display) &&
            (ev.xproperty.atom == desktop_watch_current ||
             ev.xproperty.atom == desktop_watch_count ||
             ev.xproperty.atom == desktop_watch_names))
            changed = 1;
    }
    if (changed && desktop_watch_callback != NULL &&
        desktop_watch_interp != NULL) {
        Tcl_Obj *callback = desktop_watch_callback;
        Tcl_Interp *ip = desktop_watch_interp;
        Tcl_IncrRefCount(callback);
        if (Tcl_EvalObjEx(ip, callback, TCL_EVAL_GLOBAL) != TCL_OK)
            Tcl_BackgroundException(ip, TCL_ERROR);
        Tcl_DecrRefCount(callback);
    }
}
static int WatchDesktopsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                            Tcl_Obj *const objv[]) {
    (void)cd;
    if (objc != 2) {
        Tcl_WrongNumArgs(ip, 1, objv, "callbackCommand");
        return TCL_ERROR;
    }
    stop_desktop_watch();
    desktop_watch_display = XOpenDisplay(NULL);
    if (desktop_watch_display == NULL) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj(
            "cannot open X display for desktop events", -1));
        return TCL_ERROR;
    }
    desktop_watch_current = atom(desktop_watch_display, "_NET_CURRENT_DESKTOP");
    desktop_watch_count = atom(desktop_watch_display, "_NET_NUMBER_OF_DESKTOPS");
    desktop_watch_names = atom(desktop_watch_display, "_NET_DESKTOP_NAMES");
    XSelectInput(desktop_watch_display,
                 DefaultRootWindow(desktop_watch_display), PropertyChangeMask);
    XFlush(desktop_watch_display);
    desktop_watch_interp = ip;
    desktop_watch_callback = objv[1];
    Tcl_IncrRefCount(desktop_watch_callback);
    Tcl_CreateFileHandler(ConnectionNumber(desktop_watch_display), TCL_READABLE,
                          desktop_watch_readable, NULL);
    Tcl_SetObjResult(ip, Tcl_NewBooleanObj(1));
    return TCL_OK;
}
static int UnwatchDesktopsCmd(ClientData cd, Tcl_Interp *ip, int objc,
                              Tcl_Obj *const objv[]) {
    (void)cd;
    if (objc != 1) { Tcl_WrongNumArgs(ip, 1, objv, ""); return TCL_ERROR; }
    if (desktop_watch_interp == ip) stop_desktop_watch();
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
#elif defined(__GNUC__)
__attribute__((visibility("default")))
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
    Tcl_CreateObjCommand(ip, "::x11::tray_active", TrayActiveCmd, NULL, NULL);
    Tcl_CallWhenDeleted(ip, tray_deleted, NULL);
    Tcl_CreateObjCommand(ip, "::x11::monitors", MonitorsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::watch_monitors", WatchMonitorsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::unwatch_monitors", UnwatchMonitorsCmd, NULL, NULL);
    Tcl_CallWhenDeleted(ip, monitor_watch_deleted, NULL);
    Tcl_CreateObjCommand(ip, "::x11::windows", WindowsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::active_window", ActiveWindowCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::watch_windows", WatchWindowsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::unwatch_windows", UnwatchWindowsCmd, NULL, NULL);
    Tcl_CallWhenDeleted(ip, window_watch_deleted, NULL);
    Tcl_CreateObjCommand(ip, "::x11::activate", ActivateCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::desktops", DesktopsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::desktop_names", DesktopNamesCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::current_desktop", CurrentDesktopCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::set_desktop", SetDesktopCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::watch_desktops", WatchDesktopsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::unwatch_desktops", UnwatchDesktopsCmd, NULL, NULL);
    Tcl_CallWhenDeleted(ip, desktop_watch_deleted, NULL);
    Tcl_PkgProvide(ip, "tclpanelx11", "0.7");
    return TCL_OK;
}
