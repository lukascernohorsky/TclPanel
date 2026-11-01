namespace eval ::tclpanel::module::launcher {}
proc ::tclpanel::module::launcher::create {parent} {
    button $parent.launcher -text "Applications" -command ::tclpanel::module::launcher::menu -padx 10
    pack $parent.launcher -side left -fill y
}
proc ::tclpanel::module::launcher::menu {} {
    set m .tclpanel_apps
    catch {destroy $m}
    menu $m -tearoff 0
    foreach app {xterm xclock xcalc firefox} {
        $m add command -label $app -command [list ::tclpanel::module::launcher::run $app]
    }
    tk_popup $m [winfo pointerx .] [winfo pointery .]
}
proc ::tclpanel::module::launcher::run {app} {
    if {[auto_execok $app] ne ""} { exec $app & }
}
