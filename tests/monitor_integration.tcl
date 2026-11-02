# Integration test for XRandR screen-change notifications and monitor watching.
# Xvfb's framebuffer pixel dimensions are fixed in CI, so this test changes
# the reported physical screen size. It exercises RandR screen notifications,
# not physical connector hotplug.
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
proc screen_info {} {
    set query [exec xrandr --query]
    if {![regexp {current (\d+) x (\d+),\s*(\d+) x (\d+) mm} $query -> width height mm_width mm_height]} {
        error "cannot parse current XRandR screen dimensions"
    }
    return [list $width $height $mm_width $mm_height]
}

set initial [::x11::monitors]
assert {[llength $initial] >= 1} "monitor query returned no monitor"
set first [lindex $initial 0]
assert {[dict exists $first width] && [dict exists $first height]} "monitor geometry fields missing"
assert {[dict get $first width] == 1280 && [dict get $first height] == 1024} \
    "unexpected initial Xvfb monitor geometry: $first"
set initial_info [screen_info]
assert {[lrange $initial_info 0 1] eq {1280 1024}} "unexpected initial XRandR pixel dimensions"
set original_mm [lrange $initial_info 2 3]

# Exercise monitor selection and panel geometry without requiring a window
# manager or a mapped Tk panel.
set selected [::tclpanel::panel::select_monitor $initial 0]
assert {[dict get $selected name] eq [dict get $first name]} "monitor index selection failed"
assert {[::tclpanel::panel::geometry_for_monitor $selected 36 bottom] eq "1280x36+0+988"} \
    "bottom panel geometry is incorrect"

set ::monitor_events 0
::x11::watch_monitors monitor_callback
if {[catch {exec xrandr --fbmm 400x300} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not change Xvfb physical screen dimensions: $err"
    exit 1
}
assert {[wait_for_monitor_event 0]} "no XRandR callback after screen physical-size change"
set changed_info [screen_info]
assert {[lrange $changed_info 0 1] eq {1280 1024}} \
    "pixel dimensions changed unexpectedly: $changed_info"
assert {[lrange $changed_info 2 3] eq {400 300}} \
    "XRandR did not report the changed physical dimensions: $changed_info"
set changed_monitors [::x11::monitors]
assert {[llength $changed_monitors] >= 1} "monitor query returned no monitor after screen change"
assert {[::tclpanel::panel::geometry_for_monitor [lindex $changed_monitors 0] 36 bottom] eq "1280x36+0+988"} \
    "panel geometry changed unexpectedly after physical-size update"

set before_restore $::monitor_events
if {[catch {exec xrandr --fbmm [join $original_mm x]} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not restore original Xvfb physical dimensions: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_restore]} "no XRandR callback after physical-size restore"
assert {[lrange [screen_info] 2 3] eq $original_mm} "physical screen dimensions were not restored"

::x11::unwatch_monitors
set events_after_unwatch $::monitor_events
if {[catch {exec xrandr --fbmm 410x310} err]} {
    puts stderr "FAIL: screen change after unwatch failed: $err"
    exit 1
}
after 300 {set ::settle 1}
vwait ::settle
assert {$::monitor_events == $events_after_unwatch} "callback fired after unwatch_monitors"
catch {exec xrandr --fbmm [join $original_mm x]}

puts "PASS: XRandR screen-change notifications, monitor query, panel geometry and watcher cleanup"
