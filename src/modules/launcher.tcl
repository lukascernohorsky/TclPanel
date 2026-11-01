namespace eval ::tclpanel::module::launcher {}

proc ::tclpanel::module::launcher::create {parent} {
    button $parent.launcher -text "Applications" -command ::tclpanel::module::launcher::menu -padx 10
    pack $parent.launcher -side left -fill y
}

proc ::tclpanel::module::launcher::category_name {category} {
    set names [dict create AudioVideo "Audio & Video" Development Development Education Education \
        Game Games Graphics Graphics Network Internet Office Office Science Science \
        Settings Settings System System Utility Utilities]
    if {[dict exists $names $category]} { return [dict get $names $category] }
    return Other
}

proc ::tclpanel::module::launcher::menu {} {
    set m .tclpanel_apps
    catch {destroy $m}
    menu $m -tearoff 0
    set apps {}
    if {[llength [info commands ::tclpanel::xdg::discover]]} {
        if {[catch {set apps [::tclpanel::xdg::discover]} message]} {
            puts stderr "TclPanel: cannot read application menu: $message"
            set apps {}
        }
    }
    if {![llength $apps]} {
        foreach app {xterm xclock xcalc firefox} {
            if {[auto_execok $app] ne ""} {
                $m add command -label $app -command [list ::tclpanel::module::launcher::run_command $app]
            }
        }
    } else {
        set menus {}
        foreach entry $apps {
            set category Other
            foreach candidate [dict get $entry Categories] {
                if {$candidate in {AudioVideo Development Education Game Graphics Network Office Science Settings System Utility}} {
                    set category [::tclpanel::module::launcher::category_name $candidate]
                    break
                }
            }
            if {![dict exists $menus $category]} {
                set sub $m.category[incr ::tclpanel::module::launcher::menu_serial]
                menu $sub -tearoff 0
                $m add cascade -label $category -menu $sub
                dict set menus $category $sub
            }
            set sub [dict get $menus $category]
            $sub add command -label [dict get $entry Name] \
                -command [list ::tclpanel::module::launcher::run_entry $entry]
        }
    }
    tk_popup $m [winfo pointerx .] [winfo pointery .]
}
namespace eval ::tclpanel::module::launcher { variable menu_serial 0 }

proc ::tclpanel::module::launcher::run_entry {entry} {
    if {[catch {::tclpanel::xdg::launch $entry} message]} {
        if {[llength [info commands tk_messageBox]]} {
            tk_messageBox -icon error -title "TclPanel" -message $message
        } else {
            puts stderr "TclPanel: cannot launch [dict get $entry Name]: $message"
        }
    }
}
proc ::tclpanel::module::launcher::run_command {app} {
    if {[auto_execok $app] ne ""} { exec $app & }
}
