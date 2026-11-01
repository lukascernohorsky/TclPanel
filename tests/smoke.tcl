set root [file normalize [file join [file dirname [info script]] ..]]
namespace eval ::tclpanel {}
set ::tclpanel::root $root
source [file join $root src config.tcl]
source [file join $root src panel.tcl]
source [file join $root src module_manager.tcl]
source [file join $root src xdg_apps.tcl]
proc assert {condition message} {
    if {![uplevel 1 [list expr $condition]]} { error $message }
}
assert {[::tclpanel::config::get height] == 36} "unexpected default panel height"
assert {[llength [::tclpanel::config::get modules]] > 0} "no default modules configured"
assert {[::tclpanel::config::get missing fallback] eq "fallback"} "config fallback failed"
foreach module [glob -nocomplain [file join $root src modules *.tcl]] {
    if {[catch {source $module} err opts]} { return -options $opts "module syntax error in $module: $err" }
}
assert {[llength [info commands ::tclpanel::module::taskbar::refresh]] == 1} "taskbar module not loaded"
assert {[llength [info commands ::tclpanel::module::workspace::step]] == 1} "workspace module not loaded"
assert {[llength [info commands ::tclpanel::module::tray::create]] == 1} "tray module not loaded"
set fakeMonitors [list \
    [dict create name HDMI-1 x 0 y 0 width 1920 height 1080 primary 1] \
    [dict create name DP-1 x 1920 y 0 width 2560 height 1440 primary 0]]
set selected [::tclpanel::panel::select_monitor $fakeMonitors 1]
assert {[dict get $selected name] eq "DP-1"} "monitor index selection failed"
set selected [::tclpanel::panel::select_monitor $fakeMonitors HDMI-1]
assert {[dict get $selected name] eq "HDMI-1"} "monitor name selection failed"
assert {[::tclpanel::panel::geometry_for_monitor $selected 40 bottom] eq "1920x40+0+1040"} "monitor geometry calculation failed"
assert {[llength [info commands ::tclpanel::xdg::discover]] == 1} "XDG discovery not loaded"
assert {[file isfile [file join $root c x11.c]]} "X11 extension source missing"
puts "TclPanel smoke tests passed"
