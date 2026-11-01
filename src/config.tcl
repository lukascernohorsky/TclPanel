namespace eval ::tclpanel::config {
    variable values [dict create \
        position bottom height 36 background #252a34 foreground #f2f2f2 \
        modules {launcher taskbar workspace clock}]
    variable path [file join $::env(HOME) .config tclpanel config.tcl]
}
proc ::tclpanel::config::load {} {
    variable values
    variable path
    if {[file exists $path]} {
        if {[catch {uplevel #0 [list source $path]} err]} {
            puts stderr "TclPanel: cannot read $path: $err"
        }
    } else {
        file mkdir [file dirname $path]
        set f [open $path w]
        puts $f {# TclPanel user configuration}
        puts $f {# set ::tclpanel::config::values [dict replace $::tclpanel::config::values height 40]}
        close $f
    }
}
proc ::tclpanel::config::get {key {default {}}} {
    variable values
    if {[dict exists $values $key]} { return [dict get $values $key] }
    return $default
}
