namespace eval ::tclpanel::module::tray {
    variable frame {}
    variable started 0
}
proc ::tclpanel::module::tray::create {parent} {
    variable frame
    frame $parent.tray -background [::tclpanel::config::get background] \
        -width 8 -height [::tclpanel::config::get height 36]
    pack $parent.tray -side right -fill y
    set frame $parent.tray
    bind $frame <Configure> ::tclpanel::module::tray::configure
    bind $frame <Destroy> ::tclpanel::module::tray::destroyed
    after idle ::tclpanel::module::tray::start
}
proc ::tclpanel::module::tray::start {} {
    variable frame
    variable started
    if {![winfo exists $frame] || ![winfo ismapped $frame]} {
        after 100 ::tclpanel::module::tray::start
        return
    }
    if {![llength [info commands ::x11::tray_start]]} {
        puts stderr "TclPanel: system tray requires the tclpanelx11 extension"
        return
    }
    if {[catch {::x11::tray_start [winfo id $frame] [winfo width $frame] \
                    [winfo height $frame] ::tclpanel::module::tray::changed} err]} {
        puts stderr "TclPanel: cannot start system tray: $err"
        return
    }
    set started 1
}
proc ::tclpanel::module::tray::configure {} {
    variable frame
    variable started
    if {$started && [winfo exists $frame]} {
        catch {::x11::tray_configure [winfo width $frame] [winfo height $frame]}
    }
}
proc ::tclpanel::module::tray::changed {} {
    # Native XEmbed clients render into the C-managed child window.
}
proc ::tclpanel::module::tray::destroyed {w} {
    variable started
    if {$w eq "."} return
    if {$started && [llength [info commands ::x11::tray_stop]]} {
        catch {::x11::tray_stop}
    }
    set started 0
}
