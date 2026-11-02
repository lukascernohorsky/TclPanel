namespace eval ::tclpanel::module::workspace {
    variable widget {}
    variable event_driven 0
}

proc ::tclpanel::module::workspace::create {parent} {
    variable widget
    variable event_driven
    frame $parent.workspace -background [::tclpanel::config::get background]
    label $parent.workspace.label -text "Desktop" -padx 6 \
        -background [::tclpanel::config::get background] \
        -foreground [::tclpanel::config::get foreground]
    pack $parent.workspace.label -side left -fill y
    button $parent.workspace.prev -text "<" -width 2 -command {::tclpanel::module::workspace::step -1}
    button $parent.workspace.next -text ">" -width 2 -command {::tclpanel::module::workspace::step 1}
    pack $parent.workspace.prev $parent.workspace.next -side left -fill y
    pack $parent.workspace -side left -fill y
    set widget $parent.workspace
    set event_driven 0
    if {[llength [info commands ::x11::watch_desktops]] &&
        ![catch {::x11::watch_desktops ::tclpanel::module::workspace::desktop_changed}]} {
        set event_driven 1
    }
    bind $widget <Destroy> [list ::tclpanel::module::workspace::destroyed %W]
    ::tclpanel::module::workspace::refresh $widget
}
proc ::tclpanel::module::workspace::destroyed {w} {
    variable widget
    variable event_driven
    if {$w ne $widget} { return }
    if {$event_driven && [llength [info commands ::x11::unwatch_desktops]]} {
        catch {::x11::unwatch_desktops}
    }
    set widget {}
    set event_driven 0
}
proc ::tclpanel::module::workspace::desktop_changed {} {
    variable widget
    if {$widget ne {} && [winfo exists $widget]} {
        ::tclpanel::module::workspace::refresh $widget
    }
}
proc ::tclpanel::module::workspace::refresh {w} {
    variable event_driven
    if {![winfo exists $w]} { return }
    set current "?"
    set count 0
    if {[llength [info commands ::x11::current_desktop]]} {
        catch {set current [::x11::current_desktop]}
        catch {set count [::x11::desktops]}
    } elseif {[auto_execok wmctrl] ne "" && ![catch {exec wmctrl -d} data]} {
        foreach line [split $data \n] {
            if {[regexp {^([0-9]+)\s+\*} $line -> n]} { set current $n }
        }
        set count [llength [split [string trim $data] \n]]
    }
    $w.label configure -text "Desktop [expr {$current eq "?" ? "?" : $current + 1}]"
    if {$count > 0 && $current ne "?"} {
        $w.prev configure -state [expr {$current <= 0 ? "disabled" : "normal"}]
        $w.next configure -state [expr {$current >= $count - 1 ? "disabled" : "normal"}]
    } else {
        $w.prev configure -state disabled
        $w.next configure -state disabled
    }
    if {!$event_driven} {
        after 4000 [list ::tclpanel::module::workspace::refresh $w]
    }
}
proc ::tclpanel::module::workspace::step {delta} {
    if {[llength [info commands ::x11::current_desktop]]} {
        if {[catch {set current [::x11::current_desktop]}] ||
            [catch {set count [::x11::desktops]}] || $count < 1} { return }
        set target [expr {$current + $delta}]
        if {$target >= 0 && $target < $count} { catch {::x11::set_desktop $target} }
        return
    }
    if {[auto_execok wmctrl] ne "" && ![catch {exec wmctrl -d} data]} {
        set current -1
        set count 0
        foreach line [split $data \n] {
            if {[regexp {^([0-9]+)\s+\*} $line -> n]} { set current $n }
            if {[regexp {^([0-9]+)\s+} $line]} { incr count }
        }
        set target [expr {$current + $delta}]
        if {$current >= 0 && $target >= 0 && $target < $count} { catch {exec wmctrl -s $target} }
    }
}
