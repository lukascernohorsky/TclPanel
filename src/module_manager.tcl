namespace eval ::tclpanel::modules { variable loaded {} }
proc ::tclpanel::modules::load_all {} {
    variable loaded
    set dir [file join $::tclpanel::root src modules]
    foreach name [::tclpanel::config::get modules {}] {
        set file [file join $dir ${name}.tcl]
        if {![file isfile $file]} {
            puts stderr "TclPanel: module not found: $name"
            continue
        }
        if {[catch {source $file; ::tclpanel::module::${name}::create [::tclpanel::panel::content]} err]} {
            puts stderr "TclPanel: module $name failed: $err"
        } else {
            lappend loaded $name
        }
    }
}
