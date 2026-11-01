#include <tcl.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>

static int StrutCmd(ClientData cd, Tcl_Interp *ip, int objc, Tcl_Obj *const objv[]) {
    Display *dpy;
    Window win;
    Atom partial, basic;
    unsigned long values[12] = {0};
    const char *edge;
    int height;
    char *end = NULL;
    unsigned long id;
    (void)cd;
    if (objc != 4) {
        Tcl_WrongNumArgs(ip, 1, objv, "windowId top|bottom thickness");
        return TCL_ERROR;
    }
    id = strtoul(Tcl_GetString(objv[1]), &end, 0);
    if (!end || *end) {
        Tcl_SetResult(ip, "invalid X11 window id", TCL_STATIC);
        return TCL_ERROR;
    }
    edge = Tcl_GetString(objv[2]);
    if (Tcl_GetIntFromObj(ip, objv[3], &height) != TCL_OK || height < 0) return TCL_ERROR;
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        Tcl_SetResult(ip, "cannot open X display", TCL_STATIC);
        return TCL_ERROR;
    }
    win = (Window)id;
    if (strcmp(edge, "top") == 0) values[2] = (unsigned long)height;
    else if (strcmp(edge, "bottom") == 0) values[3] = (unsigned long)height;
    else {
        XCloseDisplay(dpy);
        Tcl_SetResult(ip, "edge must be top or bottom", TCL_STATIC);
        return TCL_ERROR;
    }
    partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    basic = XInternAtom(dpy, "_NET_WM_STRUT", False);
    XChangeProperty(dpy, win, partial, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)values, 12);
    XChangeProperty(dpy, win, basic, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)values, 4);
    XFlush(dpy);
    XCloseDisplay(dpy);
    return TCL_OK;
}
#ifdef _WIN32
__declspec(dllexport)
#endif
int Tclpanelx11_Init(Tcl_Interp *ip) {
    if (Tcl_InitStubs(ip, "8.6", 0) == NULL) return TCL_ERROR;
    Tcl_CreateNamespace(ip, "::x11", NULL, NULL);
    Tcl_CreateObjCommand(ip, "::x11::strut", StrutCmd, NULL, NULL);
    Tcl_PkgProvide(ip, "tclpanelx11", "0.1");
    return TCL_OK;
}
