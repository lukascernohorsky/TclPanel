namespace eval ::tclpanel::module::taskbar {}

proc ::tclpanel::module::taskbar::create {parent} {
    frame $parent.tasks -background [::tclpanel::config::get background]
    pack $parent.tasks -side left -fill both -expand 1
    ::tclpanel::module::taskbar::refresh $parent.tasks
}

proc ::tclpanel::module::taskbar::refresh {w} {
    if {![winfo exists $w]} {
        return
    }
    foreach child [winfo children $w] {
        destroy $child
    }

    set n 0
    if {[auto_execok wmctrl] ne "" && ![catch {exec wmctrl -l} lines]} {
        foreach line [split $lines \n] {
            if {[regexp {^(\S+)\s+\S+\s+\S+\s+(.+)$} $line -> id title]} {
                set button $w.b[incr n]
                button $button -text $title \
                    -command [list ::tclpanel::module::taskbar::activate $id] \
                    -relief flat -takefocus 0
                pack $button -side left -fill y
            }
        }
    }
    after 3000 [list ::tclpanel::module::taskbar::refresh $w]
}

proc ::tclpanel::module::taskbar::activate {id} {
    if {[auto_execok wmctrl] ne ""} {
        catch {exec wmctrl -ia $id}
    }
}
