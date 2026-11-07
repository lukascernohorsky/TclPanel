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
assert {[::tclpanel::config::get monitor] eq "primary"} "default monitor selection should prefer the primary monitor"
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
set selected [::tclpanel::panel::select_monitor $fakeMonitors primary]
assert {[dict get $selected name] eq "HDMI-1"} "primary monitor selection failed"
set selected [::tclpanel::panel::select_monitor $fakeMonitors 1]
assert {[dict get $selected name] eq "DP-1"} "monitor index selection failed"
set selected [::tclpanel::panel::select_monitor $fakeMonitors DP-1]
assert {[dict get $selected name] eq "DP-1"} "monitor name selection failed"
set selected [::tclpanel::panel::select_monitor $fakeMonitors missing-output]
assert {[dict get $selected name] eq "HDMI-1"} "missing monitor name should fall back to primary"
set selected [::tclpanel::panel::select_monitor $fakeMonitors 99]
assert {[dict get $selected name] eq "HDMI-1"} "out-of-range monitor index should fall back to primary"
assert {[::tclpanel::panel::geometry_for_monitor $selected 40 bottom] eq "1920x40+0+1040"} "bottom monitor geometry calculation failed"
assert {[::tclpanel::panel::geometry_for_monitor $selected 40 top] eq "1920x40+0+0"} "top monitor geometry calculation failed"
set leftMonitor [dict create name DP-2 x -1600 y -200 width 1600 height 900 primary 0]
assert {[::tclpanel::panel::geometry_for_monitor $leftMonitor 32 bottom] eq "1600x32-1600+668"} "negative-origin bottom monitor geometry failed"
assert {[::tclpanel::panel::geometry_for_monitor $leftMonitor 32 top] eq "1600x32-1600-200"} "negative-origin top monitor geometry failed"
proc winfo {subcommand widget} {
    if {$subcommand eq "screenwidth"} { return 2560 }
    if {$subcommand eq "screenheight"} { return 1440 }
    error "unexpected winfo query: $subcommand"
}
set fallback [::tclpanel::panel::select_monitor {} primary]
assert {[dict get $fallback name] eq "screen" && [dict get $fallback width] == 2560 && [dict get $fallback height] == 1440} "empty monitor list fallback failed"
assert {[llength [info commands ::tclpanel::xdg::discover]] == 1} "XDG discovery not loaded"
assert {[file isfile [file join $root c x11.c]]} "X11 extension source missing"
puts "TclPanel smoke tests passed"
