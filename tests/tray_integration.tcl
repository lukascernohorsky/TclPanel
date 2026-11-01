if {$argc != 2} {
    error "usage: tray_integration.tcl extension-path client-path"
}
set extension [file normalize [lindex $argv 0]]
set client [file normalize [lindex $argv 1]]
package require Tk 8.6
load $extension Tclpanelx11

proc assert {condition message} {
    if {![uplevel 1 [list expr $condition]]} { error $message }
}
proc wait_for {condition message} {
    for {set i 0} {$i < 80} {incr i} {
        update
        if {[uplevel 1 [list expr $condition]]} { return }
        after 25
    }
    error $message
}

wm withdraw .
frame .tray -width 48 -height 24
pack .tray
update
set callbackCount 0
proc tray_changed {} { incr ::callbackCount }
set trayWindow [::x11::tray_start [winfo id .tray] 48 24 tray_changed]
set channel {}
try {
    set channel [open |[list $client $trayWindow] r+]
    fconfigure $channel -buffering line
    if {[gets $channel iconId] < 0} { error "XEmbed test client did not start" }

    wait_for {[::x11::tray_count] == 1} "tray did not accept dock request"
    puts $channel unmap
    if {[gets $channel response] < 0 || $response ne "ok"} {
        error "test client failed to send unmap state"
    }
    update
    puts $channel map
    if {[gets $channel response] < 0 || $response ne "ok"} {
        error "test client failed to send map state"
    }
    update
    assert {[::x11::tray_count] == 1} "mapped-state changes removed tray icon"

    puts $channel destroy
    if {[gets $channel response] < 0 || $response ne "done"} {
        error "test client failed to destroy icon"
    }
    wait_for {[::x11::tray_count] == 0} "tray did not remove destroyed icon"
    close $channel
    set channel {}
    puts "XEmbed integration test passed"
} finally {
    if {$channel ne {}} {
        catch {puts $channel quit; flush $channel; close $channel}
    }
    ::x11::tray_stop
    destroy .tray
}
