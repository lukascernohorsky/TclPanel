namespace eval ::tclpanel::config {
    variable values [dict create \
        position bottom monitor primary height 36 background #252a34 foreground #f2f2f2 \
        tray_icon_size 0 tray_icon_gap 2 \
        modules {launcher taskbar workspace tray clock}]
    variable home [expr {[info exists ::env(HOME)] ? $::env(HOME) : [file normalize ~]}]
    variable path [file join $home .config tclpanel config.tcl]
}

proc ::tclpanel::config::load {} {
    variable values
    variable path
    if {[file exists $path]} {
        # The configuration is executable Tcl; load only a file controlled by the user.
        if {[catch {uplevel #0 [list source $path]} err]} {
            puts stderr "TclPanel: cannot read $path: $err"
            return 0
        }
    } else {
        file mkdir [file dirname $path]
        set f [open $path w]
        puts $f {# TclPanel user configuration}
        puts $f {# Example: dict set ::tclpanel::config::values height 40}
        close $f
    }
    return 1
}

proc ::tclpanel::config::get {key {default {}}} {
    variable values
    if {[dict exists $values $key]} {
        return [dict get $values $key]
    }
    return $default
}
