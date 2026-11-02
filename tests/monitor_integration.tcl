# Integration test for XRandR CRTC/mode changes and monitor watching.
# Xvfb switches between two virtual display modes; this exercises RandR events
# and panel geometry refreshes, but does not emulate physical connector hotplug.
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
assert {[dict get $first width] == 1280 && [dict get $first height] == 1024} \
    "unexpected initial Xvfb monitor geometry: $first"
assert {[screen_size] eq {1280 1024}} "unexpected initial XRandR screen size"

# Exercise monitor selection and panel geometry without requiring a window
# manager or a mapped Tk panel.
set selected [::tclpanel::panel::select_monitor $initial 0]
assert {[dict get $selected name] eq [dict get $first name]} "monitor index selection failed"
assert {[::tclpanel::panel::geometry_for_monitor $selected 36 bottom] eq "1280x36+0+988"} \
    "bottom panel geometry is incorrect"

set query [exec xrandr --query]
if {![regexp -line {^(\S+)\s+connected} $query -> output]} {
    puts stderr "FAIL: Xvfb did not expose a connected RandR output"
    exit 1
}

# Xvfb exposes a single initial mode. Add a second, smaller mode so that
# changing the CRTC mode produces a real RandR notification.
if {[catch {
    exec xrandr --newmode 1024x768 65.00 1024 1048 1184 1344 768 771 777 806 -hsync -vsync
    exec xrandr --addmode $output 1024x768
} err]} {
    puts stderr "FAIL: could not add alternate Xvfb mode: $err"
    exit 1
}

set ::monitor_events 0
::x11::watch_monitors monitor_callback
if {[catch {exec xrandr --output $output --mode 1024x768} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not switch Xvfb to alternate mode: $err"
    exit 1
}
assert {[wait_for_monitor_event 0]} "no XRandR callback after CRTC mode change"
assert {[screen_size] eq {1024 768}} "XRandR screen size did not change to 1024x768"
set resized [::x11::monitors]
assert {[llength $resized] >= 1} "monitor query returned no monitor after mode change"
assert {[dict get [lindex $resized 0] width] == 1024 &&
        [dict get [lindex $resized 0] height] == 768} \
    "monitor query did not report the new mode: $resized"
assert {[::tclpanel::panel::geometry_for_monitor [lindex $resized 0] 36 bottom] eq "1024x36+0+732"} \
    "panel geometry was not recalculated for the alternate mode"

set before_restore $::monitor_events
if {[catch {exec xrandr --output $output --mode 1280x1024} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not restore original Xvfb mode: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_restore]} "no XRandR callback after CRTC mode restore"
assert {[screen_size] eq {1280 1024}} "XRandR screen size was not restored"

::x11::unwatch_monitors
set events_after_unwatch $::monitor_events
if {[catch {exec xrandr --output $output --mode 1024x768} err]} {
    puts stderr "FAIL: CRTC mode change after unwatch failed: $err"
    exit 1
}
after 300 {set ::settle 1}
vwait ::settle
assert {$::monitor_events == $events_after_unwatch} "callback fired after unwatch_monitors"
catch {exec xrandr --output $output --mode 1280x1024}

puts "PASS: XRandR CRTC mode notifications, monitor query, panel geometry and watcher cleanup"
