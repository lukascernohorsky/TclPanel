# Smoke test for the non-GUI configuration/module code.
set root [file normalize [file join [file dirname [info script]] ..]]
set ::tclpanel::root $root
source [file join $root src config.tcl]
source [file join $root src module_manager.tcl]
if {[::tclpanel::config::get height] != 36} { error "unexpected default panel height" }
if {[llength [::tclpanel::config::get modules]] < 1} { error "no default modules configured" }
puts "TclPanel smoke tests passed"
