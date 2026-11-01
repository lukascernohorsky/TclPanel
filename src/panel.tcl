namespace eval ::tclpanel::panel { variable bar .tclpanel; variable monitor [dict create x 0 y 0 width 0 height 0 name screen] }

proc ::tclpanel::panel::select_monitor {monitors selection} {
    if {![llength $monitors]} {
        return [dict create name screen x 0 y 0 width [winfo screenwidth .] height [winfo screenheight .] primary 1]
    }
    if {[string is integer -strict $selection] && $selection >= 0 && $selection < [llength $monitors]} {
        return [lindex $monitors $selection]
    }
    if {![string is integer -strict $selection]} {
        foreach item $monitors {
            if {[dict get $item name] eq $selection} { return $item }
        }
    }
    foreach item $monitors {
        if {[dict get $item primary]} { return $item }
    }
    return [lindex $monitors 0]
}
proc ::tclpanel::panel::geometry_for_monitor {monitor height edge} {
    set x [dict get $monitor x]
    set y [dict get $monitor y]
    set width [dict get $monitor width]
    set monitor_height [dict get $monitor height]
    if {$edge eq "top"} { set py $y } else { set py [expr {$y + $monitor_height - $height}] }
    return [format "%dx%d%+d%+d" $width $height $x $py]
}
proc ::tclpanel::panel::create {} {
    variable bar
    set h [::tclpanel::config::get height 36]
    set bg [::tclpanel::config::get background #252a34]
    toplevel $bar
    wm title $bar TclPanel
    wm overrideredirect $bar 0
    catch {wm attributes $bar -type dock}
    wm resizable $bar 1 0
    frame $bar.inner -background $bg
    pack $bar.inner -fill both -expand 1
    pack propagate $bar.inner 0
    $bar.inner configure -height $h
    bind $bar <Map> [list after idle ::tclpanel::panel::apply_strut $h]
    bind $bar <Configure> [list after idle ::tclpanel::panel::apply_strut $h]
    wm protocol $bar WM_DELETE_WINDOW {exit}
    refresh_geometry
    wm deiconify $bar
    if {[llength [info commands ::x11::watch_monitors]]} {
        catch {::x11::watch_monitors ::tclpanel::panel::monitor_changed}
    }
}
proc ::tclpanel::panel::content {} {
    variable bar
    return $bar.inner
}
proc ::tclpanel::panel::refresh_geometry {} {
    variable bar
    variable monitor
    set h [::tclpanel::config::get height 36]
    set edge [::tclpanel::config::get position bottom]
    if {$edge ni {top bottom}} { set edge bottom }
    set list {}
    if {[llength [info commands ::x11::monitors]]} { catch {set list [::x11::monitors]} }
    set monitor [select_monitor $list [::tclpanel::config::get monitor 0]]
    wm geometry $bar [geometry_for_monitor $monitor $h $edge]
    apply_strut $h
}
proc ::tclpanel::panel::monitor_changed {} {
    after idle ::tclpanel::panel::refresh_geometry
}
proc ::tclpanel::panel::apply_strut {height} {
    variable bar
    variable monitor
    if {![winfo exists $bar] || ![llength [info commands ::x11::strut]]} { return }
    set edge [::tclpanel::config::get position bottom]
    set root_height [winfo screenheight $bar]
    set y [dict get $monitor y]
    set mh [dict get $monitor height]
    # EWMH struts only reserve regions attached to a root-screen edge.
    if {($edge eq "top" && $y != 0) || ($edge eq "bottom" && ($y + $mh) != $root_height)} {
        return
    }
    set x [dict get $monitor x]
    set width [dict get $monitor width]
    catch {::x11::strut [winfo id $bar] $edge $height $x [expr {$x + $width - 1}]}
}
