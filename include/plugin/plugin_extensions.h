/**
 * @file plugin_extensions.h
 * @brief Supported plugin script extensions
 */

#ifndef PLUGIN_EXTENSIONS_H
#define PLUGIN_EXTENSIONS_H

/**
 * Supported script file extensions (source-visible, no compiled executables)
 * Sorted alphabetically by language name
 */
static const char* PLUGIN_EXTENSIONS[] = {
    "*.agda",                   // Agda
    "*.apl",                    // APL
    "*.applescript",            // AppleScript
    "*.ahk", "*.ahk2",          // AutoHotkey
    "*.au3",                    // AutoIt
    "*.awk",                    // AWK
    "*.bal",                    // Ballerina
    "*.bat", "*.cmd",           // Batch
    "*.boo",                    // Boo
    "*.ck",                     // ChucK
    "*.clj", "*.cljs", "*.cljc",// Clojure
    "*.coffee",                 // CoffeeScript
    "*.lisp", "*.lsp", "*.cl",  // Common Lisp
    "*.cr",                     // Crystal
    "*.d",                      // D
    "*.dart",                   // Dart
    "*.ex", "*.exs",            // Elixir
    "*.elm",                    // Elm
    "*.el",                     // Emacs Lisp
    "*.erl", "*.escript",       // Erlang
    "*.fs", "*.fsx",            // F#
    "*.factor",                 // Factor
    "*.fnl",                    // Fennel
    "*.forth", "*.4th", "*.fth",// Forth
    "*.go",                     // Go
    "*.groovy", "*.gvy",        // Groovy
    "*.hack", "*.hh",           // Hack
    "*.hs", "*.lhs",            // Haskell
    "*.hx",                     // Haxe
    "*.hy",                     // Hy
    "*.idr",                    // Idris
    "*.io",                     // Io
    "*.ijs",                    // J
    "*.janet",                  // Janet
    "*.js", "*.mjs", "*.cjs",   // JavaScript
    "*.jl",                     // Julia
    "*.k", "*.q",               // K/Q
    "*.kts",                    // Kotlin Script
    "*.lean",                   // Lean
    "*.lua",                    // Lua
    "*.m", "*.wl",              // MATLAB/Mathematica
    "*.moon",                   // MoonScript
    "*.nims", "*.nimble",       // Nim
    "*.ml", "*.mli",            // OCaml
    "*.pl", "*.pm", "*.perl",   // Perl
    "*.p6", "*.pl6", "*.raku",  // Perl 6/Raku
    "*.php", "*.php5", "*.php7",// PHP
    "*.pike",                   // Pike
    "*.pony",                   // Pony
    "*.ps1",                    // PowerShell
    "*.pde",                    // Processing
    "*.pro",                    // Prolog
    "*.purs",                   // PureScript
    "*.py", "*.pyw",            // Python
    "*.r", "*.R", "*.Rscript",  // R
    "*.rkt", "*.scm", "*.ss",   // Racket/Scheme
    "*.red", "*.reds",          // Red
    "*.rexx", "*.rex",          // Rexx
    "*.rb", "*.rbw",            // Ruby
    "*.scala", "*.sc",          // Scala
    "*.sed",                    // sed
    "*.sh", "*.bash", "*.zsh",  // Shell
    "*.ksh", "*.csh", "*.fish", // Shell (more)
    "*.st",                     // Smalltalk
    "*.sml",                    // Standard ML
    "*.swift",                  // Swift
    "*.tcl", "*.tk",            // Tcl/Tk
    "*.ts", "*.mts",            // TypeScript
    "*.v", "*.vsh",             // V
    "*.vbs", "*.wsf",           // VBScript/WSH
    "*.wren",                   // Wren
    "*.zig"                     // Zig
};

#define PLUGIN_EXTENSION_COUNT (sizeof(PLUGIN_EXTENSIONS) / sizeof(PLUGIN_EXTENSIONS[0]))

#endif /* PLUGIN_EXTENSIONS_H */
