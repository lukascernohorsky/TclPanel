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
proc monitor_restart_callback {} {
    # Ignore delayed notifications from earlier monitor changes. Restart only
    # when the monitor introduced by this test is actually present.
    set before_query [::x11::monitors]
    set before_unwatch_xrandr [exec xrandr --listmonitors]
    puts stderr "RESTARTDBG callback query=$before_query xrandr=$before_unwatch_xrandr"
    if {[monitor_named $before_query TclPanelRestartProbe] eq ""} {
        return
    }
    incr ::monitor_events
    # Restart from inside the timer-driven callback. The expired timer must
    # not overwrite the timer installed by this new watcher.
    ::x11::unwatch_monitors
    set after_unwatch_query [::x11::monitors]
    set after_unwatch_xrandr [exec xrandr --listmonitors]
    puts stderr "RESTARTDBG after-unwatch monitors=$after_unwatch_query xrandr=$after_unwatch_xrandr"
    ::x11::watch_monitors monitor_callback
    puts stderr "RESTARTDBG after-watch monitors=[::x11::monitors]"
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

# Rename/reconfigure the Xvfb output as a synthetic logical monitor. Binding
# the monitor to the active output ensures XRRGetMonitors(get_active=True)
# reports it, and exercises the RRNotify/resource-change path.
set before_add $::monitor_events
if {[catch {exec xrandr --setmonitor TclPanelTestRight 1280/340x1024/270+0+0 screen} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not add synthetic RandR monitor: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_add]} "no XRandR callback after logical monitor addition"
set with_logical [::x11::monitors]
set logical [monitor_named $with_logical TclPanelTestRight]
assert {$logical ne ""} "new logical monitor missing from ::x11::monitors: $with_logical"
assert {[dict get $logical x] == 0 && [dict get $logical y] == 0 &&
        [dict get $logical width] == 1280 && [dict get $logical height] == 1024} \
    "logical monitor geometry is incorrect: $logical"
assert {[::tclpanel::panel::geometry_for_monitor $logical 36 bottom] eq "1280x36+0+988"} \
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

# Exercise watcher replacement from inside a timer-driven callback.
# Adding a logical monitor is detected by polling; the callback restarts the
# watcher. A subsequent removal must be observed by the replacement watcher.
set ::monitor_events 0
::x11::watch_monitors monitor_restart_callback
set before_restart_probe $::monitor_events
if {[catch {exec xrandr --setmonitor TclPanelRestartProbe 1280/340x1024/270+0+0 screen} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not add restart-probe monitor: $err"
    exit 1
}
assert {[wait_for_monitor_event $before_restart_probe]} \
    "monitor callback did not run while replacing its watcher"
# Let the callback's Tcl evaluation and any queued XRandR notifications fully
# unwind before issuing the next topology change. The replacement watcher must
# then detect a fresh change using its own polling timer.
after 700 {set ::restart_settled 1}
vwait ::restart_settled
set before_restart_remove $::monitor_events
# Capture the server-side state immediately before removal. This distinguishes
# a watcher snapshot problem from an Xvfb/xrandr topology change that vanished
# during the callback-driven connection replacement.
set restart_probe_baseline [::x11::monitors]
set restart_probe_xrandr [exec xrandr --listmonitors]
if {[catch {exec xrandr --delmonitor TclPanelRestartProbe} err]} {
    ::x11::unwatch_monitors
    puts stderr "FAIL: could not remove restart-probe monitor: $err"
    exit 1
}
set restart_probe_state [::x11::monitors]
set restart_probe_waited [wait_for_monitor_event $before_restart_remove]
assert {$restart_probe_waited} \
    "replacement monitor watcher stopped polling after callback restart (events=$::monitor_events, before=$before_restart_remove, baseline=$restart_probe_baseline, xrandr=$restart_probe_xrandr, after=$restart_probe_state)"

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
