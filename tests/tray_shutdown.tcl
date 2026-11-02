if {$argc != 2} {
    error "usage: tray_shutdown.tcl extension-path parent-window-id"
}
set extension [file normalize [lindex $argv 0]]
set parent [lindex $argv 1]
load $extension Tclpanelx11
proc shutdown_on_input {} {
    if {[gets stdin command] < 0 || $command eq "exit"} {
        exit
    }
}
set trayWindow [::x11::tray_start $parent 48 24 { }]
puts $trayWindow
flush stdout
fileevent stdin readable shutdown_on_input
vwait ::wait_for_shutdown
