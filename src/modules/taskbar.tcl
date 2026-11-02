namespace eval ::tclpanel::module::taskbar {
    variable widget {}
    variable event_driven 0
    variable buttons {}
    variable titles {}
    variable active_id 0
    variable ordered_ids {}
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
    variable buttons
    variable titles
    variable active_id
    variable ordered_ids
    if {$w ne $widget} { return }
    if {$event_driven && [llength [info commands ::x11::unwatch_windows]]} {
        catch {::x11::unwatch_windows}
    }
    set widget {}
    set event_driven 0
    set buttons {}
    set titles {}
    set active_id 0
    set ordered_ids {}
}

proc ::tclpanel::module::taskbar::window_changed {} {
    variable widget
    if {$widget ne {} && [winfo exists $widget]} {
        ::tclpanel::module::taskbar::refresh $widget
    }
}

proc ::tclpanel::module::taskbar::refresh {w} {
    variable event_driven
    variable buttons
    variable titles
    variable active_id
    variable ordered_ids
    if {![winfo exists $w]} { return }

    set entries {}
    set new_active 0
    if {[llength [info commands ::x11::windows]]} {
        if {![catch {::x11::windows} windows]} {
            set entries $windows
        }
        if {[llength [info commands ::x11::active_window]]} {
            catch {set new_active [::x11::active_window]}
        }
    } elseif {[auto_execok wmctrl] ne "" && ![catch {exec wmctrl -l} lines]} {
        foreach line [split $lines \n] {
            if {[regexp {^(\S+)\s+\S+\s+\S+\s+(.+)$} $line -> id title]} {
                lappend entries [list $id $title 0]
            }
        }
    }

    set present {}
    set order {}
    foreach entry $entries {
        lassign $entry id title desktop
        dict set present $id 1
        lappend order $id
        if {![dict exists $buttons $id] || ![winfo exists [dict get $buttons $id]]} {
            set button $w.window_[string map {":" "_" "." "_" "x" "x"} $id]
            button $button -text $title -command [list ::tclpanel::module::taskbar::activate $id] \
                -relief flat -takefocus 0
            dict set buttons $id $button
        } else {
            set button [dict get $buttons $id]
        }
        if {![dict exists $titles $id] || [dict get $titles $id] ne $title} {
            $button configure -text $title
            dict set titles $id $title
        }
        set relief [expr {$id eq $new_active ? "sunken" : "flat"}]
        if {$id eq $active_id || $id eq $new_active} {
            $button configure -relief $relief
        }
    }

    foreach id [dict keys $buttons] {
        if {![dict exists $present $id]} {
            set button [dict get $buttons $id]
            if {[winfo exists $button]} { destroy $button }
            dict unset buttons $id
            if {[dict exists $titles $id]} { dict unset titles $id }
        }
    }

    # Repack existing widgets only when EWMH ordering changes.
    if {$order ne $ordered_ids} {
        foreach id $order {
            set button [dict get $buttons $id]
            pack forget $button
            pack $button -side left -fill y
        }
        set ordered_ids $order
    }
    set active_id $new_active
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
