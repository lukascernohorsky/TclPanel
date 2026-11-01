namespace eval ::tclpanel::xdg {}

proc ::tclpanel::xdg::data_dirs {} {
    set dirs {}
    if {[info exists ::env(XDG_DATA_HOME)] && $::env(XDG_DATA_HOME) ne ""} {
        lappend dirs $::env(XDG_DATA_HOME)
    } else {
        lappend dirs [file join [expr {[info exists ::env(HOME)] ? $::env(HOME) : "~"}] .local share]
    }
    if {[info exists ::env(XDG_DATA_DIRS)] && $::env(XDG_DATA_DIRS) ne ""} {
        foreach dir [split $::env(XDG_DATA_DIRS) :] { if {$dir ne ""} { lappend dirs $dir } }
    } else {
        lappend dirs /usr/local/share /usr/share
    }
    return [lsort -unique $dirs]
}

proc ::tclpanel::xdg::decode_value {value} {
    return [string map [list {\s} { } {\n} "\n" {\t} "\t" {\r} "\r" {\\} {\}] $value]
}

proc ::tclpanel::xdg::visible_for_desktop {entry} {
    set current {}
    if {[info exists ::env(XDG_CURRENT_DESKTOP)]} { set current [split [string toupper $::env(XDG_CURRENT_DESKTOP)] :] }
    set only [split [string trim [dict get $entry OnlyShowIn] ";"] ";"]
    set not [split [string trim [dict get $entry NotShowIn] ";"] ";"]
    if {$only eq {}} { set only {} }
    if {$not eq {}} { set not {} }
    if {[llength $only]} {
        set matched 0
        foreach desktop $only { if {[string toupper $desktop] in $current} { set matched 1 } }
        if {!$matched} { return 0 }
    }
    foreach desktop $not { if {[string toupper $desktop] in $current} { return 0 } }
    return 1
}

proc ::tclpanel::xdg::parse_file {path} {
    if {![file isfile $path] || ![file readable $path]} { return {} }
    set ch [open $path r]
    try { fconfigure $ch -encoding utf-8; set data [read $ch] } finally { close $ch }
    set in_entry 0
    set values [dict create]
    foreach line [split $data "\n"] {
        set line [string trim $line "\r"]
        if {[regexp {^\[([^]]+)\]$} $line -> section]} { set in_entry [expr {$section eq "Desktop Entry"}]; continue }
        if {!$in_entry || $line eq "" || [regexp {^[ \t]*[#;]} $line]} { continue }
        if {![regexp {^([^=]+)=(.*)$} $line -> key value]} { continue }
        set key [string trim $key]
        if {[regexp {^(Name|GenericName|Comment)(\[([^]]+)\])?$} $key -> base locale]} {
            if {$locale ne ""} { dict set values [format "%s@%s" $base $locale] [::tclpanel::xdg::decode_value $value] } else { dict set values $base [::tclpanel::xdg::decode_value $value] }
        } else { dict set values $key [::tclpanel::xdg::decode_value $value] }
    }
    if {![dict exists $values Type] || [dict get $values Type] ne "Application"} { return {} }
    foreach key {Name Exec Icon Categories TryExec Terminal Hidden NoDisplay OnlyShowIn NotShowIn} {
        if {![dict exists $values $key]} { dict set values $key "" }
    }
    if {[dict get $values Hidden] eq "true" || [dict get $values NoDisplay] eq "true"} { return {} }
    if {![::tclpanel::xdg::visible_for_desktop $values]} { return {} }
    if {[dict get $values Name] eq "" || [dict get $values Exec] eq ""} { return {} }
    set tryexec [dict get $values TryExec]
    if {$tryexec ne "" && [auto_execok $tryexec] eq "" && ![file executable $tryexec]} { return {} }
    dict set values DesktopFile [file normalize $path]
    dict set values Categories [split [string trim [dict get $values Categories] ";"] ";"]
    dict set values Terminal [expr {[dict get $values Terminal] eq "true"}]
    return $values
}

proc ::tclpanel::xdg::discover {{dirs ""}} {
    if {$dirs eq ""} {
        set dirs {}
        foreach base [::tclpanel::xdg::data_dirs] { lappend dirs [file join $base applications] }
    }
    set found {}
    set seen {}
    foreach dir $dirs {
        if {![file isdirectory $dir]} { continue }
        foreach path [lsort [glob -nocomplain -directory $dir -types f *.desktop]] {
            set id [file tail $path]
            if {[dict exists $seen $id]} { continue }
            dict set seen $id 1
            set app [::tclpanel::xdg::parse_file $path]
            if {$app ne ""} { lappend found $app }
        }
    }
    return [lsort -command ::tclpanel::xdg::compare_apps $found]
}
proc ::tclpanel::xdg::compare_apps {a b} { return [string compare -nocase [dict get $a Name] [dict get $b Name]] }

proc ::tclpanel::xdg::tokenize_exec {command} {
    set words {}; set word ""; set started 0; set quote 0; set escaped 0
    foreach ch [split $command ""] {
        if {$escaped} { append word $ch; set escaped 0; set started 1; continue }
        if {$ch eq "\\"} { set escaped 1; set started 1; continue }
        if {$quote} {
            if {$ch eq "\""} { set quote 0 } else { append word $ch }
            set started 1; continue
        }
        if {$ch eq "\""} { set quote 1; set started 1; continue }
        if {[string is space $ch]} {
            if {$started} { lappend words $word; set word ""; set started 0 }
            continue
        }
        append word $ch; set started 1
    }
    if {$escaped || $quote} { error "unterminated escape or quote in Desktop Entry Exec" }
    if {$started} { lappend words $word }
    return $words
}

proc ::tclpanel::xdg::exec_argv {entry} {
    set name [dict get $entry Name]; set icon [dict get $entry Icon]; set desktopfile [dict get $entry DesktopFile]
    set result {}
    set literal_percent [format %c 1]
    foreach token [::tclpanel::xdg::tokenize_exec [dict get $entry Exec]] {
        if {$token in {%f %F %u %U}} { continue }
        if {$token eq "%i"} { if {$icon ne ""} { lappend result --icon $icon }; continue }
        set token [string map [list %% $literal_percent %c $name %k $desktopfile] $token]
        if {[regexp {%[A-Za-z]} $token]} { continue }
        set token [string map [list $literal_percent %] $token]
        lappend result $token
    }
    if {![llength $result]} { error "Desktop Entry Exec expands to an empty command" }
    return $result
}
proc ::tclpanel::xdg::launch {entry} {
    if {[dict get $entry Terminal]} { error "Terminal=true applications require a configured terminal launcher" }
    exec {*}[::tclpanel::xdg::exec_argv $entry] &
}
