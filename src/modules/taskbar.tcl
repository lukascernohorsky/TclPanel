namespace eval ::tclpanel::module::taskbar {
    variable widget {}
    variable event_driven 0
}

proc ::tclpanel::module::taskbar::create {parent} {
    variable widget
    variable event_driven
    frame $parent.tasks -background [::tclpanel::config::get background]
    pack $parent.tasks -side left -fill both -expand 1
    set widget $parent.tasks
    set event_driven 0
    if {[llength [info commands ::x11::watch_windows]] &&
        ![catch {::x11::watch_windows ::tclpanel::module::taskbar::window_changed}]} {
        set event_driven 1
    }
    bind $widget <Destroy> [list ::tclpanel::module::taskbar::destroyed %W]
    ::tclpanel::module::taskbar::refresh $widget
}

proc ::tclpanel::module::taskbar::destroyed {w} {
    variable widget
    variable event_driven
    if {$w ne $widget} { return }
    if {$event_driven && [llength [info commands ::x11::unwatch_windows]]} {
        catch {::x11::unwatch_windows}
    }
    set widget {}
    set event_driven 0
}

proc ::tclpanel::module::taskbar::window_changed {} {
    variable widget
    if {$widget ne {} && [winfo exists $widget]} {
        ::tclpanel::module::taskbar::refresh $widget
    }
}

proc ::tclpanel::module::taskbar::refresh {w} {
    variable event_driven
    if {![winfo exists $w]} { return }
    foreach child [winfo children $w] { destroy $child }

    set n 0
    if {[llength [info commands ::x11::windows]]} {
        if {![catch {::x11::windows} windows]} {
            foreach entry $windows {
                lassign $entry id title desktop
                set button $w.b[incr n]
                button $button -text $title \
                    -command [list ::tclpanel::module::taskbar::activate $id] \
                    -relief flat -takefocus 0
                pack $button -side left -fill y
            }
        }
    } elseif {[auto_execok wmctrl] ne "" && ![catch {exec wmctrl -l} lines]} {
        foreach line [split $lines \n] {
            if {[regexp {^(\S+)\s+\S+\s+\S+\s+(.+)$} $line -> id title]} {
                set button $w.b[incr n]
                button $button -text $title \
                    -command [list ::tclpanel::module::taskbar::activate_wmctrl $id] \
                    -relief flat -takefocus 0
                pack $button -side left -fill y
            }
        }
    }
    if {!$event_driven} {
        after 3000 [list ::tclpanel::module::taskbar::refresh $w]
    }
}

proc ::tclpanel::module::taskbar::activate {id} {
    catch {::x11::activate $id}
}
proc ::tclpanel::module::taskbar::activate_wmctrl {id} {
    if {[auto_execok wmctrl] ne ""} { catch {exec wmctrl -ia $id} }
}
