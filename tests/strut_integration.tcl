if {$argc != 2} {
    error "usage: strut_integration.tcl path-to-lib path-to-property-probe"
}
package require Tk
load [file normalize [lindex $argv 0]] Tclpanelx11

toplevel .strutprobe
wm geometry .strutprobe 200x40+0+0
update
set window_id [winfo id .strutprobe]
set probe [file normalize [lindex $argv 1]]

# The short form reserves the complete root-screen width.
::x11::strut $window_id top 24
exec $probe $window_id top 24 0 1279

# Partial struts preserve the selected horizontal interval.
::x11::strut $window_id bottom 36 128 1023
exec $probe $window_id bottom 36 128 1023

# Zero thickness clears the reservation while retaining dock hints.
::x11::strut $window_id bottom 0 0 1279
exec $probe $window_id bottom 0 0 1279

destroy .strutprobe
puts "EWMH strut integration tests passed"
