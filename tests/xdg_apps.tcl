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
Name[cs]=Textový editor
Name[cs_CZ]=Textový editor CZ
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
    set saved_locale_env {}
    foreach variable {LANGUAGE LC_ALL LC_MESSAGES LANG} {
        if {[info exists ::env($variable)]} { dict set saved_locale_env $variable $::env($variable) }
        unset -nocomplain ::env($variable)
    }
    set ::env(LANG) cs_CZ.UTF-8
    set localized [::tclpanel::xdg::parse_file [file join $apps editor.desktop]]
    assert_equal [dict get $localized Name] "Textový editor CZ" "localized desktop name"
    foreach variable {LANGUAGE LC_ALL LC_MESSAGES LANG} { unset -nocomplain ::env($variable) }
    dict for {variable value} $saved_locale_env { set ::env($variable) $value }

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
    write_file [file join $apps onlyshow.desktop] {
[Desktop Entry]
Type=Application
Name=Only GNOME
Exec=only-gnome
OnlyShowIn=KDE;
}
    write_file [file join $apps notshow.desktop] {
[Desktop Entry]
Type=Application
Name=Not GNOME
Exec=not-gnome
NotShowIn=GNOME;
}
    set ::env(XDG_CURRENT_DESKTOP) GNOME
    assert_equal [::tclpanel::xdg::parse_file [file join $apps onlyshow.desktop]] "" "OnlyShowIn filtering"
    assert_equal [::tclpanel::xdg::parse_file [file join $apps notshow.desktop]] "" "NotShowIn filtering"
    unset ::env(XDG_CURRENT_DESKTOP)
    set icon_dir [file join $tmp share icons hicolor 48x48 apps]
    file mkdir $icon_dir
    write_file [file join $icon_dir fixture-icon.png] {not-an-image}
    set old_data_home ""
    if {[info exists ::env(XDG_DATA_HOME)]} { set old_data_home $::env(XDG_DATA_HOME) }
    set ::env(XDG_DATA_HOME) [file join $tmp share]
    assert_equal [::tclpanel::xdg::icon_path fixture-icon 24] [file join $icon_dir fixture-icon.png] "icon theme lookup"
    assert_equal [::tclpanel::xdg::icon_path missing-fixture-icon 24] "" "missing icon lookup"
    if {$old_data_home eq ""} { unset -nocomplain ::env(XDG_DATA_HOME) } else { set ::env(XDG_DATA_HOME) $old_data_home }
    set q [::tclpanel::xdg::parse_file [file join $apps quoted.desktop]]
    assert_equal [::tclpanel::xdg::tokenize_exec [dict get $q Exec]] {/usr/bin/printf {hello world} %%c} "Exec tokenization"
    assert_true {[catch {::tclpanel::xdg::tokenize_exec [dict get [::tclpanel::xdg::parse_file [file join $apps badquote.desktop]] Exec]}]} "Malformed Exec must fail"
    puts "XDG tests passed ($::test_count assertions)"
} finally { file delete -force $tmp }
