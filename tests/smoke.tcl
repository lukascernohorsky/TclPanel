set root [file normalize [file join [file dirname [info script]] ..]]
namespace eval ::tclpanel {}
set ::tclpanel::root $root
source [file join $root src config.tcl]
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
assert {[llength [info commands ::tclpanel::xdg::discover]] == 1} "XDG discovery not loaded"
assert {[file isfile [file join $root c x11.c]]} "X11 extension source missing"
puts "TclPanel smoke tests passed"
