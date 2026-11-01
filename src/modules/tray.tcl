namespace eval ::tclpanel::module::tray {
    variable frame {}
    variable started 0
    variable iconSize
    variable gap
}
proc ::tclpanel::module::tray::create {parent} {
    variable frame
    variable iconSize [::tclpanel::config::get tray_icon_size 0]
    variable gap [::tclpanel::config::get tray_icon_gap 2]
    frame $parent.tray -background [::tclpanel::config::get background] \
        -width 8 -height [::tclpanel::config::get height 36]
    pack $parent.tray -side right -fill y
    set frame $parent.tray
    bind $frame <Configure> ::tclpanel::module::tray::configure
    bind $frame <Destroy> [list ::tclpanel::module::tray::destroyed %W]
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
    ::tclpanel::module::tray::apply_layout
}
proc ::tclpanel::module::tray::configure {} {
    variable frame
    variable started
    if {$started && [winfo exists $frame]} {
        ::tclpanel::module::tray::apply_layout
    }
}
proc ::tclpanel::module::tray::apply_layout {} {
    variable frame
    variable iconSize
    variable gap
    if {![winfo exists $frame]} return
    set height [winfo height $frame]
    if {$height < 1} { set height [::tclpanel::config::get height 36] }
    set size $iconSize
    if {$size <= 0 || $size > $height} { set size $height }
    set slots 0
    if {[llength [info commands ::x11::tray_count]]} {
        set slots [::x11::tray_count]
    }
    set width [expr {max(8, $slots * $size + max(0, $slots - 1) * $gap)}]
    $frame configure -width $width
    if {$::tclpanel::module::tray::started} {
        catch {::x11::tray_configure $width $height $size $gap}
    }
}
proc ::tclpanel::module::tray::changed {} {
    variable frame
    variable started
    if {![::x11::tray_active]} {
        set started 0
        puts stderr "TclPanel: system tray selection was taken by another manager"
        return
    }
    if {![winfo exists $frame]} return
    ::tclpanel::module::tray::apply_layout
}
proc ::tclpanel::module::tray::destroyed {w} {
    variable started
    if {$w eq "."} return
    if {$started && [llength [info commands ::x11::tray_stop]]} {
        catch {::x11::tray_stop}
    }
    set started 0
}
