if {$argc != 2} {
    error "usage: tray_integration.tcl extension-path client-path"
}
set extension [file normalize [lindex $argv 0]]
set client [file normalize [lindex $argv 1]]
load $extension Tclpanelx11

proc wait_for {condition message} {
    for {set i 0} {$i < 80} {incr i} {
        update
        if {[uplevel 1 [list expr $condition]]} { return }
        after 25
    }
    error $message
}

set callbackCount 0
proc tray_changed {} { incr ::callbackCount }
set channel [open |[list $client] r+]
fconfigure $channel -buffering line
set trayStarted 0
try {
    if {[gets $channel parentId] < 0} { error "test client did not create parent window" }
    set trayWindow [::x11::tray_start $parentId 48 24 tray_changed]
    set trayStarted 1

    puts $channel "dock $trayWindow"
    flush $channel
    if {[gets $channel iconId] < 0} { error "test client did not create tray icon" }
    wait_for {[::x11::tray_count] == 1} "tray did not accept dock request"

    puts $channel unmap
    flush $channel
    if {[gets $channel response] < 0 || $response ne "ok"} {
        error "test client failed to send unmap state"
    }
    update
    puts $channel map
    flush $channel
    if {[gets $channel response] < 0 || $response ne "ok"} {
        error "test client failed to send map state"
    }
    update
    if {[::x11::tray_count] != 1} { error "mapped-state changes removed tray icon" }

    puts $channel destroy
    flush $channel
    if {[gets $channel response] < 0 || $response ne "done"} {
        error "test client failed to destroy icon"
    }
    wait_for {[::x11::tray_count] == 0} "tray did not remove destroyed icon"
    puts "XEmbed integration test passed"
} finally {
    if {$trayStarted} { catch {::x11::tray_stop} }
    catch {puts $channel quit; flush $channel; close $channel}
}
