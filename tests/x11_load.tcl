if {$argc != 1} {
    error "usage: x11_load.tcl extension-path"
}
load [file normalize [lindex $argv 0]] Tclpanelx11
set commands {
    ::x11::strut ::x11::tray_start ::x11::tray_configure
    ::x11::tray_stop ::x11::tray_count ::x11::tray_active
    ::x11::monitors ::x11::watch_monitors ::x11::unwatch_monitors
    ::x11::windows ::x11::active_window
    ::x11::watch_windows ::x11::unwatch_windows
    ::x11::activate ::x11::desktops ::x11::current_desktop
    ::x11::desktop_names ::x11::set_desktop
    ::x11::watch_desktops ::x11::unwatch_desktops
}
foreach command $commands {
    if {[llength [info commands $command]] != 1} {
        error "X11 extension did not register $command"
    }
}
puts "X11 extension load test passed ([llength $commands] commands)"
