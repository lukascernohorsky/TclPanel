package require Tk
set root [file normalize [file join [file dirname [info script]] ..]]
namespace eval ::tclpanel {}
set ::tclpanel::root $root
source [file join $root src config.tcl]
source [file join $root src modules workspace.tcl]

namespace eval ::mock {
    variable current 1
    variable count 3
    variable names {Main Development Chat}
    variable callback {}
    variable stopped 0
    variable selected {}
}
namespace eval ::x11 {}
proc ::x11::current_desktop {} { return $::mock::current }
proc ::x11::desktops {} { return $::mock::count }
proc ::x11::desktop_names {} { return $::mock::names }
proc ::x11::set_desktop {index} {
    lappend ::mock::selected $index
    set ::mock::current $index
}
proc ::x11::watch_desktops {callback} { set ::mock::callback $callback }
proc ::x11::unwatch_desktops {} {
    set ::mock::stopped 1
    set ::mock::callback {}
}
proc ::mock::trigger {} {
    if {$::mock::callback ne ""} { uplevel #0 $::mock::callback }
}
proc assert {condition message} {
    if {![uplevel 1 [list expr $condition]]} { error $message }
}

frame .panel
::tclpanel::module::workspace::create .panel
assert {[.panel.workspace.label cget -text] eq "2: Development"} "current desktop name was not displayed"
assert {[.panel.workspace.prev cget -state] eq "normal"} "previous button should be enabled"
assert {[.panel.workspace.next cget -state] eq "normal"} "next button should be enabled"

set ::mock::current 2
set ::mock::names {Main Development Chat}
::mock::trigger
assert {[.panel.workspace.label cget -text] eq "3: Chat"} "desktop watcher did not refresh the displayed name"
assert {[.panel.workspace.next cget -state] eq "disabled"} "next button should be disabled on the last desktop"

::tclpanel::module::workspace::step -1
assert {$::mock::selected eq {1}} "workspace previous action did not select the expected desktop"
set ::mock::current 1
::mock::trigger
assert {[.panel.workspace.label cget -text] eq "2: Development"} "workspace label did not follow selected desktop"

destroy .panel
assert {$::mock::stopped} "workspace destruction did not unregister desktop watcher"
destroy .
puts "Workspace names integration test passed"
