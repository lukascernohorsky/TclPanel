set root [file normalize [file join [file dirname [info script]] ..]]
source [file join $root src xdg_apps.tcl]
set ::test_count 0
proc assert_equal {actual expected message} {
    incr ::test_count
    if {$actual ne $expected} { error "$message: expected <$expected>, got <$actual>" }
}
proc assert_true {condition message} {
    incr ::test_count
    if {![uplevel 1 [list expr $condition]]} { error $message }
}
proc write_file {path content} {
    file mkdir [file dirname $path]
    set ch [open $path w]
    try { fconfigure $ch -encoding utf-8; puts -nonewline $ch $content } finally { close $ch }
}
set tmp [file join [pwd] [format ".xdg-test-%d" [pid]]]
file delete -force $tmp
try {
    set apps [file join $tmp applications]
    write_file [file join $apps editor.desktop] {
[Desktop Entry]
Type=Application
Name=Text Editor
Exec=/usr/bin/env "APP_TITLE=%c" editor %%f
Icon=accessories-text-editor
Categories=Utility;TextEditor;
}
    write_file [file join $apps hidden.desktop] {
[Desktop Entry]
Type=Application
Name=Hidden App
Exec=hidden-app
Hidden=true
}
    write_file [file join $apps nodisplay.desktop] {
[Desktop Entry]
Type=Application
Name=No Display
Exec=nodisplay
NoDisplay=true
}
    write_file [file join $apps link.desktop] {
[Desktop Entry]
Type=Link
Name=Not an application
URL=https://example.invalid
}
    write_file [file join $apps broken.desktop] {
[Desktop Entry]
Type=Application
Name=Broken
Exec=broken
TryExec=/definitely/not/a/real/program
}
    write_file [file join $apps quoted.desktop] {
[Desktop Entry]
Type=Application
Name=Quoted App
Exec=/usr/bin/printf "hello world" %%c
}
    write_file [file join $apps badquote.desktop] {
[Desktop Entry]
Type=Application
Name=Bad Quote
Exec=printf "unterminated
}
    set parsed [::tclpanel::xdg::parse_file [file join $apps editor.desktop]]
    assert_equal [dict get $parsed Name] "Text Editor" "Name parsing"
    assert_equal [dict get $parsed Icon] "accessories-text-editor" "Icon parsing"
    assert_equal [dict get $parsed Categories] {Utility TextEditor} "Category parsing"
    assert_equal [dict get $parsed Terminal] 0 "Terminal default"
    assert_equal [::tclpanel::xdg::parse_file [file join $apps hidden.desktop]] "" "Hidden filtering"
    assert_equal [::tclpanel::xdg::parse_file [file join $apps nodisplay.desktop]] "" "NoDisplay filtering"
    assert_equal [::tclpanel::xdg::parse_file [file join $apps link.desktop]] "" "Type filtering"
    assert_equal [::tclpanel::xdg::parse_file [file join $apps broken.desktop]] "" "TryExec filtering"
    set list [::tclpanel::xdg::discover [list $apps]]
    assert_equal [llength $list] 2 "Discovery count"
    assert_equal [dict get [lindex $list 0] Name] "Quoted App" "Discovery sorting"
    set argv [::tclpanel::xdg::exec_argv $parsed]
    assert_equal [lindex $argv 0] /usr/bin/env "Exec executable"
    assert_equal [lindex $argv 1] {APP_TITLE=Text Editor} "Quoted argument preservation"
    assert_equal [lindex $argv 2] editor "Exec field code"
    assert_equal [lindex $argv 3] %f "Literal percent"
    set q [::tclpanel::xdg::parse_file [file join $apps quoted.desktop]]
    assert_equal [::tclpanel::xdg::tokenize_exec [dict get $q Exec]] {/usr/bin/printf {hello world} %%c} "Exec tokenization"
    assert_true {[catch {::tclpanel::xdg::tokenize_exec [dict get [::tclpanel::xdg::parse_file [file join $apps badquote.desktop]] Exec]}]} "Malformed Exec must fail"
    puts "XDG tests passed ($::test_count assertions)"
} finally { file delete -force $tmp }
