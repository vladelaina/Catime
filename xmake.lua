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
    
    -- 禁用某些警告选项
    add_cxflags("-Wno-unknown-warning-option", {force = false})  -- 禁用未知警告的警告
    
    -- 添加编译选项
    if is_mode("release") then
        -- 基本优化选项
        add_cflags("-O3", "-mtune=generic", "-ffunction-sections", "-fdata-sections", "-fno-strict-aliasing")
        
        -- 添加LTO(链接时优化)支持
        add_cflags("-flto")
        add_ldflags("-flto")
        
        -- 更多的优化标志
        add_cflags("-fno-exceptions", "-fomit-frame-pointer", "-fmerge-all-constants")
        add_cflags("-fno-math-errno", "-fno-trapping-math", "-ffast-math")
        
        -- 去除不必要的部分
        add_ldflags("-Wl,--gc-sections", "-s", "-Wl,--strip-all")
        
        -- 使用更小的运行时库
        add_cflags("-Os", {force = true})  -- 更倾向于体积优化而非速度优化
        
        add_defines("NDEBUG")
    end
    
    -- 设置输出目录
    set_targetdir("$(buildir)")
    set_objectdir("build")
    
    -- 添加miniaudio实现定义
    add_defines("MINIAUDIO_IMPLEMENTATION")
    
-- 配置自定义构建事件
after_build(function (target)
    -- 压缩可执行文件
    local targetfile = target:targetfile()
    local size_before = os.filesize(targetfile)
    
    -- 直接尝试执行upx压缩，使用try-catch捕获可能的错误
    local compression_success = false
    try {
        function()
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
                    -- 使用--ultra-brute进行更激进的压缩
                    script:write("upx --ultra-brute --lzma " .. targetfile .. " > nul 2>&1\n")
                    script:write("exit 0\n")  -- 确保即使失败也返回成功
                else
                    -- Unix shell脚本
                    script:write("#!/bin/sh\n")
                    -- 使用--ultra-brute进行更激进的压缩
                    script:write("upx --ultra-brute --lzma " .. targetfile .. " > /dev/null 2>&1 || true\n")
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
                
                compression_success = true
            end
        end,
        catch = function()
            -- 如果出现错误，不做任何处理
        end
    }
    
    -- 显示压缩结果，格式类似Makefile
    local size_after = os.filesize(targetfile)
    local size_before_kb = math.floor(size_before / 1024)
    local size_after_kb = math.floor(size_after / 1024)
    
    if compression_success and size_before ~= size_after then
        -- 只有成功压缩时才显示结果
        print("\x1b[38;2;0;255;0m[" .. " 99%]:\x1b[0m Compressed: " .. size_before_kb .. "KiB → " .. size_after_kb .. "KiB")
    else
        print("\x1b[38;2;0;255;0m[" .. " 99%]:\x1b[0m Compression skipped (UPX failed or not available)")
    end
    
    print("\x1b[38;2;0;255;0m[" .. " 99%]:\x1b[0m " .. "Output directory: " .. target:targetdir())
end)

-- 自定义菜单
option("debug")
    set_default(false)
    set_showmenu(true)
    set_category("选项")
    set_description("启用调试模式")
