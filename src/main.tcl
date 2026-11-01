#!/usr/bin/env wish
package require Tk 8.6

namespace eval ::tclpanel {}
set ::tclpanel::root [file normalize [file join [file dirname [info script]] ..]]

source [file join $::tclpanel::root src config.tcl]
source [file join $::tclpanel::root src panel.tcl]
source [file join $::tclpanel::root src module_manager.tcl]

if {[info exists ::env(TCLPANEL_LIB)] && [file isfile $::env(TCLPANEL_LIB)]} {
    if {[catch {load $::env(TCLPANEL_LIB) Tclpanelx11} err]} {
        puts stderr "TclPanel: X11 extension unavailable: $err"
    }
}

::tclpanel::config::load
::tclpanel::panel::create
::tclpanel::modules::load_all
