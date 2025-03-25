/**
 * @file language.c
 * @brief 多语言支持模块实现文件
 * 
 * 本文件实现了应用程序的多语言支持功能，包含语言检测和本地化字符串处理。
 */

#include <windows.h>
#include <wchar.h>
#include "../include/language.h"

/// 全局语言变量，存储当前应用语言设置
AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;  // 默认英语

/**
 * @brief 初始化应用程序语言环境
 * 
 * 根据系统语言自动检测并设置应用程序的当前语言。
 * 支持检测简体中文、繁体中文及其他预设语言。
 */
static void DetectSystemLanguage()
{
    LANGID langID = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(langID)) {
        case LANG_CHINESE:
            // 区分简繁体中文
            if (SUBLANGID(langID) == SUBLANG_CHINESE_SIMPLIFIED) {
                CURRENT_LANGUAGE = APP_LANG_CHINESE_SIMP;
            } else {
                CURRENT_LANGUAGE = APP_LANG_CHINESE_TRAD;
            }
            break;
        case LANG_SPANISH:
            CURRENT_LANGUAGE = APP_LANG_SPANISH;
            break;
        case LANG_FRENCH:
            CURRENT_LANGUAGE = APP_LANG_FRENCH;
            break;
        case LANG_GERMAN:
            CURRENT_LANGUAGE = APP_LANG_GERMAN;
            break;
        case LANG_RUSSIAN:
            CURRENT_LANGUAGE = APP_LANG_RUSSIAN;
            break;
        case LANG_PORTUGUESE:
            CURRENT_LANGUAGE = APP_LANG_PORTUGUESE;
            break;
        case LANG_JAPANESE:
            CURRENT_LANGUAGE = APP_LANG_JAPANESE;
            break;
        case LANG_KOREAN:
            CURRENT_LANGUAGE = APP_LANG_KOREAN;
            break;
        default:
            CURRENT_LANGUAGE = APP_LANG_ENGLISH;  // 默认回退到英语
    }
}

/**
 * @brief 获取本地化字符串
 * @param chinese 简体中文版本的字符串
 * @param english 英语版本的字符串
 * @return const wchar_t* 当前语言对应的字符串指针
 * 
 * 根据当前语言设置返回对应语言的字符串。新增语言支持时需在此函数中添加分支处理。
 * 
 * @note 当前版本优先返回中文，后续可扩展其他语言支持
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    // 首次调用时自动检测系统语言
    static BOOL initialized = FALSE;
    if (!initialized) {
        DetectSystemLanguage();
        initialized = TRUE;
    }

    // 根据当前语言返回对应字符串
    switch (CURRENT_LANGUAGE) {
        case APP_LANG_CHINESE_SIMP:
            if (wcscmp(english, L"Set Countdown") == 0) return L"倒计时";
            if (wcscmp(english, L"Set Time") == 0) return L"倒计时";
            if (wcscmp(english, L"Time's up!") == 0) return L"时间到啦！";
            if (wcscmp(english, L"Show Current Time") == 0) return L"显示当前时间";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24小时制";
            if (wcscmp(english, L"Show Seconds") == 0) return L"显示秒数";
            if (wcscmp(english, L"Time Display") == 0) return L"时间显示";
            if (wcscmp(english, L"Count Up") == 0) return L"正计时";
            if (wcscmp(english, L"Countdown") == 0) return L"倒计时";
            if (wcscmp(english, L"Start") == 0) return L"开始";
            if (wcscmp(english, L"Pause") == 0) return L"暂停";
            if (wcscmp(english, L"Resume") == 0) return L"继续";
            if (wcscmp(english, L"Start Over") == 0) return L"重新开始";
            if (wcscmp(english, L"Restart") == 0) return L"重启";
            if (wcscmp(english, L"Edit Mode") == 0) return L"编辑模式";
            if (wcscmp(english, L"Show Message") == 0) return L"显示消息";
            if (wcscmp(english, L"Lock Screen") == 0) return L"锁定屏幕";
            if (wcscmp(english, L"Shutdown") == 0) return L"关机";
            if (wcscmp(english, L"Timeout Action") == 0) return L"超时动作";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"修改时间选项";
            if (wcscmp(english, L"Customize") == 0) return L"自定义";
            if (wcscmp(english, L"Color") == 0) return L"颜色";
            if (wcscmp(english, L"Font") == 0) return L"字体";
            if (wcscmp(english, L"Version: %hs") == 0) return L"版本: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"反馈";
            if (wcscmp(english, L"Check for Updates") == 0) return L"检查更新";
            if (wcscmp(english, L"About") == 0) return L"关于";
            if (wcscmp(english, L"Reset") == 0) return L"重置";
            if (wcscmp(english, L"Exit") == 0) return L"退出";
            if (wcscmp(english, L"Settings") == 0) return L"设置";
            if (wcscmp(english, L"Preset Manager") == 0) return L"预设管理";
            if (wcscmp(english, L"Startup Settings") == 0) return L"启动设置";
            if (wcscmp(english, L"Start with Windows") == 0) return L"开机自启动";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"所有番茄钟循环完成！";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"休息结束！重新开始工作！";
            if (wcscmp(english, L"Error") == 0) return L"错误";
            if (wcscmp(english, L"Failed to open file") == 0) return L"无法打开文件";
            if (wcscmp(english, L"Timer Control") == 0) return L"计时管理";
            if (wcscmp(english, L"Pomodoro") == 0) return L"番茄时钟";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"循环次数: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"置顶";
            if (wcscmp(english, L"Save Preset") == 0) return L"保存预设";
            if (wcscmp(english, L"Load Preset") == 0) return L"加载预设";
            if (wcscmp(english, L"Delete Preset") == 0) return L"删除预设";
            if (wcscmp(english, L"Create New Preset") == 0) return L"创建新预设";
            if (wcscmp(english, L"Preset Name") == 0) return L"预设名称";
            if (wcscmp(english, L"Select Preset") == 0) return L"选择预设";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"确认删除";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"确定要删除这个预设吗？";
            if (wcscmp(english, L"Open File/Software") == 0) return L"打开文件/软件";
            if (wcscmp(english, L"No Display") == 0) return L"不显示";
            if (wcscmp(english, L"Preset Management") == 0) return L"预设管理";
            if (wcscmp(english, L"Color Value") == 0) return L"颜色值";
            if (wcscmp(english, L"Color Panel") == 0) return L"颜色面板";
            if (wcscmp(english, L"More") == 0) return L"更多";
            if (wcscmp(english, L"Help") == 0) return L"帮助";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"工作中: %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"短休息: %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"长休息";
            if (wcscmp(english, L"Time to focus!") == 0) return L"专注时间到了！";
            if (wcscmp(english, L"Time for a break!") == 0) return L"休息时间到了！";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"已完成: %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"浏览...";
            if (wcscmp(english, L"Open File/Software") == 0) return L"打开文件/软件";
            if (wcscmp(english, L"Open Website") == 0) return L"打开网站";
            if (wcscmp(english, L"Combination") == 0) return L"组合";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"設定為啟動時不顯示";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"設定為啟動時正計時";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"設定為啟動時倒計時";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"請輸入以空格分隔的數字\n範例: 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分鐘30秒\n25 30m = 25小時30分鐘\n1 30 20 = 1小時30分鐘20秒") == 0)
                return L"25    = 25分鐘\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分鐘30秒\n25 30m = 25小時30分鐘\n1 30 20 = 1小時30分鐘20秒";
            return chinese;
            
        case APP_LANG_CHINESE_TRAD:
            if (wcscmp(english, L"Set Countdown") == 0) return L"倒計時";
            if (wcscmp(english, L"Set Time") == 0) return L"倒計時";
            if (wcscmp(english, L"Time's up!") == 0) return L"時間到了！";
            if (wcscmp(english, L"Show Current Time") == 0) return L"顯示目前時間";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24小時制";
            if (wcscmp(english, L"Show Seconds") == 0) return L"顯示秒數";
            if (wcscmp(english, L"Time Display") == 0) return L"時間顯示";
            if (wcscmp(english, L"Count Up") == 0) return L"正計時";
            if (wcscmp(english, L"Countdown") == 0) return L"倒計時";
            if (wcscmp(english, L"Start") == 0) return L"開始";
            if (wcscmp(english, L"Pause") == 0) return L"暫停";
            if (wcscmp(english, L"Resume") == 0) return L"繼續";
            if (wcscmp(english, L"Start Over") == 0) return L"重新開始";
            if (wcscmp(english, L"Restart") == 0) return L"重新啟動";
            if (wcscmp(english, L"Edit Mode") == 0) return L"編輯模式";
            if (wcscmp(english, L"Show Message") == 0) return L"顯示訊息";
            if (wcscmp(english, L"Lock Screen") == 0) return L"鎖定螢幕";
            if (wcscmp(english, L"Shutdown") == 0) return L"關機";
            if (wcscmp(english, L"Timeout Action") == 0) return L"逾時動作";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"修改時間選項";
            if (wcscmp(english, L"Customize") == 0) return L"自訂";
            if (wcscmp(english, L"Color") == 0) return L"顏色";
            if (wcscmp(english, L"Font") == 0) return L"字型";
            if (wcscmp(english, L"Version: %hs") == 0) return L"版本: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"意見回饋";
            if (wcscmp(english, L"Check for Updates") == 0) return L"檢查更新";
            if (wcscmp(english, L"About") == 0) return L"關於";
            if (wcscmp(english, L"Reset") == 0) return L"重設";
            if (wcscmp(english, L"Exit") == 0) return L"結束";
            if (wcscmp(english, L"Settings") == 0) return L"設定";
            if (wcscmp(english, L"Preset Manager") == 0) return L"預設管理";
            if (wcscmp(english, L"Startup Settings") == 0) return L"啟動設定";
            if (wcscmp(english, L"Start with Windows") == 0) return L"開機時啟動";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"所有番茄鐘循環已完成！";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"休息結束！該專注了！";
            if (wcscmp(english, L"Error") == 0) return L"錯誤";
            if (wcscmp(english, L"Failed to open file") == 0) return L"無法開啟檔案";
            if (wcscmp(english, L"Timer Control") == 0) return L"計時器控制";
            if (wcscmp(english, L"Pomodoro") == 0) return L"番茄鐘";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"循環次數: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"視窗置頂";
            if (wcscmp(english, L"Save Preset") == 0) return L"儲存預設";
            if (wcscmp(english, L"Load Preset") == 0) return L"載入預設";
            if (wcscmp(english, L"Delete Preset") == 0) return L"刪除預設";
            if (wcscmp(english, L"Create New Preset") == 0) return L"建立新預設";
            if (wcscmp(english, L"Preset Name") == 0) return L"預設名稱";
            if (wcscmp(english, L"Select Preset") == 0) return L"選擇預設";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"確認刪除";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"確定要刪除此預設嗎？";
            if (wcscmp(english, L"Open File/Software") == 0) return L"開啟檔案/軟體";
            if (wcscmp(english, L"No Display") == 0) return L"不顯示";
            if (wcscmp(english, L"Preset Management") == 0) return L"預設管理";
            if (wcscmp(english, L"Color Value") == 0) return L"顏色數值";
            if (wcscmp(english, L"Color Panel") == 0) return L"顏色面板";
            if (wcscmp(english, L"More") == 0) return L"更多";
            if (wcscmp(english, L"Help") == 0) return L"說明";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"工作中: %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"短休息: %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"長休息";
            if (wcscmp(english, L"Time to focus!") == 0) return L"該專注了！";
            if (wcscmp(english, L"Time for a break!") == 0) return L"該休息了！";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"已完成: %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"瀏覽...";
            if (wcscmp(english, L"Open File/Software") == 0) return L"開啟檔案/軟體";
            if (wcscmp(english, L"Open Website") == 0) return L"開啟網站";
            if (wcscmp(english, L"Combination") == 0) return L"組合";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"設定為啟動時不顯示";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"設定為啟動時正計時";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"設定為啟動時倒計時";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"請輸入以空格分隔的數字\n範例: 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分鐘30秒\n25 30m = 25小時30分鐘\n1 30 20 = 1小時30分鐘20秒") == 0)
                return L"25    = 25分鐘\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分鐘30秒\n25 30m = 25小時30分鐘\n1 30 20 = 1小時30分鐘20秒";
            return chinese;

        case APP_LANG_SPANISH:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Cuenta regresiva";
            if (wcscmp(english, L"Set Time") == 0) return L"Establecer tiempo";
            if (wcscmp(english, L"Time's up!") == 0) return L"¡Tiempo terminado!";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Mostrar hora actual";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Formato 24 horas";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Mostrar segundos";
            if (wcscmp(english, L"Time Display") == 0) return L"Visualización de tiempo";
            if (wcscmp(english, L"Count Up") == 0) return L"Cronómetro";
            if (wcscmp(english, L"Countdown") == 0) return L"Cuenta regresiva";
            if (wcscmp(english, L"Start") == 0) return L"Iniciar";
            if (wcscmp(english, L"Pause") == 0) return L"Pausar";
            if (wcscmp(english, L"Resume") == 0) return L"Reanudar";
            if (wcscmp(english, L"Start Over") == 0) return L"Comenzar de nuevo";
            if (wcscmp(english, L"Restart") == 0) return L"Reiniciar";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Modo de edición";
            if (wcscmp(english, L"Show Message") == 0) return L"Mostrar mensaje";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bloquear pantalla";
            if (wcscmp(english, L"Shutdown") == 0) return L"Apagar";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Acción al finalizar";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modificar opciones de tiempo";
            if (wcscmp(english, L"Customize") == 0) return L"Personalizar";
            if (wcscmp(english, L"Color") == 0) return L"Color";
            if (wcscmp(english, L"Font") == 0) return L"Fuente";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versión: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Comentarios";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Buscar actualizaciones";
            if (wcscmp(english, L"About") == 0) return L"Acerca de";
            if (wcscmp(english, L"Reset") == 0) return L"Restablecer";
            if (wcscmp(english, L"Exit") == 0) return L"Salir";
            if (wcscmp(english, L"Settings") == 0) return L"Configuración";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gestor de ajustes";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Configuración de inicio";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Iniciar con Windows";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"¡Todos los ciclos Pomodoro completados!";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"¡Descanso terminado! ¡Hora de volver a concentrarse!";
            if (wcscmp(english, L"Error") == 0) return L"Error";
            if (wcscmp(english, L"Failed to open file") == 0) return L"Error al abrir el archivo";
            if (wcscmp(english, L"Timer Control") == 0) return L"Control del temporizador";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Pomodoro";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Número de ciclos: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Siempre visible";
            if (wcscmp(english, L"Save Preset") == 0) return L"Guardar ajuste";
            if (wcscmp(english, L"Load Preset") == 0) return L"Cargar ajuste";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Eliminar ajuste";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Crear nuevo ajuste";
            if (wcscmp(english, L"Preset Name") == 0) return L"Nombre del ajuste";
            if (wcscmp(english, L"Select Preset") == 0) return L"Seleccionar ajuste";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Confirmar eliminación";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"¿Está seguro de que desea eliminar este ajuste?";
            if (wcscmp(english, L"Open File/Software") == 0) return L"Abrir archivo/programa";
            if (wcscmp(english, L"No Display") == 0) return L"Sin mostrar";
            if (wcscmp(english, L"Preset Management") == 0) return L"Gestión de ajustes";
            if (wcscmp(english, L"Color Value") == 0) return L"Valor del color";
            if (wcscmp(english, L"Color Panel") == 0) return L"Panel de colores";
            if (wcscmp(english, L"More") == 0) return L"Más";
            if (wcscmp(english, L"Help") == 0) return L"Ayuda";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"Trabajando: %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"Descanso corto: %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"Descanso largo";
            if (wcscmp(english, L"Time to focus!") == 0) return L"¡Hora de concentrarse!";
            if (wcscmp(english, L"Time for a break!") == 0) return L"¡Hora de descansar!";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"Completado: %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"Examinar...";
            if (wcscmp(english, L"Open Website") == 0) return L"Abrir sitio web";
            if (wcscmp(english, L"Combination") == 0) return L"Combinación";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"No mostrar al inicio";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Iniciar como cronómetro";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Iniciar como cuenta regresiva";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"Ingrese números separados por espacios\nEjemplo: 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 horas\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos";
            return english;

        case APP_LANG_FRENCH:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Compte à rebours";
            if (wcscmp(english, L"Set Time") == 0) return L"Régler le temps";
            if (wcscmp(english, L"Time's up!") == 0) return L"Temps écoulé !";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Afficher l'heure actuelle";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Format 24 heures";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Afficher les secondes";
            if (wcscmp(english, L"Time Display") == 0) return L"Affichage du temps";
            if (wcscmp(english, L"Count Up") == 0) return L"Chronomètre";
            if (wcscmp(english, L"Countdown") == 0) return L"Compte à rebours";
            if (wcscmp(english, L"Start") == 0) return L"Démarrer";
            if (wcscmp(english, L"Pause") == 0) return L"Pause";
            if (wcscmp(english, L"Resume") == 0) return L"Reprendre";
            if (wcscmp(english, L"Start Over") == 0) return L"Recommencer";
            if (wcscmp(english, L"Restart") == 0) return L"Redémarrer";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Mode édition";
            if (wcscmp(english, L"Show Message") == 0) return L"Afficher le message";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Verrouiller l'écran";
            if (wcscmp(english, L"Shutdown") == 0) return L"Éteindre";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Action à l'expiration";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modifier les options de temps";
            if (wcscmp(english, L"Customize") == 0) return L"Personnaliser";
            if (wcscmp(english, L"Color") == 0) return L"Couleur";
            if (wcscmp(english, L"Font") == 0) return L"Police";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version : %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Commentaires";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Vérifier les mises à jour";
            if (wcscmp(english, L"About") == 0) return L"À propos";
            if (wcscmp(english, L"Reset") == 0) return L"Réinitialiser";
            if (wcscmp(english, L"Exit") == 0) return L"Quitter";
            if (wcscmp(english, L"Settings") == 0) return L"Paramètres";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gestionnaire de préréglages";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Paramètres de démarrage";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Démarrer avec Windows";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"Tous les cycles Pomodoro sont terminés !";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"Pause terminée ! C'est l'heure de se concentrer !";
            if (wcscmp(english, L"Error") == 0) return L"Erreur";
            if (wcscmp(english, L"Failed to open file") == 0) return L"Échec de l'ouverture du fichier";
            if (wcscmp(english, L"Timer Control") == 0) return L"Contrôle du minuteur";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Pomodoro";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Nombre de cycles : %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Toujours au premier plan";
            if (wcscmp(english, L"Save Preset") == 0) return L"Enregistrer le préréglage";
            if (wcscmp(english, L"Load Preset") == 0) return L"Charger le préréglage";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Supprimer le préréglage";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Créer un nouveau préréglage";
            if (wcscmp(english, L"Preset Name") == 0) return L"Nom du préréglage";
            if (wcscmp(english, L"Select Preset") == 0) return L"Sélectionner un préréglage";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Confirmer la suppression";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"Êtes-vous sûr de vouloir supprimer ce préréglage ?";
            if (wcscmp(english, L"Open File/Software") == 0) return L"Ouvrir un fichier/logiciel";
            if (wcscmp(english, L"No Display") == 0) return L"Aucun affichage";
            if (wcscmp(english, L"Preset Management") == 0) return L"Gestion des préréglages";
            if (wcscmp(english, L"Color Value") == 0) return L"Valeur de couleur";
            if (wcscmp(english, L"Color Panel") == 0) return L"Palette de couleurs";
            if (wcscmp(english, L"More") == 0) return L"Plus";
            if (wcscmp(english, L"Help") == 0) return L"Aide";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"Travail : %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"Pause courte : %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"Pause longue";
            if (wcscmp(english, L"Time to focus!") == 0) return L"C'est l'heure de se concentrer !";
            if (wcscmp(english, L"Time for a break!") == 0) return L"C'est l'heure de faire une pause !";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"Terminé : %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"Parcourir...";
            if (wcscmp(english, L"Open Website") == 0) return L"Ouvrir un site web";
            if (wcscmp(english, L"Combination") == 0) return L"Combinaison";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Ne pas afficher au démarrage";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Démarrer en mode chronomètre";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Démarrer en mode compte à rebours";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"Entrez des nombres séparés par des espaces\nExemple : 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 heures\n25s   = 25 secondes\n25 30 = 25 minutes 30 secondes\n25 30m = 25 heures 30 minutes\n1 30 20 = 1 heure 30 minutes 20 secondes") == 0)
                return L"25    = 25 minutes\n25h   = 25 heures\n25s   = 25 secondes\n25 30 = 25 minutes 30 secondes\n25 30m = 25 heures 30 minutes\n1 30 20 = 1 heure 30 minutes 20 secondes";
            return english;

        case APP_LANG_GERMAN:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Countdown einstellen";
            if (wcscmp(english, L"Set Time") == 0) return L"Zeit einstellen";
            if (wcscmp(english, L"Time's up!") == 0) return L"Zeit ist um!";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Aktuelle Zeit anzeigen";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24-Stunden-Format";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Sekunden anzeigen";
            if (wcscmp(english, L"Time Display") == 0) return L"Zeitanzeige";
            if (wcscmp(english, L"Count Up") == 0) return L"Stoppuhr";
            if (wcscmp(english, L"Countdown") == 0) return L"Countdown";
            if (wcscmp(english, L"Start") == 0) return L"Start";
            if (wcscmp(english, L"Pause") == 0) return L"Pause";
            if (wcscmp(english, L"Resume") == 0) return L"Fortsetzen";
            if (wcscmp(english, L"Start Over") == 0) return L"Neu starten";
            if (wcscmp(english, L"Restart") == 0) return L"Neustart";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Bearbeitungsmodus";
            if (wcscmp(english, L"Show Message") == 0) return L"Nachricht anzeigen";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bildschirm sperren";
            if (wcscmp(english, L"Shutdown") == 0) return L"Herunterfahren";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Zeitüberschreitungsaktion";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Zeitoptionen ändern";
            if (wcscmp(english, L"Customize") == 0) return L"Anpassen";
            if (wcscmp(english, L"Color") == 0) return L"Farbe";
            if (wcscmp(english, L"Font") == 0) return L"Schriftart";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Feedback";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Nach Updates suchen";
            if (wcscmp(english, L"About") == 0) return L"Über";
            if (wcscmp(english, L"Reset") == 0) return L"Zurücksetzen";
            if (wcscmp(english, L"Exit") == 0) return L"Beenden";
            if (wcscmp(english, L"Settings") == 0) return L"Einstellungen";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Voreinstellungen verwalten";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Starteinstellungen";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Mit Windows starten";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"Alle Pomodoro-Zyklen abgeschlossen!";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"Pause vorbei! Zeit, sich wieder zu konzentrieren!";
            if (wcscmp(english, L"Error") == 0) return L"Fehler";
            if (wcscmp(english, L"Failed to open file") == 0) return L"Datei konnte nicht geöffnet werden";
            if (wcscmp(english, L"Timer Control") == 0) return L"Timer-Steuerung";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Pomodoro";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Anzahl der Zyklen: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Immer im Vordergrund";
            if (wcscmp(english, L"Save Preset") == 0) return L"Voreinstellung speichern";
            if (wcscmp(english, L"Load Preset") == 0) return L"Voreinstellung laden";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Voreinstellung löschen";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Neue Voreinstellung erstellen";
            if (wcscmp(english, L"Preset Name") == 0) return L"Name der Voreinstellung";
            if (wcscmp(english, L"Select Preset") == 0) return L"Voreinstellung auswählen";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Löschen bestätigen";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"Möchten Sie diese Voreinstellung wirklich löschen?";
            if (wcscmp(english, L"Open File/Software") == 0) return L"Datei/Programm öffnen";
            if (wcscmp(english, L"No Display") == 0) return L"Keine Anzeige";
            if (wcscmp(english, L"Preset Management") == 0) return L"Voreinstellungsverwaltung";
            if (wcscmp(english, L"Color Value") == 0) return L"Farbwert";
            if (wcscmp(english, L"Color Panel") == 0) return L"Farbauswahl";
            if (wcscmp(english, L"More") == 0) return L"Mehr";
            if (wcscmp(english, L"Help") == 0) return L"Hilfe";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"Arbeitsphase: %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"Kurze Pause: %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"Lange Pause";
            if (wcscmp(english, L"Time to focus!") == 0) return L"Zeit zum Konzentrieren!";
            if (wcscmp(english, L"Time for a break!") == 0) return L"Zeit für eine Pause!";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"Abgeschlossen: %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"Durchsuchen...";
            if (wcscmp(english, L"Open Website") == 0) return L"Webseite öffnen";
            if (wcscmp(english, L"Combination") == 0) return L"Kombination";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Beim Start nicht anzeigen";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Beim Start als Stoppuhr starten";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Beim Start als Countdown starten";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"Geben Sie Zahlen durch Leerzeichen getrennt ein\nBeispiel: 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 Minuten\n25h   = 25 Stunden\n25s   = 25 Sekunden\n25 30 = 25 Minuten 30 Sekunden\n25 30m = 25 Stunden 30 Minuten\n1 30 20 = 1 Stunde 30 Minuten 20 Sekunden";
            return english;

        case APP_LANG_RUSSIAN:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Обратный отсчёт";
            if (wcscmp(english, L"Set Time") == 0) return L"Установить время";
            if (wcscmp(english, L"Time's up!") == 0) return L"Время истекло!";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Показать текущее время";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24-часовой формат";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Показывать секунды";
            if (wcscmp(english, L"Time Display") == 0) return L"Отображение времени";
            if (wcscmp(english, L"Count Up") == 0) return L"Секундомер";
            if (wcscmp(english, L"Countdown") == 0) return L"Обратный отсчёт";
            if (wcscmp(english, L"Start") == 0) return L"Старт";
            if (wcscmp(english, L"Pause") == 0) return L"Пауза";
            if (wcscmp(english, L"Resume") == 0) return L"Продолжить";
            if (wcscmp(english, L"Start Over") == 0) return L"Начать заново";
            if (wcscmp(english, L"Restart") == 0) return L"Перезапустить";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Режим редактирования";
            if (wcscmp(english, L"Show Message") == 0) return L"Показать сообщение";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Заблокировать экран";
            if (wcscmp(english, L"Shutdown") == 0) return L"Выключить компьютер";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Действие по истечении времени";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Настройки времени";
            if (wcscmp(english, L"Customize") == 0) return L"Настроить";
            if (wcscmp(english, L"Color") == 0) return L"Цвет";
            if (wcscmp(english, L"Font") == 0) return L"Шрифт";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Версия: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Обратная связь";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Проверить обновления";
            if (wcscmp(english, L"About") == 0) return L"О программе";
            if (wcscmp(english, L"Reset") == 0) return L"Сбросить";
            if (wcscmp(english, L"Exit") == 0) return L"Выход";
            if (wcscmp(english, L"Settings") == 0) return L"Настройки";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Управление пресетами";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Настройки запуска";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Запускать с Windows";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"Все циклы Помодоро завершены!";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"Перерыв окончен! Время снова сосредоточиться!";
            if (wcscmp(english, L"Error") == 0) return L"Ошибка";
            if (wcscmp(english, L"Failed to open file") == 0) return L"Не удалось открыть файл";
            if (wcscmp(english, L"Timer Control") == 0) return L"Управление таймером";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Помодоро";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Количество циклов: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Поверх всех окон";
            if (wcscmp(english, L"Save Preset") == 0) return L"Сохранить пресет";
            if (wcscmp(english, L"Load Preset") == 0) return L"Загрузить пресет";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Удалить пресет";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Создать новый пресет";
            if (wcscmp(english, L"Preset Name") == 0) return L"Название пресета";
            if (wcscmp(english, L"Select Preset") == 0) return L"Выбрать пресет";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Подтвердить удаление";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"Вы уверены, что хотите удалить этот пресет?";
            if (wcscmp(english, L"Open File/Software") == 0) return L"Открыть файл/программу";
            if (wcscmp(english, L"No Display") == 0) return L"Не отображать";
            if (wcscmp(english, L"Preset Management") == 0) return L"Управление пресетами";
            if (wcscmp(english, L"Color Value") == 0) return L"Значение цвета";
            if (wcscmp(english, L"Color Panel") == 0) return L"Палитра цветов";
            if (wcscmp(english, L"More") == 0) return L"Ещё";
            if (wcscmp(english, L"Help") == 0) return L"Справка";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"Работа: %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"Короткий перерыв: %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"Длинный перерыв";
            if (wcscmp(english, L"Time to focus!") == 0) return L"Время сосредоточиться!";
            if (wcscmp(english, L"Time for a break!") == 0) return L"Время для перерыва!";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"Завершено: %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"Обзор...";
            if (wcscmp(english, L"Open Website") == 0) return L"Открыть сайт";
            if (wcscmp(english, L"Combination") == 0) return L"Комбинация";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Не показывать при запуске";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Запускать как секундомер";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Запускать как обратный отсчёт";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"Введите числа через пробел\nПример: 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 минут\n25h   = 25 часов\n25s   = 25 секунд\n25 30 = 25 минут 30 секунд\n25 30m = 25 часов 30 минут\n1 30 20 = 1 час 30 минут 20 секунд";
            return english;

        case APP_LANG_PORTUGUESE:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Contagem regressiva";
            if (wcscmp(english, L"Set Time") == 0) return L"Definir tempo";
            if (wcscmp(english, L"Time's up!") == 0) return L"Tempo esgotado!";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Mostrar hora atual";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Formato 24 horas";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Mostrar segundos";
            if (wcscmp(english, L"Time Display") == 0) return L"Exibição de tempo";
            if (wcscmp(english, L"Count Up") == 0) return L"Cronômetro";
            if (wcscmp(english, L"Countdown") == 0) return L"Contagem regressiva";
            if (wcscmp(english, L"Start") == 0) return L"Iniciar";
            if (wcscmp(english, L"Pause") == 0) return L"Pausar";
            if (wcscmp(english, L"Resume") == 0) return L"Retomar";
            if (wcscmp(english, L"Start Over") == 0) return L"Recomeçar";
            if (wcscmp(english, L"Restart") == 0) return L"Reiniciar";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Modo de edição";
            if (wcscmp(english, L"Show Message") == 0) return L"Mostrar mensagem";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bloquear tela";
            if (wcscmp(english, L"Shutdown") == 0) return L"Desligar";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Ação ao finalizar";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modificar opções de tempo";
            if (wcscmp(english, L"Customize") == 0) return L"Personalizar";
            if (wcscmp(english, L"Color") == 0) return L"Cor";
            if (wcscmp(english, L"Font") == 0) return L"Fonte";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versão: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Feedback";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Verificar atualizações";
            if (wcscmp(english, L"About") == 0) return L"Sobre";
            if (wcscmp(english, L"Reset") == 0) return L"Redefinir";
            if (wcscmp(english, L"Exit") == 0) return L"Sair";
            if (wcscmp(english, L"Settings") == 0) return L"Configurações";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gerenciador de predefinições";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Configurações de inicialização";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Iniciar com o Windows";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"Todos os ciclos Pomodoro concluídos!";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"Intervalo terminado! Hora de focar novamente!";
            if (wcscmp(english, L"Error") == 0) return L"Erro";
            if (wcscmp(english, L"Failed to open file") == 0) return L"Falha ao abrir arquivo";
            if (wcscmp(english, L"Timer Control") == 0) return L"Controle do temporizador";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Pomodoro";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Número de ciclos: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Sempre visível";
            if (wcscmp(english, L"Save Preset") == 0) return L"Salvar predefinição";
            if (wcscmp(english, L"Load Preset") == 0) return L"Carregar predefinição";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Excluir predefinição";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Criar nova predefinição";
            if (wcscmp(english, L"Preset Name") == 0) return L"Nome da predefinição";
            if (wcscmp(english, L"Select Preset") == 0) return L"Selecionar predefinição";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Confirmar exclusão";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"Tem certeza que deseja excluir esta predefinição?";
            if (wcscmp(english, L"Open File/Software") == 0) return L"Abrir arquivo/programa";
            if (wcscmp(english, L"No Display") == 0) return L"Não exibir";
            if (wcscmp(english, L"Preset Management") == 0) return L"Gerenciamento de predefinições";
            if (wcscmp(english, L"Color Value") == 0) return L"Valor da cor";
            if (wcscmp(english, L"Color Panel") == 0) return L"Painel de cores";
            if (wcscmp(english, L"More") == 0) return L"Mais";
            if (wcscmp(english, L"Help") == 0) return L"Ajuda";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"Trabalhando: %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"Pausa curta: %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"Pausa longa";
            if (wcscmp(english, L"Time to focus!") == 0) return L"Hora de focar!";
            if (wcscmp(english, L"Time for a break!") == 0) return L"Hora da pausa!";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"Concluído: %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"Procurar...";
            if (wcscmp(english, L"Open Website") == 0) return L"Abrir site";
            if (wcscmp(english, L"Combination") == 0) return L"Combinação";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Não exibir na inicialização";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Iniciar como cronômetro";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Iniciar como contagem regressiva";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"Digite números separados por espaços\nExemplo: 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos";
            return english;

        case APP_LANG_JAPANESE:
            if (wcscmp(english, L"Set Countdown") == 0) return L"カウントダウン設定";
            if (wcscmp(english, L"Set Time") == 0) return L"時間設定";
            if (wcscmp(english, L"Time's up!") == 0) return L"時間になりました！";
            if (wcscmp(english, L"Show Current Time") == 0) return L"現在時刻を表示";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24時間表示";
            if (wcscmp(english, L"Show Seconds") == 0) return L"秒を表示";
            if (wcscmp(english, L"Time Display") == 0) return L"時間表示";
            if (wcscmp(english, L"Count Up") == 0) return L"ストップウォッチ";
            if (wcscmp(english, L"Countdown") == 0) return L"カウントダウン";
            if (wcscmp(english, L"Start") == 0) return L"開始";
            if (wcscmp(english, L"Pause") == 0) return L"一時停止";
            if (wcscmp(english, L"Resume") == 0) return L"再開";
            if (wcscmp(english, L"Start Over") == 0) return L"最初からやり直す";
            if (wcscmp(english, L"Restart") == 0) return L"再起動";
            if (wcscmp(english, L"Edit Mode") == 0) return L"編集モード";
            if (wcscmp(english, L"Show Message") == 0) return L"メッセージを表示";
            if (wcscmp(english, L"Lock Screen") == 0) return L"画面をロック";
            if (wcscmp(english, L"Shutdown") == 0) return L"シャットダウン";
            if (wcscmp(english, L"Timeout Action") == 0) return L"タイムアウト時の動作";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"時間オプションの変更";
            if (wcscmp(english, L"Customize") == 0) return L"カスタマイズ";
            if (wcscmp(english, L"Color") == 0) return L"色";
            if (wcscmp(english, L"Font") == 0) return L"フォント";
            if (wcscmp(english, L"Version: %hs") == 0) return L"バージョン: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"フィードバック";
            if (wcscmp(english, L"Check for Updates") == 0) return L"アップデートを確認";
            if (wcscmp(english, L"About") == 0) return L"このアプリについて";
            if (wcscmp(english, L"Reset") == 0) return L"リセット";
            if (wcscmp(english, L"Exit") == 0) return L"終了";
            if (wcscmp(english, L"Settings") == 0) return L"設定";
            if (wcscmp(english, L"Preset Manager") == 0) return L"プリセット管理";
            if (wcscmp(english, L"Startup Settings") == 0) return L"起動設定";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Windowsの起動時に実行";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"全てのポモドーロサイクルが完了しました！";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"休憩終了！集中タイムの開始です！";
            if (wcscmp(english, L"Error") == 0) return L"エラー";
            if (wcscmp(english, L"Failed to open file") == 0) return L"ファイルを開けませんでした";
            if (wcscmp(english, L"Timer Control") == 0) return L"タイマー制御";
            if (wcscmp(english, L"Pomodoro") == 0) return L"ポモドーロ";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"繰り返し回数: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"常に最前面に表示";
            if (wcscmp(english, L"Save Preset") == 0) return L"プリセットを保存";
            if (wcscmp(english, L"Load Preset") == 0) return L"プリセットを読み込む";
            if (wcscmp(english, L"Delete Preset") == 0) return L"プリセットを削除";
            if (wcscmp(english, L"Create New Preset") == 0) return L"新規プリセットを作成";
            if (wcscmp(english, L"Preset Name") == 0) return L"プリセット名";
            if (wcscmp(english, L"Select Preset") == 0) return L"プリセットを選択";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"削除の確認";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"このプリセットを削除してもよろしいですか？";
            if (wcscmp(english, L"Open File/Software") == 0) return L"ファイル/ソフトウェアを開く";
            if (wcscmp(english, L"No Display") == 0) return L"非表示";
            if (wcscmp(english, L"Preset Management") == 0) return L"プリセット管理";
            if (wcscmp(english, L"Color Value") == 0) return L"色の値";
            if (wcscmp(english, L"Color Panel") == 0) return L"カラーパネル";
            if (wcscmp(english, L"More") == 0) return L"その他";
            if (wcscmp(english, L"Help") == 0) return L"ヘルプ";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"作業中: %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"小休憩: %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"長休憩";
            if (wcscmp(english, L"Time to focus!") == 0) return L"集中タイムの開始です！";
            if (wcscmp(english, L"Time for a break!") == 0) return L"休憩時間です！";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"完了: %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"参照...";
            if (wcscmp(english, L"Open Website") == 0) return L"ウェブサイトを開く";
            if (wcscmp(english, L"Combination") == 0) return L"コンビネーション";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"起動時に非表示で開始";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"起動時にストップウォッチで開始";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"起動時にカウントダウンで開始";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"数字をスペースで区切って入力してください\n例: 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25時間\n25s   = 25秒\n25 30 = 25分30秒\n25 30m = 25時間30分\n1 30 20 = 1時間30分20秒") == 0)
                return L"25    = 25分\n25h   = 25時間\n25s   = 25秒\n25 30 = 25分30秒\n25 30m = 25時間30分\n1 30 20 = 1時間30分20秒";
            return english;

        case APP_LANG_KOREAN:
            if (wcscmp(english, L"Set Countdown") == 0) return L"타이머 설정";
            if (wcscmp(english, L"Set Time") == 0) return L"시간 설정";
            if (wcscmp(english, L"Time's up!") == 0) return L"시간이 다 되었습니다!";
            if (wcscmp(english, L"Show Current Time") == 0) return L"현재 시간 표시";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24시간 형식";
            if (wcscmp(english, L"Show Seconds") == 0) return L"초 표시";
            if (wcscmp(english, L"Time Display") == 0) return L"시간 표시";
            if (wcscmp(english, L"Count Up") == 0) return L"스톱워치";
            if (wcscmp(english, L"Countdown") == 0) return L"타이머";
            if (wcscmp(english, L"Start") == 0) return L"시작";
            if (wcscmp(english, L"Pause") == 0) return L"일시정지";
            if (wcscmp(english, L"Resume") == 0) return L"계속하기";
            if (wcscmp(english, L"Start Over") == 0) return L"처음부터 시작";
            if (wcscmp(english, L"Restart") == 0) return L"다시 시작";
            if (wcscmp(english, L"Edit Mode") == 0) return L"편집 모드";
            if (wcscmp(english, L"Show Message") == 0) return L"메시지 표시";
            if (wcscmp(english, L"Lock Screen") == 0) return L"화면 잠금";
            if (wcscmp(english, L"Shutdown") == 0) return L"시스템 종료";
            if (wcscmp(english, L"Timeout Action") == 0) return L"시간 종료 시 동작";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"시간 옵션 수정";
            if (wcscmp(english, L"Customize") == 0) return L"사용자 지정";
            if (wcscmp(english, L"Color") == 0) return L"색상";
            if (wcscmp(english, L"Font") == 0) return L"글꼴";
            if (wcscmp(english, L"Version: %hs") == 0) return L"버전: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"피드백";
            if (wcscmp(english, L"Check for Updates") == 0) return L"업데이트 확인";
            if (wcscmp(english, L"About") == 0) return L"프로그램 정보";
            if (wcscmp(english, L"Reset") == 0) return L"초기화";
            if (wcscmp(english, L"Exit") == 0) return L"종료";
            if (wcscmp(english, L"Settings") == 0) return L"설정";
            if (wcscmp(english, L"Preset Manager") == 0) return L"프리셋 관리";
            if (wcscmp(english, L"Startup Settings") == 0) return L"시작 설정";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Windows 시작 시 자동 실행";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"모든 뽀모도로 사이클이 완료되었습니다!";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"휴식 종료! 다시 집중할 시간입니다!";
            if (wcscmp(english, L"Error") == 0) return L"오류";
            if (wcscmp(english, L"Failed to open file") == 0) return L"파일을 열 수 없습니다";
            if (wcscmp(english, L"Timer Control") == 0) return L"타이머 제어";
            if (wcscmp(english, L"Pomodoro") == 0) return L"뽀모도로";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"반복 횟수: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"항상 위에 표시";
            if (wcscmp(english, L"Save Preset") == 0) return L"프리셋 저장";
            if (wcscmp(english, L"Load Preset") == 0) return L"프리셋 불러오기";
            if (wcscmp(english, L"Delete Preset") == 0) return L"프리셋 삭제";
            if (wcscmp(english, L"Create New Preset") == 0) return L"새 프리셋 만들기";
            if (wcscmp(english, L"Preset Name") == 0) return L"프리셋 이름";
            if (wcscmp(english, L"Select Preset") == 0) return L"프리셋 선택";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"삭제 확인";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"이 프리셋을 삭제하시겠습니까?";
            if (wcscmp(english, L"Open File/Software") == 0) return L"파일 열기/프로그램";
            if (wcscmp(english, L"No Display") == 0) return L"표시하지 않음";
            if (wcscmp(english, L"Preset Management") == 0) return L"프리셋 관리";
            if (wcscmp(english, L"Color Value") == 0) return L"색상 값";
            if (wcscmp(english, L"Color Panel") == 0) return L"색상 패널";
            if (wcscmp(english, L"More") == 0) return L"더 보기";
            if (wcscmp(english, L"Help") == 0) return L"도움말";
            if (wcscmp(english, L"Working: %d/%d") == 0) return L"작업 중: %d/%d";
            if (wcscmp(english, L"Short Break: %d/%d") == 0) return L"짧은 휴식: %d/%d";
            if (wcscmp(english, L"Long Break") == 0) return L"긴 휴식";
            if (wcscmp(english, L"Time to focus!") == 0) return L"집중할 시간입니다!";
            if (wcscmp(english, L"Time for a break!") == 0) return L"휴식 시간입니다!";
            if (wcscmp(english, L"Completed: %d/%d") == 0) return L"완료: %d/%d";
            if (wcscmp(english, L"Browse...") == 0) return L"찾아보기...";
            if (wcscmp(english, L"Open Website") == 0) return L"웹사이트 열기";
            if (wcscmp(english, L"Combination") == 0) return L"조합";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"시작 시 표시하지 않음";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"시작 시 스톱워치로 실행";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"시작 시 타이머로 실행";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0) 
                return L"숫자를 공백으로 구분하여 입력하세요\n예시: 25 10 5";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25시간\n25s   = 25초\n25 30 = 25분 30초\n25 30m = 25시간 30분\n1 30 20 = 1시간 30분 20초") == 0)
                return L"25    = 25분\n25h   = 25시간\n25s   = 25초\n25 30 = 25분 30초\n25 30m = 25시간 30분\n1 30 20 = 1시간 30분 20초";
            return english;

        case APP_LANG_ENGLISH:
        default:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Set Countdown";
            if (wcscmp(english, L"Set Time") == 0) return L"Set Countdown";
            return english;
    }
    return english;
}
