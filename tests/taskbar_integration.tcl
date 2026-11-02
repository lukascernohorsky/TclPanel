package require Tk
set root [file normalize [file join [file dirname [info script]] ..]]
namespace eval ::tclpanel {}
set ::tclpanel::root $root
source [file join $root src config.tcl]
source [file join $root src modules taskbar.tcl]

namespace eval ::mock {
    variable windows {{16 Terminal 0} {32 Editor 1}}
    variable active 16
}
namespace eval ::x11 {}
proc ::x11::windows {} { return $::mock::windows }
proc ::x11::active_window {} { return $::mock::active }
proc ::x11::activate {id} { set ::mock::activated $id }

proc assert {condition message} {
    if {![uplevel 1 [list expr $condition]]} { error $message }
}
proc button_for {id} {
    return [dict get $::tclpanel::module::taskbar::buttons $id]
}

frame .tasks
set ::tclpanel::module::taskbar::widget .tasks
set ::tclpanel::module::taskbar::event_driven 1
::tclpanel::module::taskbar::refresh .tasks
set terminal [button_for 16]
set editor [button_for 32]
assert {[winfo exists $terminal] && [winfo exists $editor]} "initial taskbar buttons were not created"
assert {[$terminal cget -text] eq "Terminal"} "initial title was not set"
assert {[$terminal cget -relief] eq "sunken"} "active window was not highlighted"
assert {[$editor cget -relief] eq "flat"} "inactive window has active relief"
::tclpanel::module::taskbar::activate 32
assert {$::mock::activated == 32} "taskbar button activation did not call the X11 API"

set ::mock::windows {{16 {Terminal Updated} 0} {32 Editor 1}}
set ::mock::active 32
::tclpanel::module::taskbar::refresh .tasks
assert {[button_for 16] eq $terminal} "existing taskbar button was recreated"
assert {[button_for 32] eq $editor} "second taskbar button was recreated"
assert {[$terminal cget -text] eq "Terminal Updated"} "changed title was not applied"
assert {[$terminal cget -relief] eq "flat"} "previous active window was not reset"
assert {[$editor cget -relief] eq "sunken"} "new active window was not highlighted"

set ::mock::windows {{32 Editor 1} {48 Browser 0}}
::tclpanel::module::taskbar::refresh .tasks
assert {![winfo exists $terminal]} "removed window button still exists"
assert {[winfo exists [button_for 48]]} "new window button was not created"
assert {[lindex [pack slaves .tasks] 0] eq [button_for 32]} "taskbar order was not updated"
assert {[llength [pack slaves .tasks]] == 2} "taskbar contains stale or duplicate buttons"

destroy .tasks
puts "Taskbar incremental update integration test passed"
destroy .
