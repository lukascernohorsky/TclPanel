# Platform-specific CPU/memory backends are not implemented yet.
namespace eval ::tclpanel::module::system_monitor {}
proc ::tclpanel::module::system_monitor::create {parent} {
    label $parent.monitor -text "System" -padx 8 -background [::tclpanel::config::get background] -foreground [::tclpanel::config::get foreground]
    pack $parent.monitor -side right -fill y
}
