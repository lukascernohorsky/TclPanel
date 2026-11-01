namespace eval ::tclpanel::module::clock {}
proc ::tclpanel::module::clock::create {parent} {
    label $parent.clock -text {} -padx 10 -background [::tclpanel::config::get background] -foreground [::tclpanel::config::get foreground]
    pack $parent.clock -side right -fill y
    ::tclpanel::module::clock::tick $parent.clock
}
proc ::tclpanel::module::clock::tick {w} {
    if {![winfo exists $w]} return
    $w configure -text [clock format [clock seconds] -format {%a %d %b  %H:%M:%S}]
    after 1000 [list ::tclpanel::module::clock::tick $w]
}
