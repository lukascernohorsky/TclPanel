#include <X11/Xlib.h>
#include <stdio.h>

int main(void) {
    Display *display = XOpenDisplay(NULL);
    int ch;

    if (display == NULL) {
        fprintf(stderr, "cannot open X display\n");
        return 1;
    }
    puts("READY");
    fflush(stdout);
    while ((ch = getchar()) != EOF) {
        /* Keep the X11 connection alive until the parent closes the pipe. */
    }
    XCloseDisplay(display);
    return 0;
}
