namespace eval ::tclpanel::panel { variable bar .tclpanel }
proc ::tclpanel::panel::create {} {
    variable bar
    set h [::tclpanel::config::get height 36]
    set bg [::tclpanel::config::get background #252a34]
    set fg [::tclpanel::config::get foreground #f2f2f2]
    toplevel $bar
    wm title $bar TclPanel
    wm overrideredirect $bar 0
    catch {wm attributes $bar -type dock}
    wm resizable $bar 1 0
    frame $bar.inner -background $bg
    pack $bar.inner -fill both -expand 1
    pack propagate $bar.inner 0
    $bar.inner configure -height $h
    bind $bar <Map> [list after 100 [list ::tclpanel::panel::apply_strut $h]]
    bind $bar <Configure> [list after 100 [list ::tclpanel::panel::apply_strut $h]]
    wm protocol $bar WM_DELETE_WINDOW {exit}
    set sw [winfo screenwidth $bar]
    set sh [winfo screenheight $bar]
    if {[::tclpanel::config::get position bottom] eq "top"} {
        wm geometry $bar ${sw}x${h}+0+0
    } else {
        wm geometry $bar ${sw}x${h}+0+[expr {$sh-$h}]
    }
    wm deiconify $bar
}
proc ::tclpanel::panel::content {} {
    variable bar
    return $bar.inner
}
proc ::tclpanel::panel::apply_strut {height} {
    variable bar
    if {[llength [info commands ::x11::strut]]} {
        set edge [::tclpanel::config::get position bottom]
        catch {::x11::strut [winfo id $bar] $edge $height}
    }
}
