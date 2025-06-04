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
 * @brief 检查桌面是否已有快捷方式以及快捷方式是否指向当前程序
 * 
 * @param exe_path 程序路径
 * @param shortcut_path_out 如果找到快捷方式，输出快捷方式路径
 * @param shortcut_path_size 快捷方式路径缓冲区大小
 * @param target_path_out 如果找到快捷方式，输出快捷方式目标路径
 * @param target_path_size 目标路径缓冲区大小
 * @return int 0=未找到快捷方式, 1=找到快捷方式且指向当前程序, 2=找到快捷方式但指向其他路径
 */
static int CheckShortcutTarget(const char* exe_path, char* shortcut_path_out, size_t shortcut_path_size, 
                              char* target_path_out, size_t target_path_size) {
    char desktop_path[MAX_PATH];
    char public_desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char link_target[MAX_PATH];
    HRESULT hr;
    IShellLinkA* psl = NULL;
    IPersistFile* ppf = NULL;
    WIN32_FIND_DATAA find_data;
    int result = 0;
    
    // 获取用户桌面路径
    hr = SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path);
    if (FAILED(hr)) {
        LOG_ERROR("获取桌面路径失败, hr=0x%08X", (unsigned int)hr);
        return 0;
    }
    LOG_DEBUG("用户桌面路径: %s", desktop_path);
    
    // 获取公共桌面路径
    hr = SHGetFolderPathA(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, public_desktop_path);
    if (FAILED(hr)) {
        LOG_WARNING("获取公共桌面路径失败, hr=0x%08X", (unsigned int)hr);
    } else {
        LOG_DEBUG("公共桌面路径: %s", public_desktop_path);
    }
    
    // 首先检查用户桌面 - 构建快捷方式完整路径（Catime.lnk）
    snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", desktop_path);
    LOG_DEBUG("检查用户桌面快捷方式: %s", shortcut_path);
    
    // 检查用户桌面快捷方式文件是否存在
    bool file_exists = (GetFileAttributesA(shortcut_path) != INVALID_FILE_ATTRIBUTES);
    
    // 如果用户桌面没有，检查公共桌面
    if (!file_exists && SUCCEEDED(hr)) {
        snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", public_desktop_path);
        LOG_DEBUG("检查公共桌面快捷方式: %s", shortcut_path);
        
        file_exists = (GetFileAttributesA(shortcut_path) != INVALID_FILE_ATTRIBUTES);
    }
    
    // 如果没找到快捷方式文件，直接返回0
    if (!file_exists) {
        LOG_DEBUG("未找到任何快捷方式文件");
        return 0;
    }
    
    // 保存找到的快捷方式路径到输出参数
    if (shortcut_path_out && shortcut_path_size > 0) {
        strncpy(shortcut_path_out, shortcut_path, shortcut_path_size);
        shortcut_path_out[shortcut_path_size - 1] = '\0';
    }
    
    // 找到了快捷方式文件，获取其指向的目标
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkA, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("创建IShellLink接口失败, hr=0x%08X", (unsigned int)hr);
        return 0;
    }
    
    // 获取IPersistFile接口
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("获取IPersistFile接口失败, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return 0;
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
        return 0;
    }
    
    // 获取快捷方式目标路径
    hr = psl->lpVtbl->GetPath(psl, link_target, MAX_PATH, &find_data, 0);
    if (FAILED(hr)) {
        LOG_ERROR("获取快捷方式目标路径失败, hr=0x%08X", (unsigned int)hr);
        result = 0;
    } else {
        LOG_DEBUG("快捷方式目标路径: %s", link_target);
        LOG_DEBUG("当前程序路径: %s", exe_path);
        
        // 保存目标路径到输出参数
        if (target_path_out && target_path_size > 0) {
            strncpy(target_path_out, link_target, target_path_size);
            target_path_out[target_path_size - 1] = '\0';
        }
        
        // 检查快捷方式是否指向当前程序
        if (_stricmp(link_target, exe_path) == 0) {
            LOG_DEBUG("快捷方式指向当前程序");
            result = 1;
        } else {
            LOG_DEBUG("快捷方式指向其他路径");
            result = 2;
        }
    }
    
    // 释放接口
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return result;
}

/**
 * @brief 创建或更新桌面快捷方式
 * 
 * @param exe_path 程序路径
 * @param existing_shortcut_path 已存在的快捷方式路径，为NULL则创建新的
 * @return bool true表示创建/更新成功，false表示失败
 */
static bool CreateOrUpdateDesktopShortcut(const char* exe_path, const char* existing_shortcut_path) {
    char desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char icon_path[MAX_PATH];
    HRESULT hr;
    IShellLinkA* psl = NULL;
    IPersistFile* ppf = NULL;
    bool success = false;
    
    // 如果提供了已存在的快捷方式路径，使用它；否则创建新的快捷方式在用户桌面
    if (existing_shortcut_path && *existing_shortcut_path) {
        LOG_INFO("开始更新桌面快捷方式: %s 指向: %s", existing_shortcut_path, exe_path);
        strcpy(shortcut_path, existing_shortcut_path);
    } else {
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
    }
    
    LOG_DEBUG("快捷方式路径: %s", shortcut_path);
    
    // 使用程序路径作为图标路径
    strcpy(icon_path, exe_path);
    
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
    hr = psl->lpVtbl->SetDescription(psl, "A very useful timer (Pomodoro Clock)");
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
        LOG_INFO("桌面快捷方式%s成功: %s", existing_shortcut_path ? "更新" : "创建", shortcut_path);
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
 * 所有安装类型都会检查桌面快捷方式是否存在且指向当前程序，
 * 如果快捷方式已存在但指向其他程序，则更新为当前路径。
 * 但只有从Windows应用商店或WinGet安装的版本才会创建新的快捷方式。
 * 如果配置文件中已标记SHORTCUT_CHECK_DONE=TRUE，则即使快捷方式被删除也不会重新创建。
 * 
 * @return int 0表示无需创建/更新或创建/更新成功，1表示失败
 */
int CheckAndCreateShortcut(void) {
    char exe_path[MAX_PATH];
    char config_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char target_path[MAX_PATH];
    bool shortcut_check_done = false;
    bool isStoreInstall = false;
    
    // 初始化 COM 库，后续创建快捷方式需要
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        LOG_ERROR("COM库初始化失败, hr=0x%08X", (unsigned int)hr);
        return 1;
    }
    
    LOG_DEBUG("开始检查快捷方式");
    
    // 读取配置文件中的标记，判断是否已检查过
    GetConfigPath(config_path, MAX_PATH);
    shortcut_check_done = IsShortcutCheckDone();
    
    LOG_DEBUG("配置路径: %s, 是否已检查: %d", config_path, shortcut_check_done);
    
    // 获取当前程序路径
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
        LOG_ERROR("获取程序路径失败");
        CoUninitialize();
        return 1;
    }
    LOG_DEBUG("程序路径: %s", exe_path);
    
    // 检查是否是应用商店或WinGet安装（仅影响创建新快捷方式的行为）
    isStoreInstall = IsStoreOrWingetInstall(exe_path, MAX_PATH);
    LOG_DEBUG("是否商店/WinGet安装: %d", isStoreInstall);
    
    // 检查快捷方式是否存在以及是否指向当前程序
    // 返回值: 0=不存在, 1=存在且指向当前程序, 2=存在但指向其他程序
    int shortcut_status = CheckShortcutTarget(exe_path, shortcut_path, MAX_PATH, target_path, MAX_PATH);
    
    if (shortcut_status == 0) {
        // 不存在快捷方式
        if (shortcut_check_done) {
            // 如果配置中已经标记为检查过，即使没有快捷方式也不再创建
            LOG_INFO("桌面未发现快捷方式，但配置已标记为检查过，不再创建");
            CoUninitialize();
            return 0;
        } else if (isStoreInstall) {
            // 只有首次运行的商店或WinGet安装才创建
            LOG_INFO("桌面未发现快捷方式，首次运行的商店/WinGet安装，开始创建");
            bool success = CreateOrUpdateDesktopShortcut(exe_path, NULL);
            
            // 标记为已检查，无论是否创建成功
            SetShortcutCheckDone(true);
            
            CoUninitialize();
            return success ? 0 : 1;
        } else {
            LOG_INFO("桌面未发现快捷方式，非商店/WinGet安装，不创建快捷方式");
            
            // 标记为已检查
            SetShortcutCheckDone(true);
            
            CoUninitialize();
            return 0;
        }
    } else if (shortcut_status == 1) {
        // 存在快捷方式且指向当前程序，无需操作
        LOG_INFO("桌面快捷方式已存在且指向当前程序");
        
        // 标记为已检查
        if (!shortcut_check_done) {
            SetShortcutCheckDone(true);
        }
        
        CoUninitialize();
        return 0;
    } else if (shortcut_status == 2) {
        // 存在快捷方式但指向其他程序，任何安装方式都要更新
        LOG_INFO("桌面快捷方式指向其他路径: %s，将更新为: %s", target_path, exe_path);
        bool success = CreateOrUpdateDesktopShortcut(exe_path, shortcut_path);
        
        // 标记为已检查，无论是否更新成功
        if (!shortcut_check_done) {
            SetShortcutCheckDone(true);
        }
        
        CoUninitialize();
        return success ? 0 : 1;
    }
    
    // 不应该走到这里
    LOG_ERROR("检查快捷方式状态未知");
    CoUninitialize();
    return 1;
} 