# Integration test for XRandR output state changes and monitor watching.
# Xvfb's virtual output is switched off/on; this tests RandR notifications and
# panel fallback behavior, but does not emulate physical connector hotplug.
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

set query [exec xrandr --query]
if {![regexp -line {^(\S+)\s+connected} $query -> output]} {
    puts stderr "FAIL: Xvfb did not expose a connected RandR output"
    exit 1
}

# Disabling an output generates RandR CRTC/output notifications. The panel
# must still receive a usable synthetic screen monitor when none are active.
set ::monitor_events 0
::x11::watch_monitors monitor_callback
if {[catch {exec xrandr --output $output --off} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not disable Xvfb RandR output: $err"
    exit 1
}
assert {[wait_for_monitor_event 0]} "no XRandR callback after output was disabled"
set inactive [::x11::monitors]
assert {[llength $inactive] == 1} "expected one synthetic screen monitor while output is disabled"
assert {[dict get [lindex $inactive 0] name] eq "screen"} \
    "inactive-output fallback did not return the synthetic screen monitor"
assert {[::tclpanel::panel::geometry_for_monitor [lindex $inactive 0] 36 bottom] eq "1280x36+0+988"} \
    "panel geometry is incorrect while output is disabled"

set before_enable $::monitor_events
if {[catch {exec xrandr --output $output --auto} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not re-enable Xvfb RandR output: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_enable]} "no XRandR callback after output was re-enabled"
assert {[llength [::x11::monitors]] >= 1} "monitor query returned no monitor after output re-enable"

::x11::unwatch_monitors
set events_after_unwatch $::monitor_events
if {[catch {exec xrandr --output $output --off} err]} {
    puts stderr "FAIL: could not disable output after unwatch: $err"
    exit 1
}
after 300 {set ::settle 1}
vwait ::settle
assert {$::monitor_events == $events_after_unwatch} "callback fired after unwatch_monitors"
catch {exec xrandr --output $output --auto}

puts "PASS: XRandR monitor query, output-change notifications, panel geometry and watcher cleanup"
