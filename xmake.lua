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

-- 定义ASCII艺术标志
local catime_logo = [[

██████╗  █████╗ ████████╗██╗███╗   ███╗███████╗
██╔════╝ ██╔══██╗╚══██╔══╝██║████╗ ████║██╔════╝
██║      ███████║   ██║   ██║██╔████╔██║█████╗  
██║      ██╔══██║   ██║   ██║██║╚██╔╝██║██╔══╝  
╚██████╗ ██║  ██║   ██║   ██║██║ ╚═╝ ██║███████╗
 ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝╚═╝     ╚═╝╚══════╝

]]

-- 定义目标
target("catime")
    -- 设置为可执行程序
    set_kind("binary")
    
    -- 在构建开始前显示标志
    before_build(function (target)
        -- 显示ASCII艺术标志
        print("")
        print("\x1b[36m" .. "██████╗  █████╗ ████████╗██╗███╗   ███╗███████╗" .. "\x1b[0m")
        print("\x1b[36m" .. "██╔════╝ ██╔══██╗╚══██╔══╝██║████╗ ████║██╔════╝" .. "\x1b[0m")
        print("\x1b[36m" .. "██║      ███████║   ██║   ██║██╔████╔██║█████╗  " .. "\x1b[0m")
        print("\x1b[36m" .. "██║      ██╔══██║   ██║   ██║██║╚██╔╝██║██╔══╝  " .. "\x1b[0m")
        print("\x1b[36m" .. "╚██████╗ ██║  ██║   ██║   ██║██║ ╚═╝ ██║███████╗" .. "\x1b[0m")
        print("\x1b[36m" .. " ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝╚═╝     ╚═╝╚══════╝" .. "\x1b[0m")
        print("")
        
        -- 显示初始进度条
        -- local bar_width = 40
        -- local filled_char = "█"
        -- local empty_char = "░"
        -- local empty_bar = string.rep(empty_char, bar_width)
        
        -- print("\x1b[34m" .. "开始构建..." .. "\x1b[0m")
        -- io.write(string.format("\r\x1b[90mProgress: [\x1b[0m\x1b[90m%s\x1b[0m\x1b[90m] %3d%% Complete\x1b[0m", empty_bar, 0))
        -- io.flush()
    end)
    
    -- 在每个文件编译后更新进度
    after_build_file(function (target, sourcefile)
        -- 注意：这个回调可能在某些xmake版本中不可用
        -- 如果不工作，用户仍然会看到标准的xmake进度输出
    end)
    
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
    set_targetdir("$(buildir)")
    set_objectdir("build")
    
    -- 添加miniaudio实现定义
    add_defines("MINIAUDIO_IMPLEMENTATION")
    
-- 配置自定义构建事件
after_build(function (target)
    -- 显示完成进度条
    local bar_width = 40
    local filled_char = "█"
    local empty_char = "░"
    local full_bar = string.rep(filled_char, bar_width)
    
    -- io.write(string.format("\r\x1b[90mProgress: [\x1b[0m\x1b[90m%s\x1b[0m\x1b[90m] %3d%% Complete\x1b[0m\n", full_bar, 100))
    -- io.flush()
    
    -- 压缩可执行文件
    local targetfile = target:targetfile()
    local size_before = os.filesize(targetfile)
    local size_before_kb = math.floor(size_before / 1024)
    
    -- 隐藏所有UPX输出，使用脚本
    local is_windows = os.host() == "windows"
    local script_ext = is_windows and ".bat" or ".sh"
    local script_file = os.tmpfile() .. script_ext
    
    -- 创建临时脚本
    local script = io.open(script_file, "w")
    if script then
        if is_windows then
            -- Windows bat脚本
            script:write("@echo off\n")
            script:write("upx --best --lzma " .. targetfile .. " > nul 2>&1\n")
        else
            -- Unix shell脚本
            script:write("#!/bin/sh\n")
            script:write("upx --best --lzma " .. targetfile .. " > /dev/null 2>&1\n")
        end
        script:close()
        
        -- 确保脚本可执行(仅Unix)
        if not is_windows then
            os.exec("chmod +x %s", script_file)
        end
        
        -- 运行脚本并只显示最终结果
        os.exec(script_file)
        
        -- 清理临时文件
        os.rm(script_file)
    else
        -- 如果无法创建脚本，则直接显示结果
        -- 但不执行UPX，因为无法隐藏输出
        try {
            function()
                os.exec("upx --best --lzma %s", targetfile)
            end,
            catch = function() end
        }
    end
    
    -- 显示压缩结果，格式类似Makefile
    local size_after = os.filesize(targetfile)
    local size_before_kb = math.floor(size_before / 1024)
    local size_after_kb = math.floor(size_after / 1024)
    
    -- 改变显示格式，使用与Makefile类似的格式但不尝试显示百分比符号
    print("\x1b[38;2;0;255;0m[" .. " 99%]:\x1b[0m Compressed: " .. size_before_kb .. "KiB → " .. size_after_kb .. "KiB")
    print("\x1b[38;2;0;255;0m[" .. " 99%]:\x1b[0m " .. "Output directory: " .. target:targetdir())
end)

-- 自定义菜单
option("debug")
    set_default(false)
    set_showmenu(true)
    set_category("选项")
    set_description("启用调试模式")
