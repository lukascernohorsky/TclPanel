#include <tcl.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>

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
    int thickness, width;
    char *end = NULL;
    unsigned long id;
    (void)cd;
    if (objc != 4) { Tcl_WrongNumArgs(ip, 1, objv, "windowId top|bottom thickness"); return TCL_ERROR; }
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
    if (strcmp(edge, "top") == 0) {
        values[2] = (unsigned long)thickness; values[8] = 0; values[9] = (unsigned long)(width - 1);
    } else {
        values[3] = (unsigned long)thickness; values[10] = 0; values[11] = (unsigned long)(width - 1);
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
#ifdef _WIN32
__declspec(dllexport)
#endif
int Tclpanelx11_Init(Tcl_Interp *ip) {
    if (Tcl_InitStubs(ip, "8.6", 0) == NULL) return TCL_ERROR;
    if (Tcl_FindNamespace(ip, "::x11", NULL, TCL_GLOBAL_ONLY) == NULL)
        Tcl_CreateNamespace(ip, "::x11", NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::strut", StrutCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::windows", WindowsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::activate", ActivateCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::desktops", DesktopsCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::current_desktop", CurrentDesktopCmd, NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::set_desktop", SetDesktopCmd, NULL, NULL);
    Tcl_PkgProvide(ip, "tclpanelx11", "0.4");
    return TCL_OK;
}
