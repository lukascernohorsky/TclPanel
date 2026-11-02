if {$argc != 2} { error "usage: window_watch.tcl extension-path client-path" }
set extension [file normalize [lindex $argv 0]]
set client [file normalize [lindex $argv 1]]
load $extension Tclpanelx11
proc assert {condition message} {
    if {![uplevel 1 [list expr $condition]]} { error $message }
}
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
    if {[gets $channel response] < 0 || $response ne "ok"} { error "window test client failed command: $command" }
}
set callbacks 0
proc windows_changed {} { incr ::callbacks }
set channel [open |[list $client] r+]
fconfigure $channel -buffering line
set watching 0
try {
    if {[gets $channel windowId] < 0} { error "window test client did not create a window" }
    set initial [::x11::windows]
    assert {[llength $initial] == 1} "EWMH client window was not enumerated: $initial"
    assert {[lindex [lindex $initial 0] 0] == $windowId} "enumerated window ID did not match test client"
    ::x11::watch_windows windows_changed
    set watching 1
    send_client $channel "title changed title"
    wait_for {$callbacks > 0} "title property change did not invoke watcher callback"
    set found [::x11::windows]
    assert {[lindex [lindex $found 0] 1] eq "changed title"} "window title was not refreshed: $found"
    set before $callbacks
    send_client $channel unmap
    wait_for {$callbacks > $before} "UnmapNotify did not invoke watcher callback"
    set before $callbacks
    send_client $channel map
    wait_for {$callbacks > $before} "MapNotify did not invoke watcher callback"
    ::x11::unwatch_windows
    set watching 0
    set before $callbacks
    send_client $channel "title after unwatch"
    after 100; update
    assert {$callbacks == $before} "callback ran after unwatch"
    ::x11::watch_windows windows_changed
    set watching 1
    set before $callbacks
    send_client $channel remove
    wait_for {$callbacks > $before} "_NET_CLIENT_LIST change did not invoke watcher callback"
    assert {[llength [::x11::windows]] == 0} "removed window remained in EWMH enumeration"
    send_client $channel destroy
    ::x11::unwatch_windows
    set watching 0
    interp create child
    interp eval child [list load $extension Tclpanelx11]
    interp eval child {proc child_callback {} {}; ::x11::watch_windows child_callback}
    interp delete child
    ::x11::watch_windows windows_changed
    set watching 1
    ::x11::unwatch_windows
    set watching 0
    puts "X11 window watcher integration test passed"
} finally {
    if {$watching} { catch {::x11::unwatch_windows} }
    catch {puts $channel quit; flush $channel; close $channel}
}
