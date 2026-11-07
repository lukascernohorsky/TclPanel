#include <X11/Xlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    Display *display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "cannot open X display\n");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    puts("READY");
    fflush(stdout);
    /*
     * A read-only Tcl pipeline cannot send EOF to the child's stdin. Emit a
     * heartbeat instead: closing the parent's read end makes write() fail,
     * which lets this helper release its X11 connection and exit cleanly.
     */
    for (;;) {
        sleep(1);
        if (write(STDOUT_FILENO, ".", 1) != 1)
            break;
    }
    XCloseDisplay(display);
    return 0;
}
