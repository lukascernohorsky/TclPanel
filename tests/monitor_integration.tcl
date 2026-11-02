# Integration test for XRandR screen-size changes and monitor watching.
# Xvfb's virtual framebuffer is resized; this exercises RandR screen-change
# notifications, but does not emulate physical connector hotplug.
if {$argc != 1} {
    puts stderr "usage: monitor_integration.tcl path-to-lib"
    exit 2
}
load [lindex $argv 0] Tclpanelx11
source [file join [file dirname [info script]] .. src panel.tcl]

proc assert {condition message} {
    if {![uplevel 1 [list expr $condition]]} {
        puts stderr "FAIL: $message"
        exit 1
    }
}
proc monitor_callback {} {
    incr ::monitor_events
    set ::monitor_done 1
}
proc wait_for_monitor_event {previous_count} {
    set ::monitor_done 0
    set timeout_id [after 4000 {set ::monitor_done timeout}]
    vwait ::monitor_done
    after cancel $timeout_id
    return [expr {$::monitor_done ne "timeout" &&
                  $::monitor_events > $previous_count}]
}
proc screen_size {} {
    set query [exec xrandr --query]
    if {![regexp {current (\d+) x (\d+)} $query -> width height]} {
        error "cannot parse current XRandR screen size"
    }
    return [list $width $height]
}

set initial [::x11::monitors]
assert {[llength $initial] >= 1} "monitor query returned no monitor"
set first [lindex $initial 0]
assert {[dict exists $first width] && [dict exists $first height]} "monitor geometry fields missing"
assert {[dict get $first width] == 1280 && [dict get $first height] == 1024} \
    "unexpected initial Xvfb monitor geometry: $first"
assert {[screen_size] eq {1280 1024}} "unexpected initial XRandR screen size"

# Exercise monitor selection and panel geometry without requiring a window
# manager or a mapped Tk panel.
set selected [::tclpanel::panel::select_monitor $initial 0]
assert {[dict get $selected name] eq [dict get $first name]} "monitor index selection failed"
assert {[::tclpanel::panel::geometry_for_monitor $selected 36 bottom] eq "1280x36+0+988"} \
    "bottom panel geometry is incorrect"

set ::monitor_events 0
::x11::watch_monitors monitor_callback
if {[catch {exec xrandr --fb 1600x1200} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not enlarge Xvfb framebuffer: $err"
    exit 1
}
assert {[wait_for_monitor_event 0]} "no XRandR callback after framebuffer enlargement"
assert {[screen_size] eq {1600 1200}} "XRandR screen size did not change after enlargement"
assert {[llength [::x11::monitors]] >= 1} "monitor query returned no monitor after screen change"

set before_restore $::monitor_events
if {[catch {exec xrandr --fb 1280x1024} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not restore Xvfb framebuffer: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_restore]} "no XRandR callback after framebuffer restore"
assert {[screen_size] eq {1280 1024}} "XRandR screen size was not restored"

::x11::unwatch_monitors
set events_after_unwatch $::monitor_events
if {[catch {exec xrandr --fb 1600x1200} err]} {
    puts stderr "FAIL: framebuffer resize after unwatch failed: $err"
    exit 1
}
after 300 {set ::settle 1}
vwait ::settle
assert {$::monitor_events == $events_after_unwatch} "callback fired after unwatch_monitors"
catch {exec xrandr --fb 1280x1024}

puts "PASS: XRandR screen-change notifications, monitor query, panel geometry and watcher cleanup"
