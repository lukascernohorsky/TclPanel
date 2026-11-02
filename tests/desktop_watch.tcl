if {$argc != 2} { error "usage: desktop_watch.tcl extension-path client-path" }
set extension [file normalize [lindex $argv 0]]
set client [file normalize [lindex $argv 1]]
load $extension Tclpanelx11
proc wait_for {condition message} {
    for {set i 0} {$i < 100} {incr i} {
        update
        if {[uplevel 1 [list expr $condition]]} { return }
        after 20
    }
    error $message
}
proc send_client {channel command} {
    puts $channel $command
    flush $channel
    if {[gets $channel response] < 0 || $response ne "ok"} { error "desktop test client failed command: $command" }
}
set callbacks 0
proc desktops_changed {} { incr ::callbacks }
set channel [open |[list $client] r+]
fconfigure $channel -buffering line
set watching 0
try {
    if {[gets $channel ready] < 0 || $ready ne "ready"} { error "desktop test client did not initialize" }
    if {[::x11::current_desktop] != 1 || [::x11::desktops] != 4} {
        error "initial EWMH desktop properties were not read correctly"
    }
    if {[::x11::desktop_names] ne {One Two Three Four}} {
        error "initial EWMH desktop names were not parsed: [::x11::desktop_names]"
    }
    ::x11::watch_desktops desktops_changed
    set watching 1
    send_client $channel "current 2"
    wait_for {$callbacks > 0} "_NET_CURRENT_DESKTOP change did not invoke callback"
    if {[::x11::current_desktop] != 2} { error "current desktop value was not updated" }
    set before $callbacks
    send_client $channel "count 5"
    wait_for {$callbacks > $before} "_NET_NUMBER_OF_DESKTOPS change did not invoke callback"
    if {[::x11::desktops] != 5} { error "desktop count was not updated" }
    set before $callbacks
    send_client $channel names
    wait_for {$callbacks > $before} "_NET_DESKTOP_NAMES change did not invoke callback"
    if {[::x11::desktop_names] ne {Alpha Beta Gamma}} {
        error "updated EWMH desktop names were not parsed: [::x11::desktop_names]"
    }
    ::x11::unwatch_desktops
    set watching 0
    set before $callbacks
    send_client $channel "current 3"
    after 100; update
    if {$callbacks != $before} { error "desktop callback ran after unwatch" }
    interp create child
    interp eval child [list load $extension Tclpanelx11]
    interp eval child {proc desktop_callback {} {}; ::x11::watch_desktops desktop_callback}
    interp delete child
    ::x11::watch_desktops desktops_changed
    set watching 1
    ::x11::unwatch_desktops
    set watching 0
    puts "EWMH desktop watcher integration test passed"
} finally {
    if {$watching} { catch {::x11::unwatch_desktops} }
    catch {puts $channel quit; flush $channel; close $channel}
}
