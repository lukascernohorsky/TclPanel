# Integration test for XRandR screen-change notifications and monitor watching.
# Xvfb has fixed pixel dimensions in CI; changing physical dimensions exercises
# RandR screen notifications, while --setmonitor exercises logical topology
# changes. Neither operation simulates a physical connector hotplug.
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
proc monitor_named {monitors wanted} {
    foreach monitor $monitors {
        if {[dict get $monitor name] eq $wanted} {
            return $monitor
        }
    }
    return {}
}

set initial [::x11::monitors]
assert {[llength $initial] >= 1} "monitor query returned no monitor"
set first [lindex $initial 0]
assert {[dict exists $first width] && [dict exists $first height]} "monitor geometry fields missing"
assert {[dict get $first width] == 1280 && [dict get $first height] == 1024} \
    "unexpected initial Xvfb monitor geometry: $first"

# Exercise monitor selection and panel geometry without requiring a window
# manager or a mapped Tk panel.
set selected [::tclpanel::panel::select_monitor $initial 0]
assert {[dict get $selected name] eq [dict get $first name]} "monitor index selection failed"
assert {[::tclpanel::panel::geometry_for_monitor $selected 36 bottom] eq "1280x36+0+988"} \
    "bottom panel geometry is incorrect"

set ::monitor_events 0
::x11::watch_monitors monitor_callback

# Xvfb keeps its pixel framebuffer fixed, but RandR still emits a screen-change
# notification when the reported physical dimensions change.
if {[catch {exec xrandr --fbmm 400x300} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not change Xvfb physical screen dimensions: $err"
    exit 1
}
assert {[wait_for_monitor_event 0]} "no XRandR callback after screen physical-size change"
set changed [::x11::monitors]
assert {[llength $changed] >= 1} "monitor query returned no monitor after screen change"
assert {[dict get [lindex $changed 0] width] == 1280 &&
        [dict get [lindex $changed 0] height] == 1024} \
    "monitor pixel geometry changed unexpectedly: $changed"
assert {[::tclpanel::panel::geometry_for_monitor [lindex $changed 0] 36 bottom] eq "1280x36+0+988"} \
    "panel geometry changed unexpectedly after physical-size update"

# Add a synthetic logical monitor with no physical output. This exercises the
# RRNotify/resource-change path and verifies that the query returns fresh
# geometry rather than merely invoking the callback.
set before_add $::monitor_events
if {[catch {exec xrandr --setmonitor TclPanelTestRight 640/160x1024/270+640+0 none} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not add synthetic RandR monitor: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_add]} "no XRandR callback after logical monitor addition"
set with_logical [::x11::monitors]
set logical [monitor_named $with_logical TclPanelTestRight]
assert {$logical ne ""} "new logical monitor missing from ::x11::monitors: $with_logical"
assert {[dict get $logical x] == 640 && [dict get $logical y] == 0 &&
        [dict get $logical width] == 640 && [dict get $logical height] == 1024} \
    "logical monitor geometry is incorrect: $logical"
assert {[::tclpanel::panel::geometry_for_monitor $logical 36 bottom] eq "640x36+640+988"} \
    "panel geometry for logical monitor is incorrect"

# Removing the logical monitor must also trigger a refresh and remove it from
# the current monitor snapshot.
set before_remove $::monitor_events
if {[catch {exec xrandr --delmonitor TclPanelTestRight} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not remove synthetic RandR monitor: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_remove]} "no XRandR callback after logical monitor removal"
set without_logical [::x11::monitors]
assert {[monitor_named $without_logical TclPanelTestRight] eq ""} \
    "removed logical monitor is still reported: $without_logical"

::x11::unwatch_monitors
set events_after_unwatch $::monitor_events
if {[catch {exec xrandr --fbmm 410x310} err]} {
    puts stderr "FAIL: screen change after unwatch failed: $err"
    exit 1
}
after 300 {set ::settle 1}
vwait ::settle
assert {$::monitor_events == $events_after_unwatch} "callback fired after unwatch_monitors"

puts "PASS: XRandR screen notifications, logical monitor add/remove, fresh geometry and watcher cleanup"
