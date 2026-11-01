#include <tcl.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>

static int StrutCmd(ClientData cd, Tcl_Interp *ip, int objc,
                    Tcl_Obj *const objv[]) {
    Display *dpy;
    Window win;
    Atom partial, basic;
    unsigned long values[12] = {0};
    const char *edge;
    int thickness;
    char *end = NULL;
    unsigned long id;
    int screen, width, height;

    (void)cd;
    if (objc != 4) {
        Tcl_WrongNumArgs(ip, 1, objv, "windowId top|bottom thickness");
        return TCL_ERROR;
    }

    id = strtoul(Tcl_GetString(objv[1]), &end, 0);
    if (end == Tcl_GetString(objv[1]) || !end || *end != '\\0') {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("invalid X11 window id", -1));
        return TCL_ERROR;
    }
    edge = Tcl_GetString(objv[2]);
    if (Tcl_GetIntFromObj(ip, objv[3], &thickness) != TCL_OK ||
        thickness < 0) {
        return TCL_ERROR;
    }
    if (strcmp(edge, "top") != 0 && strcmp(edge, "bottom") != 0) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("edge must be top or bottom", -1));
        return TCL_ERROR;
    }

    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        Tcl_SetObjResult(ip, Tcl_NewStringObj("cannot open X display", -1));
        return TCL_ERROR;
    }

    screen = DefaultScreen(dpy);
    width = DisplayWidth(dpy, screen);
    height = DisplayHeight(dpy, screen);
    win = (Window)id;

    if (strcmp(edge, "top") == 0) {
        values[2] = (unsigned long)thickness;
        values[8] = 0;
        values[9] = (unsigned long)(width - 1);
    } else {
        values[3] = (unsigned long)thickness;
        values[10] = 0;
        values[11] = (unsigned long)(width - 1);
    }

    partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    basic = XInternAtom(dpy, "_NET_WM_STRUT", False);
    XChangeProperty(dpy, win, partial, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)values, 12);
    XChangeProperty(dpy, win, basic, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)values, 4);
    XFlush(dpy);
    XCloseDisplay(dpy);
    (void)height;
    return TCL_OK;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int Tclpanelx11_Init(Tcl_Interp *ip) {
    if (Tcl_InitStubs(ip, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }
    if (Tcl_FindNamespace(ip, "::x11", NULL, TCL_GLOBAL_ONLY) == NULL) {
        Tcl_CreateNamespace(ip, "::x11", NULL, NULL);
    }
    Tcl_CreateObjCommand(ip, "::x11::strut", StrutCmd, NULL, NULL);
    Tcl_PkgProvide(ip, "tclpanelx11", "0.2");
    return TCL_OK;
}
