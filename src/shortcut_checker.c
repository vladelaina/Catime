/**
 * @file shortcut_checker.c
 * @brief 检测和创建桌面快捷方式的功能实现
 *
 * 检测程序是否从应用商店或WinGet安装，
 * 并在需要时在桌面创建快捷方式。
 */

#include "../include/shortcut_checker.h"
#include "../include/config.h"
#include "../include/log.h"  // 添加日志头文件
#include <stdio.h>  // 用于 printf 调试输出
#include <shlobj.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <stdbool.h>
#include <string.h>

// 引入需要的COM接口
#include <shobjidl.h>

// 我们不需要手动定义IID_IShellLinkA，它已经在系统头文件中定义

/**
 * @brief 判断字符串是否以指定前缀开始
 * 
 * @param str 要检查的字符串
 * @param prefix 前缀字符串
 * @return bool true表示以指定前缀开始，false表示不是
 */
static bool StartsWith(const char* str, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    size_t str_len = strlen(str);
    
    if (str_len < prefix_len) {
        return false;
    }
    
    return strncmp(str, prefix, prefix_len) == 0;
}

/**
 * @brief 判断字符串是否包含指定子串
 * 
 * @param str 要检查的字符串
 * @param substring 子串
 * @return bool true表示包含指定子串，false表示不包含
 */
static bool Contains(const char* str, const char* substring) {
    return strstr(str, substring) != NULL;
}

/**
 * @brief 检查程序是否从应用商店或WinGet安装
 * 
 * @param exe_path 输出程序路径的缓冲区
 * @param path_size 缓冲区大小
 * @return bool true表示是从应用商店或WinGet安装，false表示不是
 */
static bool IsStoreOrWingetInstall(char* exe_path, size_t path_size) {
    // 获取程序路径
    if (GetModuleFileNameA(NULL, exe_path, path_size) == 0) {
        LOG_ERROR("获取程序路径失败");
        return false;
    }
    
    LOG_DEBUG("检查程序路径: %s", exe_path);
    
    // 检查是否是应用商店安装路径（C:\Program Files\WindowsApps开头）
    if (StartsWith(exe_path, "C:\\Program Files\\WindowsApps")) {
        LOG_DEBUG("检测到应用商店安装路径");
        return true;
    }
    
    // 检查是否是WinGet安装路径
    // 1. 常规包含\AppData\Local\Microsoft\WinGet\Packages的路径
    if (Contains(exe_path, "\\AppData\\Local\\Microsoft\\WinGet\\Packages")) {
        LOG_DEBUG("检测到WinGet安装路径(常规)");
        return true;
    }
    
    // 2. 可能的自定义WinGet安装路径 (如果在C:\Users\username\AppData\Local\Microsoft\*)
    if (Contains(exe_path, "\\AppData\\Local\\Microsoft\\") && Contains(exe_path, "WinGet")) {
        LOG_DEBUG("检测到可能的WinGet安装路径(自定义)");
        return true;
    }
    
    // 强制测试：当路径包含特定字符串时认为是需要创建快捷方式的路径
    // 这个测试路径与用户日志中看到的安装路径相匹配
    if (Contains(exe_path, "\\WinGet\\catime.exe")) {
        LOG_DEBUG("检测到特定WinGet安装路径");
        return true;
    }
    
    LOG_DEBUG("不是商店或WinGet安装路径");
    return false;
}

/**
 * @brief 检查桌面是否已有快捷方式
 * 
 * @param exe_path 程序路径
 * @return bool true表示已有快捷方式，false表示没有
 */
static bool ShortcutExists(const char* exe_path) {
    char desktop_path[MAX_PATH];
    char public_desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char link_target[MAX_PATH];
    HRESULT hr;
    IShellLinkA* psl = NULL;
    IPersistFile* ppf = NULL;
    WIN32_FIND_DATAA find_data;
    bool exists = false;
    
    // 获取用户桌面路径
    hr = SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path);
    if (FAILED(hr)) {
        LOG_ERROR("获取桌面路径失败, hr=0x%08X", (unsigned int)hr);
        return false;
    }
    LOG_DEBUG("用户桌面路径: %s", desktop_path);
    
    // 获取公共桌面路径
    hr = SHGetFolderPathA(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, public_desktop_path);
    if (FAILED(hr)) {
        LOG_WARNING("获取公共桌面路径失败, hr=0x%08X", (unsigned int)hr);
        // 继续检查用户桌面，不返回
    } else {
        LOG_DEBUG("公共桌面路径: %s", public_desktop_path);
    }
    
    // 首先检查用户桌面 - 构建快捷方式完整路径（Catime.lnk）
    snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", desktop_path);
    LOG_DEBUG("检查用户桌面快捷方式: %s", shortcut_path);
    
    // 检查用户桌面快捷方式文件是否存在
    if (GetFileAttributesA(shortcut_path) != INVALID_FILE_ATTRIBUTES) {
        LOG_DEBUG("用户桌面上发现快捷方式文件");
        exists = true;
    }
    
    // 如果用户桌面没有，检查公共桌面
    if (!exists && SUCCEEDED(hr)) {
        snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", public_desktop_path);
        LOG_DEBUG("检查公共桌面快捷方式: %s", shortcut_path);
        
        if (GetFileAttributesA(shortcut_path) != INVALID_FILE_ATTRIBUTES) {
            LOG_DEBUG("公共桌面上发现快捷方式文件");
            exists = true;
        }
    }
    
    // 如果没找到快捷方式文件，直接返回false
    if (!exists) {
        LOG_DEBUG("未找到任何快捷方式文件");
        return false;
    }
    
    // 找到了快捷方式文件，验证其指向的目标
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkA, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("创建IShellLink接口失败, hr=0x%08X", (unsigned int)hr);
        return false;
    }
    
    // 获取IPersistFile接口
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("获取IPersistFile接口失败, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    // 转换为宽字符
    WCHAR wide_path[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, shortcut_path, -1, wide_path, MAX_PATH);
    
    // 加载快捷方式
    hr = ppf->lpVtbl->Load(ppf, wide_path, STGM_READ);
    if (FAILED(hr)) {
        LOG_ERROR("加载快捷方式失败, hr=0x%08X", (unsigned int)hr);
        ppf->lpVtbl->Release(ppf);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    // 获取快捷方式目标路径
    hr = psl->lpVtbl->GetPath(psl, link_target, MAX_PATH, &find_data, 0);
    if (FAILED(hr)) {
        LOG_ERROR("获取快捷方式目标路径失败, hr=0x%08X", (unsigned int)hr);
    } else {
        LOG_DEBUG("快捷方式目标路径: %s", link_target);
        LOG_DEBUG("当前程序路径: %s", exe_path);
        
        // 检查快捷方式指向的是否就是我们的程序
        // 放宽条件 - 只要目标文件名匹配即可
        char* shortcut_target_filename = strrchr(link_target, '\\');
        char* program_filename = strrchr(exe_path, '\\');
        
        if (shortcut_target_filename && program_filename) {
            shortcut_target_filename++; // 跳过反斜杠
            program_filename++; // 跳过反斜杠
            
            LOG_DEBUG("比较文件名: 快捷方式=%s, 程序=%s", shortcut_target_filename, program_filename);
            
            // 检查文件名是否相同，忽略大小写
            exists = (_stricmp(shortcut_target_filename, program_filename) == 0);
            
            // 如果文件名相同或包含程序名，则认为是有效的快捷方式
            if (!exists && strstr(shortcut_target_filename, "catime") != NULL) {
                LOG_DEBUG("目标文件名包含程序名，认为是有效快捷方式");
                exists = true;
            }
        } else {
            // 回退到完整路径比较
            exists = (_stricmp(link_target, exe_path) == 0);
        }
        
        LOG_DEBUG("快捷方式是否指向当前程序: %s", exists ? "是" : "否");
    }
    
    // 释放接口
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return exists;
}

/**
 * @brief 创建桌面快捷方式
 * 
 * @param exe_path 程序路径
 * @return bool true表示创建成功，false表示创建失败
 */
static bool CreateDesktopShortcut(const char* exe_path) {
    char desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char icon_path[MAX_PATH];
    HRESULT hr;
    IShellLinkA* psl = NULL;
    IPersistFile* ppf = NULL;
    bool success = false;
    
    LOG_INFO("开始创建桌面快捷方式，程序路径: %s", exe_path);
    
    // 获取桌面路径
    hr = SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path);
    if (FAILED(hr)) {
        LOG_ERROR("获取桌面路径失败, hr=0x%08X", (unsigned int)hr);
        return false;
    }
    LOG_DEBUG("桌面路径: %s", desktop_path);
    
    // 构建快捷方式完整路径
    snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", desktop_path);
    LOG_DEBUG("快捷方式路径: %s", shortcut_path);
    
    // 使用程序路径作为图标路径
    strcpy(icon_path, exe_path);
    
    // 如果文件已存在，先删除
    if (GetFileAttributesA(shortcut_path) != INVALID_FILE_ATTRIBUTES) {
        LOG_DEBUG("删除已存在的快捷方式");
        if (!DeleteFileA(shortcut_path)) {
            LOG_WARNING("删除已存在的快捷方式失败: %lu", GetLastError());
            // 继续尝试创建
        }
    }
    
    // 创建IShellLink接口
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkA, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("创建IShellLink接口失败, hr=0x%08X", (unsigned int)hr);
        return false;
    }
    
    // 设置目标路径
    hr = psl->lpVtbl->SetPath(psl, exe_path);
    if (FAILED(hr)) {
        LOG_ERROR("设置快捷方式目标路径失败, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    // 设置工作目录（使用可执行文件所在的目录）
    char work_dir[MAX_PATH];
    strcpy(work_dir, exe_path);
    char* last_slash = strrchr(work_dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    LOG_DEBUG("工作目录: %s", work_dir);
    
    hr = psl->lpVtbl->SetWorkingDirectory(psl, work_dir);
    if (FAILED(hr)) {
        LOG_ERROR("设置工作目录失败, hr=0x%08X", (unsigned int)hr);
    }
    
    // 设置图标
    hr = psl->lpVtbl->SetIconLocation(psl, icon_path, 0);
    if (FAILED(hr)) {
        LOG_ERROR("设置图标失败, hr=0x%08X", (unsigned int)hr);
    }
    
    // 设置描述
    hr = psl->lpVtbl->SetDescription(psl, "Catime - 优雅的计时工具");
    if (FAILED(hr)) {
        LOG_ERROR("设置描述失败, hr=0x%08X", (unsigned int)hr);
    }
    
    // 设置窗口样式 (正常窗口)
    psl->lpVtbl->SetShowCmd(psl, SW_SHOWNORMAL);
    
    // 获取IPersistFile接口
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("获取IPersistFile接口失败, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    // 转换为宽字符
    WCHAR wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, shortcut_path, -1, wide_path, MAX_PATH);
    
    // 保存快捷方式
    hr = ppf->lpVtbl->Save(ppf, wide_path, TRUE);
    if (FAILED(hr)) {
        LOG_ERROR("保存快捷方式失败, hr=0x%08X", (unsigned int)hr);
    } else {
        LOG_INFO("桌面快捷方式创建成功: %s", shortcut_path);
        success = true;
    }
    
    // 释放接口
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return success;
}

/**
 * @brief 检查并创建桌面快捷方式
 * 
 * 检查程序是否从Windows应用商店或WinGet安装，
 * 如果是且配置中没有标记为已检查过，则检查桌面
 * 是否有程序的快捷方式，如果没有则创建一个。
 * 
 * @return int 0表示无需创建或创建成功，1表示创建失败
 */
int CheckAndCreateShortcut(void) {
    char exe_path[MAX_PATH];
    char config_path[MAX_PATH];
    bool shortcut_check_done = false;
    
    // 用于调试的消息
    LOG_DEBUG("开始检查快捷方式");
    
    // 读取配置文件中的标记，判断是否已检查过
    GetConfigPath(config_path, MAX_PATH);
    shortcut_check_done = IsShortcutCheckDone();
    
    LOG_DEBUG("配置路径: %s, 是否已检查: %d", config_path, shortcut_check_done);
    
    // 如果已经检查过，无需再检查
    if (shortcut_check_done) {
        LOG_DEBUG("已检查过快捷方式，无需再检查");
        return 0;
    }
    
    // 检查是否是应用商店或WinGet安装
    BOOL isStoreInstall = IsStoreOrWingetInstall(exe_path, MAX_PATH);
    LOG_DEBUG("程序路径: %s, 是否商店安装: %d", exe_path, isStoreInstall);
    
    if (!isStoreInstall) {
        // 不是应用商店或WinGet安装，标记为已检查
        LOG_DEBUG("不是应用商店安装，标记已检查");
        SetShortcutCheckDone(true);
        return 0;
    }
    
    // 检查桌面是否已有快捷方式
    BOOL hasShortcut = ShortcutExists(exe_path);
    LOG_DEBUG("是否已有快捷方式: %d", hasShortcut);
    
    if (hasShortcut) {
        // 已有快捷方式，标记为已检查
        LOG_DEBUG("已有快捷方式，标记已检查");
        SetShortcutCheckDone(true);
        return 0;
    }
    
    // 创建桌面快捷方式
    LOG_INFO("开始创建快捷方式");
    bool success = CreateDesktopShortcut(exe_path);
    LOG_DEBUG("创建快捷方式结果: %d", success);
    
    // 标记为已检查，无论是否创建成功
    SetShortcutCheckDone(true);
    
    return success ? 0 : 1;
} 