if {$argc != 2} {
    error "usage: tray_shutdown.tcl extension-path parent-window-id"
}
set extension [file normalize [lindex $argv 0]]
set parent [lindex $argv 1]

# Run the tray in a slave interpreter so the test can exercise the exact
# Tcl_CallWhenDeleted path deterministically, without terminating tclsh.
set manager [interp create]
interp eval $manager [list load $extension Tclpanelx11]
set trayWindow [interp eval $manager [list ::x11::tray_start $parent 48 24 { }]]
puts $trayWindow
flush stdout

proc shutdown_on_input {} {
    if {[gets stdin command] < 0 || $command eq "exit"} {
        set ::wait_for_shutdown 1
    }
}
fileevent stdin readable shutdown_on_input
vwait ::wait_for_shutdown
interp delete $manager
