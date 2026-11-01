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
proc client_state {channel} {
    puts $channel state
    flush $channel
    if {[gets $channel value] < 0} { error "client state query failed" }
    return $value
}
proc wait_client_state {channel expected message} {
    for {set i 0} {$i < 80} {incr i} {
        update
        if {[client_state $channel] == $expected} { return }
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
    if {![catch {
        ::x11::tray_start 2147483647 48 24 tray_changed
    } invalidParentError]} {
        error "tray accepted a nonexistent parent window"
    }
    if {![string match "*tray parent*" $invalidParentError]} {
        error "unexpected invalid-parent error: $invalidParentError"
    }

    set trayWindow [::x11::tray_start $parentId 48 24 tray_changed]
    set trayStarted 1

    puts $channel "dock $trayWindow"
    flush $channel
    if {[gets $channel iconId] < 0} { error "test client did not create tray icon" }
    wait_for {[::x11::tray_count] == 1} "tray did not accept dock request"
    puts $channel embed
    flush $channel
    if {[gets $channel embedded] < 0 || $embedded != 1} {
        error "tray did not send XEMBED_EMBEDDED_NOTIFY"
    }
    wait_client_state $channel 2 "newly docked icon was not mapped"

    ::x11::tray_configure 100 24 16 4
    puts $channel geometry
    flush $channel
    if {[gets $channel geometry] < 0 || $geometry ne "0 4 16 16"} {
        error "tray icon was not centered and resized: $geometry"
    }

    puts $channel "dock2 $trayWindow"
    flush $channel
    if {[gets $channel icon2Id] < 0} { error "test client did not create second tray icon" }
    wait_for {[::x11::tray_count] == 2} "tray did not accept second dock request"
    puts $channel geometry2
    flush $channel
    if {[gets $channel geometry2] < 0 || $geometry2 ne "20 4 16 16"} {
        error "second tray icon was not placed after the first: $geometry2"
    }

    ::x11::tray_configure 100 32 20 5
    puts $channel geometry
    flush $channel
    if {[gets $channel geometry] < 0 || $geometry ne "0 6 20 20"} {
        error "first tray icon did not adapt to resized tray: $geometry"
    }
    puts $channel geometry2
    flush $channel
    if {[gets $channel geometry2] < 0 || $geometry2 ne "25 6 20 20"} {
        error "second tray icon did not adapt to resized tray: $geometry2"
    }

    puts $channel unmap
    flush $channel
    if {[gets $channel response] < 0 || $response ne "ok"} {
        error "test client failed to send unmap state"
    }
    wait_client_state $channel 0 "tray ignored _XEMBED_INFO unmapped flag"

    puts $channel map
    flush $channel
    if {[gets $channel response] < 0 || $response ne "ok"} {
        error "test client failed to send map state"
    }
    wait_client_state $channel 2 "tray ignored _XEMBED_INFO mapped flag"

    puts $channel destroy
    flush $channel
    if {[gets $channel response] < 0 || $response ne "done"} {
        error "test client failed to destroy first icon"
    }
    wait_for {[::x11::tray_count] == 1} "tray did not remove destroyed first icon"
    puts $channel geometry2
    flush $channel
    if {[gets $channel geometry2] < 0 || $geometry2 ne "0 6 20 20"} {
        error "tray did not compact icon layout after removal: $geometry2"
    }
    puts $channel destroy2
    flush $channel
    if {[gets $channel response] < 0 || $response ne "done"} {
        error "test client failed to destroy second icon"
    }
    wait_for {[::x11::tray_count] == 0} "tray did not remove final destroyed icon"
    if {$callbackCount < 2} { error "tray change callback was not invoked" }

    puts $channel steal
    flush $channel
    if {[gets $channel response] < 0 || $response ne "ok"} {
        error "test client failed to take tray selection"
    }
    wait_for {![::x11::tray_active]} "tray did not detect selection loss"
    wait_for {[::x11::tray_count] == 0} "tray did not clean up after selection loss"
    puts "XEmbed integration test passed"
} finally {
    if {$trayStarted} { catch {::x11::tray_stop} }
    catch {puts $channel quit; flush $channel; close $channel}
}
