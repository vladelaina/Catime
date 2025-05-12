-- 定义项目信息
set_project("Catime")
set_version("1.0.0")

-- 设置xmake最低版本要求
set_xmakever("2.5.0")

-- 设置默认编译模式
set_defaultmode("release")

-- 允许使用平台API
add_rules("mode.debug", "mode.release")

-- 强制设定平台为Windows
set_plat("mingw")

-- 设置默认架构
set_arch("x86_64")

-- 设置MinGW工具链
set_toolchains("mingw")

-- 定义目标
target("catime")
    -- 设置为可执行程序
    set_kind("binary")
    
    -- 添加Windows特有设置
    add_defines("_WINDOWS")
    add_ldflags("-mwindows", {force = true})
    add_links("ole32", "shell32", "comdlg32", "uuid", "wininet", "winmm", "comctl32", "dwmapi", "user32", "gdi32")
    add_files("resource/resource.rc")
    
    -- 添加源文件
    add_files("src/*.c")
    
    -- 添加头文件搜索路径
    add_includedirs("include", "libs/miniaudio")
    
    -- 添加编译选项
    if is_mode("release") then
        add_cflags("-O3", "-flto=8", "-mtune=generic", "-ffunction-sections", "-fdata-sections", "-fno-strict-aliasing")
        add_ldflags("-Wl,--gc-sections", "-flto=8", "-s")
        add_defines("NDEBUG")
    end
    
    -- 设置输出目录
    set_targetdir("bin")
    set_objectdir("build")
    
    -- 添加miniaudio实现定义
    add_defines("MINIAUDIO_IMPLEMENTATION")
    
-- 配置自定义构建事件
after_build(function (target)
    -- 压缩可执行文件
    import("core.project.task")
    import("core.base.option")
    import("utils.progress")
    
    print("正在压缩可执行文件...")
    local targetfile = target:targetfile()
    local size_before = os.filesize(targetfile)
    
    -- 使用UPX压缩，需要确保UPX已安装
    os.exec("upx --best --lzma %s", targetfile)
    
    local size_after = os.filesize(targetfile)
    local size_before_kb = math.floor(size_before / 1024)
    local size_after_kb = math.floor(size_after / 1024)
    local ratio = math.floor((size_after * 100) / size_before)
    
    print(string.format("压缩完成: %dKiB -> %dKiB (%d%%)", size_before_kb, size_after_kb, ratio))
end)

-- 自定义菜单
option("debug")
    set_default(false)
    set_showmenu(true)
    set_category("选项")
    set_description("启用调试模式")
