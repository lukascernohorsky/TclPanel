# Non-GUI tests: these run with tclsh and do not require an X server.
set root [file normalize [file join [file dirname [info script]] ..]]
namespace eval ::tclpanel {}
set ::tclpanel::root $root
source [file join $root src config.tcl]
source [file join $root src module_manager.tcl]

proc assert {condition message} {
    if {![uplevel 1 [list expr $condition]]} {
        error $message
    }
}

assert {[::tclpanel::config::get height] == 36} "unexpected default panel height"
assert {[llength [::tclpanel::config::get modules]] > 0} "no default modules configured"
assert {[::tclpanel::config::get missing fallback] eq "fallback"} "config fallback failed"
assert {[file isfile [file join $root src main.tcl]]} "main entry point missing"
assert {[file isfile [file join $root c x11.c]]} "X11 extension source missing"

puts "TclPanel smoke tests passed"
