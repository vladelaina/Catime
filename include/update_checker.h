/**
 * @file update_checker.h
 * @brief GitHub版本检查系统 - 自动更新功能
 * @version 2.0 - 重构版：简化API，提升可维护性
 * 
 * 提供应用程序版本检查功能，支持：
 * - 语义化版本比较（Semantic Versioning 2.0.0）
 * - GitHub API集成（自动获取最新release信息）
 * - 预发布版本支持（alpha, beta, rc）
 * - 静默/交互式检查模式
 */

#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief 检查应用程序更新（交互模式）
 * 
 * 执行完整的更新检查流程，显示所有对话框：
 * - 发现新版本时：显示更新提示，包含版本对比和发布说明
 * - 已是最新版本：显示"已是最新版本"对话框
 * - 网络错误：显示错误提示对话框
 * 
 * @param hwnd 父窗口句柄（用于对话框显示）
 * 
 * @note 此函数会阻塞UI线程，建议配合 async_update_checker 使用
 * @see CheckForUpdateAsync 异步版本（推荐）
 */
void CheckForUpdate(HWND hwnd);

/**
 * @brief 检查应用程序更新（可选静默模式）
 * 
 * 与 CheckForUpdate 功能相同，但支持静默模式：
 * - silentCheck=FALSE：等同于 CheckForUpdate
 * - silentCheck=TRUE：仅在发现新版本或出错时显示对话框，已是最新版本时不提示
 * 
 * @param hwnd 父窗口句柄
 * @param silentCheck TRUE=静默模式，FALSE=交互模式
 * 
 * @note 静默模式适用于启动时自动检查，避免打扰用户
 * @see CheckForUpdateAsync 异步版本（推荐）
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck);

/**
 * @brief 比较两个语义化版本号
 * 
 * 支持 Semantic Versioning 2.0.0 规范：
 * - 格式：MAJOR.MINOR.PATCH[-PRERELEASE]
 * - 预发布优先级：alpha < beta < rc < stable
 * - 同类型预发布：比较数字后缀（alpha2 < alpha10）
 * 
 * @param version1 第一个版本字符串（如 "1.3.0-alpha2"）
 * @param version2 第二个版本字符串（如 "1.3.0"）
 * @return 比较结果：
 *         - 负数：version1 < version2
 *         - 0：version1 == version2
 *         - 正数：version1 > version2
 * 
 * @example
 * CompareVersions("1.2.9", "1.3.0") -> -1
 * CompareVersions("1.3.0-alpha2", "1.3.0-beta1") -> -1
 * CompareVersions("1.3.0-rc1", "1.3.0") -> -1
 * CompareVersions("1.3.0", "1.3.0") -> 0
 * CompareVersions("2.0.0", "1.9.9") -> 1
 */
int CompareVersions(const char* version1, const char* version2);

#endif // UPDATE_CHECKER_H
