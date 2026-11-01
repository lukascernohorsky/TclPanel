namespace eval ::tclpanel::module::taskbar {}
proc ::tclpanel::module::taskbar::create {parent} {
    frame $parent.tasks -background [::tclpanel::config::get background]
    pack $parent.tasks -side left -fill both -expand 1
    ::tclpanel::module::taskbar::refresh $parent.tasks
}
proc ::tclpanel::module::taskbar::refresh {w} {
    if {![winfo exists $w]} return
    foreach child [winfo children $w] { destroy $child }
    if {[auto_execok wmctrl] ne "" && ![catch {exec wmctrl -l} lines]} {
        foreach line [split $lines \n] {
            if {[regexp {^\S+\s+\S+\s+\S+\s+(.+)$} $line -> title]} {
                button $w.b[incr n] -text $title -command [list ::tclpanel::module::taskbar::activate [lindex $line 0]] -relief flat
                pack $w.b$n -side left -fill y
            }
        }
    }
    after 3000 [list ::tclpanel::module::taskbar::refresh $w]
}
proc ::tclpanel::module::taskbar::activate {id} {
    if {[auto_execok wmctrl] ne ""} { catch {exec wmctrl -ia $id} }
}
