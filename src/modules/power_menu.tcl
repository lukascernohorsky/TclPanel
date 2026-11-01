# Power actions are intentionally disabled until a platform-specific policy backend is added.
namespace eval ::tclpanel::module::power_menu {}
proc ::tclpanel::module::power_menu::create {parent} {
    button $parent.power -text "Power" -state disabled
    pack $parent.power -side right -fill y
}
