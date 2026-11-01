namespace eval ::tclpanel::module::workspace {}
proc ::tclpanel::module::workspace::create {parent} {
    label $parent.workspace -text "Desktop" -padx 8 -background [::tclpanel::config::get background] -foreground [::tclpanel::config::get foreground]
    pack $parent.workspace -side left -fill y
    ::tclpanel::module::workspace::refresh $parent.workspace
}
proc ::tclpanel::module::workspace::refresh {w} {
    if {![winfo exists $w]} return
    if {[auto_execok wmctrl] ne "" && ![catch {exec wmctrl -d} data]} {
        set current "?"
        foreach line [split $data \n] {
            if {[regexp {^([0-9]+)\s+\*} $line -> n]} { set current $n }
        }
        $w configure -text "Desktop $current"
    }
    after 5000 [list ::tclpanel::module::workspace::refresh $w]
}
