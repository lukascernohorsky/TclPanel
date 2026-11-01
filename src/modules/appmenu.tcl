# Placeholder module: XDG .desktop discovery and safe Exec field-code expansion are not implemented yet.
namespace eval ::tclpanel::module::appmenu {}
proc ::tclpanel::module::appmenu::create {parent} {
    button $parent.appmenu -text "Menu" -state disabled
    pack $parent.appmenu -side left -fill y
}
