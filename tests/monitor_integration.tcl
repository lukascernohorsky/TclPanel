# Integration test for XRandR logical monitor changes.
# Xvfb provides a virtual output; xrandr --setmonitor/--delmonitor exercise
# monitor creation/removal notifications, not physical connector hotplug.
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
proc has_monitor {monitors name} {
    foreach monitor $monitors {
        if {[dict get $monitor name] eq $name} { return 1 }
    }
    return 0
}

set initial [::x11::monitors]
assert {[llength $initial] >= 1} "monitor query returned no monitor"
set first [lindex $initial 0]
assert {[dict exists $first width] && [dict exists $first height]} "monitor geometry fields missing"
assert {[dict get $first width] == 1280 && [dict get $first height] == 1024} \
    "unexpected initial Xvfb monitor geometry: $first"

# Exercise panel monitor selection and geometry calculation without requiring
# a window manager or a mapped Tk panel.
set selected [::tclpanel::panel::select_monitor $initial 0]
assert {[dict get $selected name] eq [dict get $first name]} "monitor index selection failed"
assert {[::tclpanel::panel::geometry_for_monitor $selected 36 bottom] eq "1280x36+0+988"} \
    "bottom panel geometry is incorrect"

set query [exec xrandr --query]
if {![regexp -line {^(\S+)\s+connected} $query -> output]} {
    puts stderr "FAIL: Xvfb did not expose a connected RandR output"
    exit 1
}
set test_name TclPanel-Test
set ::monitor_events 0
::x11::watch_monitors monitor_callback

if {[catch {exec xrandr --setmonitor $test_name 1280/340x1024/270+0+0 $output} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not create XRandR logical monitor: $err"
    exit 1
}
assert {[wait_for_monitor_event 0]} "no XRandR callback after monitor creation"
set with_test_monitor [::x11::monitors]
assert {[has_monitor $with_test_monitor $test_name]} \
    "new logical monitor is missing from ::x11::monitors"
set test_monitor {}
foreach item $with_test_monitor {
    if {[dict get $item name] eq $test_name} { set test_monitor $item; break }
}
assert {[dict get $test_monitor width] == 1280 && [dict get $test_monitor height] == 1024} \
    "new monitor geometry is incorrect: $test_monitor"
assert {[::tclpanel::panel::geometry_for_monitor $test_monitor 36 bottom] eq "1280x36+0+988"} \
    "panel geometry calculation failed for added monitor"

set before_remove $::monitor_events
if {[catch {exec xrandr --delmonitor $test_name} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not remove XRandR logical monitor: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_remove]} "no XRandR callback after monitor removal"
set after_remove [::x11::monitors]
assert {![has_monitor $after_remove $test_name]} \
    "deleted logical monitor is still present in ::x11::monitors"

::x11::unwatch_monitors
set events_after_unwatch $::monitor_events
if {[catch {exec xrandr --setmonitor $test_name 1280/340x1024/270+0+0 $output} err]} {
    puts stderr "FAIL: second monitor creation failed: $err"
    exit 1
}
after 300 {set ::settle 1}
vwait ::settle
assert {$::monitor_events == $events_after_unwatch} "callback fired after unwatch_monitors"
catch {exec xrandr --delmonitor $test_name}

puts "PASS: XRandR monitor query, add/remove notifications, panel geometry and watcher cleanup"
