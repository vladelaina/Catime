set_project("catime")
set_version("1.0.7")
set_encodings("utf-8")
set_toolchains("mingw")
set_installdir("$(builddir)")

set_policy("build.optimization.lto", true) -- 启用LTO
add_cflags("-O3", "-mwindows", "-ffunction-sections", "-fdata-sections", "-fno-strict-aliasing")
add_ldflags("-Wl,--gc-sections", "-s")

target("miniaudio")
    set_default(false)
    set_kind("static")
    add_includedirs("libs/miniaudio")
    add_files("libs/miniaudio/miniaudio.c")

target("catime")
    set_kind("binary")
    set_rundir(".")
    set_prefixdir(".", {bindir = "."})
    add_files("src/*.c", "resource/*.rc")
    add_includedirs("include")

    add_deps("miniaudio")
    add_links(
        "gdi32", 
        "user32", 
        "ole32",
        "shell32",
        "comdlg32",
        "uuid",
        "wininet",
        "winmm",
        "comctl32" 
    )

    on_load(function (target) -- 文件名后添加版本号
        target:set("suffixname", "_" .. import("core.project.project").version())
    end)