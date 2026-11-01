namespace eval ::tclpanel::module::launcher {}
proc ::tclpanel::module::launcher::create {parent} {
    button $parent.launcher -text "Applications" -command ::tclpanel::module::launcher::menu -padx 10
    pack $parent.launcher -side left -fill y
}
proc ::tclpanel::module::launcher::menu {} {
    set m .tclpanel_apps
    catch {destroy $m}
    menu $m -tearoff 0
    set apps {}
    if {[llength [info commands ::tclpanel::xdg::discover]]} { catch {set apps [::tclpanel::xdg::discover]} }
    if {![llength $apps]} {
        foreach app {xterm xclock xcalc firefox} {
            if {[auto_execok $app] ne ""} { $m add command -label $app -command [list ::tclpanel::module::launcher::run_command $app] }
        }
    } else {
        foreach entry $apps {
            $m add command -label [dict get $entry Name] -command [list ::tclpanel::module::launcher::run_entry $entry]
        }
    }
    tk_popup $m [winfo pointerx .] [winfo pointery .]
}
proc ::tclpanel::module::launcher::run_entry {entry} {
    if {[catch {::tclpanel::xdg::launch $entry} message]} {
        if {[llength [info commands tk_messageBox]]} { tk_messageBox -icon error -title "TclPanel" -message $message }
    }
}
proc ::tclpanel::module::launcher::run_command {app} { if {[auto_execok $app] ne ""} { exec $app & } }
