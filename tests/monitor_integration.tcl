# Integration test for XRandR screen changes and the monitor watcher.
# This uses Xvfb + xrandr framebuffer resizing; it does not emulate physical
# connector hotplug, but exercises RandR notifications and monitor refreshes.
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

set initial [::x11::monitors]
assert {[llength $initial] >= 1} "monitor query returned no monitor"
set first [lindex $initial 0]
assert {[dict exists $first width] && [dict exists $first height]} "monitor geometry fields missing"
assert {[dict get $first width] == 1280 && [dict get $first height] == 1024} \
    "unexpected initial Xvfb framebuffer geometry: $first"

# Exercise panel monitor selection and geometry calculation without requiring
# a window manager or a mapped Tk panel.
set selected [::tclpanel::panel::select_monitor $initial 0]
assert {[dict get $selected name] eq [dict get $first name]} "monitor index selection failed"
assert {[::tclpanel::panel::geometry_for_monitor $selected 36 bottom] eq "1280x36+0+988"} \
    "bottom panel geometry is incorrect"

set ::monitor_events 0
set ::monitor_done 0
::x11::watch_monitors monitor_callback
if {[catch {exec xrandr --fb 1024x768} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: xrandr framebuffer resize failed: $err"
    exit 1
}
after 4000 {set ::monitor_done timeout}
vwait ::monitor_done
assert {$::monitor_done ne "timeout"} "no XRandR callback after framebuffer resize"
assert {$::monitor_events > 0} "monitor callback count did not increase"

set resized [::x11::monitors]
set resized_first [lindex $resized 0]
assert {[dict get $resized_first width] == 1024 && [dict get $resized_first height] == 768} \
    "monitor query did not report resized framebuffer: $resized_first"
assert {[::tclpanel::panel::geometry_for_monitor $resized_first 36 bottom] eq "1024x36+0+732"} \
    "panel geometry was not recalculated for resized monitor"

::x11::unwatch_monitors
set events_after_unwatch $::monitor_events
if {[catch {exec xrandr --fb 800x600} err]} {
    puts stderr "FAIL: second xrandr framebuffer resize failed: $err"
    exit 1
}
after 300 {set ::settle 1}
vwait ::settle
assert {$::monitor_events == $events_after_unwatch} "callback fired after unwatch_monitors"

puts "PASS: XRandR monitor query, notification, geometry refresh and watcher cleanup"
