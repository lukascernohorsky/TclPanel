# Module behavior tests using mocked X11 commands; no display is required.
set root [file normalize [file join [file dirname [info script]] ..]]
namespace eval ::tclpanel {}
set ::tclpanel::root $root
source [file join $root src config.tcl]
source [file join $root src/modules/taskbar.tcl]
source [file join $root src/modules/workspace.tcl]

namespace eval ::mock {}
set ::mock::current 0
set ::mock::count 3
set ::mock::activated {}
set ::mock::selected {}
namespace eval ::x11 {}
proc ::x11::windows {} { return {{0x10 "Terminal" 0} {0x20 "Editor" 1}} }
proc ::x11::activate {id} { lappend ::mock::activated $id }
proc ::x11::current_desktop {} { return $::mock::current }
proc ::x11::desktops {} { return $::mock::count }
proc ::x11::set_desktop {index} { lappend ::mock::selected $index }

proc assert_equal {actual expected message} {
    if {$actual ne $expected} { error "$message: expected <$expected>, got <$actual>" }
}

::tclpanel::module::taskbar::activate 0x10
assert_equal $::mock::activated {0x10} "taskbar activation dispatch"
::tclpanel::module::workspace::step 1
assert_equal $::mock::selected {1} "workspace next dispatch"
::tclpanel::module::workspace::step -1
assert_equal $::mock::selected {1 0} "workspace previous dispatch"
set ::mock::selected {}
set ::mock::current 2
::tclpanel::module::workspace::step 1
assert_equal $::mock::selected {} "workspace upper-bound guard"
set ::mock::current 0
::tclpanel::module::workspace::step -1
assert_equal $::mock::selected {} "workspace lower-bound guard"
puts "Module tests passed (5 assertions)"
