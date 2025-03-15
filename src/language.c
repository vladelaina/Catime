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
            if (wcscmp(english, L"Focus: %ls") == 0) return L"集中精力: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"短暂休息: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"长时间休息: %ls";
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
            return chinese;
            
        case APP_LANG_CHINESE_TRAD:
            if (wcscmp(english, L"Set Countdown") == 0) return L"倒計時";
            if (wcscmp(english, L"Set Time") == 0) return L"倒計時";
            if (wcscmp(english, L"Time's up!") == 0) return L"時間到啦！";
            if (wcscmp(english, L"Show Current Time") == 0) return L"顯示當前時間";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24小時制";
            if (wcscmp(english, L"Show Seconds") == 0) return L"顯示秒數";
            if (wcscmp(english, L"Time Display") == 0) return L"時間顯示";
            if (wcscmp(english, L"Count Up") == 0) return L"正計時";
            if (wcscmp(english, L"Countdown") == 0) return L"倒計時";
            if (wcscmp(english, L"Start") == 0) return L"開始";
            if (wcscmp(english, L"Pause") == 0) return L"暫停";
            if (wcscmp(english, L"Resume") == 0) return L"繼續";
            if (wcscmp(english, L"Start Over") == 0) return L"重新開始";
            if (wcscmp(english, L"Restart") == 0) return L"重啟";
            if (wcscmp(english, L"Edit Mode") == 0) return L"編輯模式";
            if (wcscmp(english, L"Show Message") == 0) return L"顯示消息";
            if (wcscmp(english, L"Lock Screen") == 0) return L"鎖定螢幕";
            if (wcscmp(english, L"Shutdown") == 0) return L"關機";
            if (wcscmp(english, L"Timeout Action") == 0) return L"超時動作";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"修改時間選項";
            if (wcscmp(english, L"Customize") == 0) return L"自定義";
            if (wcscmp(english, L"Color") == 0) return L"顏色";
            if (wcscmp(english, L"Font") == 0) return L"字體";
            if (wcscmp(english, L"Version: %hs") == 0) return L"版本: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"反饋";
            if (wcscmp(english, L"Check for Updates") == 0) return L"檢查更新";
            if (wcscmp(english, L"About") == 0) return L"關於";
            if (wcscmp(english, L"Reset") == 0) return L"重置";
            if (wcscmp(english, L"Exit") == 0) return L"退出";
            if (wcscmp(english, L"Settings") == 0) return L"設置";
            if (wcscmp(english, L"Preset Manager") == 0) return L"預設管理";
            if (wcscmp(english, L"Startup Settings") == 0) return L"啟動設置";
            if (wcscmp(english, L"Start with Windows") == 0) return L"開機自啟動";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"所有番茄鐘循環完成！";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"休息結束！重新開始工作！";
            if (wcscmp(english, L"Error") == 0) return L"錯誤";
            if (wcscmp(english, L"Failed to open file") == 0) return L"無法打開文件";
            if (wcscmp(english, L"Timer Control") == 0) return L"計時管理";
            if (wcscmp(english, L"Pomodoro") == 0) return L"番茄鐘";
            if (wcscmp(english, L"Focus: %ls") == 0) return L"集中精力: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"短暫休息: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"長時間休息: %ls";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"循環次數: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"置頂";
            if (wcscmp(english, L"Save Preset") == 0) return L"保存預設";
            if (wcscmp(english, L"Load Preset") == 0) return L"加載預設";
            if (wcscmp(english, L"Delete Preset") == 0) return L"刪除預設";
            if (wcscmp(english, L"Create New Preset") == 0) return L"創建新預設";
            if (wcscmp(english, L"Preset Name") == 0) return L"預設名稱";
            if (wcscmp(english, L"Select Preset") == 0) return L"選擇預設";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"確認刪除";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"確定要刪除這個預設嗎？";
            return chinese;

        case APP_LANG_SPANISH:
            if (wcscmp(english, L"Set Time") == 0) return L"Establecer tiempo";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Modo de edición";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Mostrar hora actual";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Formato 24 horas";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Mostrar segundos";
            if (wcscmp(english, L"Time Display") == 0) return L"Visualización de tiempo";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Acción de tiempo";
            if (wcscmp(english, L"Show Message") == 0) return L"Mostrar mensaje";
            if (wcscmp(english, L"Browse...") == 0) return L"Explorar...";
            if (wcscmp(english, L"Open File") == 0) return L"Abrir archivo";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Abrir: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bloquear pantalla";
            if (wcscmp(english, L"Shutdown") == 0) return L"Apagar";
            if (wcscmp(english, L"Restart") == 0) return L"Reiniciar";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modificar opciones";
            if (wcscmp(english, L"Customize") == 0) return L"Personalizar";
            if (wcscmp(english, L"Color") == 0) return L"Color";
            if (wcscmp(english, L"Font") == 0) return L"Fuente";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versión: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Comentarios";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Buscar actualizaciones";
            if (wcscmp(english, L"About") == 0) return L"Acerca de";
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Exit") == 0) return L"Salir";
            if (wcscmp(english, L"¡Tiempo terminado!") == 0) return L"¡Tiempo terminado!";
            if (wcscmp(english, L"Formato de entrada") == 0) return L"Formato de entrada";
            if (wcscmp(english, L"Entrada inválida") == 0) return L"Entrada inválida";
            if (wcscmp(english, L"Error") == 0) return L"Error";
            if (wcscmp(english, L"Error al cargar la fuente: %hs") == 0) return L"Error al cargar la fuente: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Ingrese números separados por espacios\nEjemplo: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"Acerca de";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versión: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Buscar actualizaciones";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"已设置为启动时不显示";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"已设置为启动时正计时";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"已设置为启动时倒计时";
            if (wcscmp(english, L"Settings") == 0) return L"设置";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gestor de Preajustes";
            if (wcscmp(english, L"Count Up") == 0) return L"Contar hacia arriba";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Configuración de inicio";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Iniciar con Windows";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Cuenta regresiva";
            if (wcscmp(english, L"Set Time") == 0) return L"Cuenta regresiva";
            if (wcscmp(english, L"Timer Control") == 0) return L"Control del Temporizador";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Pomodoro";
            if (wcscmp(english, L"Focus: %ls") == 0) return L"Enfoque: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"Descanso Corto: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"Descanso Largo: %ls";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Conteo de Ciclos: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Siempre Visible";
            if (wcscmp(english, L"Save Preset") == 0) return L"Guardar Preajuste";
            if (wcscmp(english, L"Load Preset") == 0) return L"Cargar Preajuste";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Eliminar Preajuste";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Crear Nuevo Preajuste";
            if (wcscmp(english, L"Preset Name") == 0) return L"Nombre del Preajuste";
            if (wcscmp(english, L"Select Preset") == 0) return L"Seleccionar Preajuste";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Confirmar Eliminación";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"¿Está seguro de que desea eliminar este preajuste?";
            return english;

        case APP_LANG_FRENCH:
            if (wcscmp(english, L"Set Time") == 0) return L"Régler l'heure";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Mode édition";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Afficher l'heure actuelle";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Format 24 heures";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Afficher les secondes";
            if (wcscmp(english, L"Time Display") == 0) return L"Affichage de l'heure";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Action de temporisation";
            if (wcscmp(english, L"Show Message") == 0) return L"Afficher le message";
            if (wcscmp(english, L"Browse...") == 0) return L"Parcourir...";
            if (wcscmp(english, L"Open File") == 0) return L"Ouvrir le fichier";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Ouvrir: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Verrouiller l'écran";
            if (wcscmp(english, L"Shutdown") == 0) return L"Arrêter";
            if (wcscmp(english, L"Restart") == 0) return L"Redémarrer";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modifier les options";
            if (wcscmp(english, L"Customize") == 0) return L"Personnaliser";
            if (wcscmp(english, L"Color") == 0) return L"Couleur";
            if (wcscmp(english, L"Font") == 0) return L"Police";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Retour";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Vérifier les mises à jour";
            if (wcscmp(english, L"About") == 0) return L"À propos";
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Exit") == 0) return L"Quitter";
            if (wcscmp(english, L"Temps écoulé !") == 0) return L"Temps écoulé !";
            if (wcscmp(english, L"Formato d'entrée") == 0) return L"Formato d'entrée";
            if (wcscmp(english, L"Entrée invalide") == 0) return L"Entrée invalide";
            if (wcscmp(english, L"Erreur") == 0) return L"Erreur";
            if (wcscmp(english, L"Échec du chargement de la police: %hs") == 0) return L"Échec du chargement de la police: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutes\n25h   = 25 heures\n25s   = 25 secondes\n25 30 = 25 minutes 30 secondes\n25 30m = 25 heures 30 minutes\n1 30 20 = 1 heure 30 minutes 20 secondes";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Entrez des nombres séparés par des espaces\nExemple : 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"À propos";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Vérifier les mises à jour";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"已设置为启动时不显示";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"已设置为启动时正计时";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"已设置为启动时倒计时";
            if (wcscmp(english, L"Settings") == 0) return L"设置";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gestionnaire de Préréglages";
            if (wcscmp(english, L"Count Up") == 0) return L"Compte à rebours positif";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Paramètres de démarrage";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Démarrer avec Windows";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Compte à rebours";
            if (wcscmp(english, L"Set Time") == 0) return L"Compte à rebours";
            if (wcscmp(english, L"Timer Control") == 0) return L"Contrôle du Minuteur";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Pomodoro";
            if (wcscmp(english, L"Focus: %ls") == 0) return L"Concentration: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"Pause Courte: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"Pause Longue: %ls";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Nombre de Cycles: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Toujours au Premier Plan";
            if (wcscmp(english, L"Save Preset") == 0) return L"Enregistrer le Préréglage";
            if (wcscmp(english, L"Load Preset") == 0) return L"Charger le Préréglage";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Supprimer le Préréglage";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Créer un Nouveau Préréglage";
            if (wcscmp(english, L"Preset Name") == 0) return L"Nom du Préréglage";
            if (wcscmp(english, L"Select Preset") == 0) return L"Sélectionner un Préréglage";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Confirmer la Suppression";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"Êtes-vous sûr de vouloir supprimer ce préréglage ?";
            return english;

        case APP_LANG_GERMAN:
            if (wcscmp(english, L"Set Time") == 0) return L"Zeit einstellen";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Bearbeitungsmodus";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Aktuelle Zeit anzeigen";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24-Stunden-Format";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Sekunden anzeigen";
            if (wcscmp(english, L"Time Display") == 0) return L"Zeitanzeige";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Zeitüberschreitungsaktion";
            if (wcscmp(english, L"Show Message") == 0) return L"Nachricht anzeigen";
            if (wcscmp(english, L"Browse...") == 0) return L"Durchsuchen...";
            if (wcscmp(english, L"Open File") == 0) return L"Datei öffnen";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Öffnen: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bildschirm sperren";
            if (wcscmp(english, L"Shutdown") == 0) return L"Herunterfahren";
            if (wcscmp(english, L"Restart") == 0) return L"Neustart";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Zeitoptionen ändern";
            if (wcscmp(english, L"Customize") == 0) return L"Anpassen";
            if (wcscmp(english, L"Color") == 0) return L"Farbe";
            if (wcscmp(english, L"Font") == 0) return L"Schriftart";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Feedback";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Nach Updates suchen";
            if (wcscmp(english, L"About") == 0) return L"Über";
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Exit") == 0) return L"Beenden";
            if (wcscmp(english, L"Zeit ist um!") == 0) return L"Zeit ist um!";
            if (wcscmp(english, L"Eingabeformat") == 0) return L"Eingabeformat";
            if (wcscmp(english, L"Ungültige Eingabe") == 0) return L"Ungültige Eingabe";
            if (wcscmp(english, L"Fehler") == 0) return L"Fehler";
            if (wcscmp(english, L"Schriftart konnte nicht geladen werden: %hs") == 0) return L"Schriftart konnte nicht geladen werden: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 Minuten\n25h   = 25 Stunden\n25s   = 25 Sekunden\n25 30 = 25 Minuten 30 Sekunden\n25 30m = 25 Stunden 30 Minuten\n1 30 20 = 1 Stunde 30 Minuten 20 Sekunden";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Geben Sie durch Leerzeichen getrennte Zahlen ein\nBeispiel: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"Über";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Nach Updates suchen";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Beim Start nicht anzeigen eingestellt";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Als Stoppuhr beim Start eingestellt";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Als Countdown beim Start eingestellt";
            if (wcscmp(english, L"Settings") == 0) return L"Einstellungen";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Voreinstellungen-Manager";
            if (wcscmp(english, L"Count Up") == 0) return L"Aufwärtszählen";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Starteinstellungen";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Mit Windows starten";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Countdown";
            if (wcscmp(english, L"Set Time") == 0) return L"Countdown";
            if (wcscmp(english, L"Timer Control") == 0) return L"Timer-Steuerung";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Pomodoro";
            if (wcscmp(english, L"Focus: %ls") == 0) return L"Fokus: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"Kurze Pause: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"Lange Pause: %ls";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Zyklenanzahl: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Immer im Vordergrund";
            if (wcscmp(english, L"Save Preset") == 0) return L"Voreinstellung Speichern";
            if (wcscmp(english, L"Load Preset") == 0) return L"Voreinstellung Laden";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Voreinstellung Löschen";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Neue Voreinstellung Erstellen";
            if (wcscmp(english, L"Preset Name") == 0) return L"Voreinstellungsname";
            if (wcscmp(english, L"Select Preset") == 0) return L"Voreinstellung Auswählen";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Löschen Bestätigen";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"Sind Sie sicher, dass Sie diese Voreinstellung löschen möchten?";
            return english;

        case APP_LANG_RUSSIAN:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Обратный отсчет";
            if (wcscmp(english, L"Set Time") == 0) return L"Обратный отсчет";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Режим редактирования";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Показать текущее время";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24-часовой формат";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Показать секунды";
            if (wcscmp(english, L"Time Display") == 0) return L"Отображение времени";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Действие по таймауту";
            if (wcscmp(english, L"Show Message") == 0) return L"Показать сообщение";
            if (wcscmp(english, L"Browse...") == 0) return L"Обзор...";
            if (wcscmp(english, L"Open File") == 0) return L"Открыть файл";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Открыть: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Заблокировать экран";
            if (wcscmp(english, L"Shutdown") == 0) return L"Выключение";
            if (wcscmp(english, L"Restart") == 0) return L"Перезагрузка";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Изменить параметры времени";
            if (wcscmp(english, L"Customize") == 0) return L"Настроить";
            if (wcscmp(english, L"Color") == 0) return L"Цвет";
            if (wcscmp(english, L"Font") == 0) return L"Шрифт";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Версия: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Обратная связь";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Проверить обновления";
            if (wcscmp(english, L"About") == 0) return L"О программе";
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Exit") == 0) return L"Выход";
            if (wcscmp(english, L"Время вышло!") == 0) return L"Время вышло!";
            if (wcscmp(english, L"Формат ввода") == 0) return L"Формат ввода";
            if (wcscmp(english, L"Неверный ввод") == 0) return L"Неверный ввод";
            if (wcscmp(english, L"Ошибка") == 0) return L"Ошибка";
            if (wcscmp(english, L"Не удалось загрузить шрифт: %hs") == 0) return L"Не удалось загрузить шрифт: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 час 30 минут 20 секунд") == 0)
                return L"25    = 25 минут\n25h   = 25 часов\n25s   = 25 секунд\n25 30 = 25 минут 30 секунд\n25 30m = 25 часов 30 минут\n1 30 20 = 1 час 30 минут 20 секунд";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Введите числа, разделенные пробелами\nПример: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"О программе";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Версия: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Проверить обновления";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Настроено на скрытый запуск";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Настроено на запуск секундомера";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Настроено на запуск обратного отсчета";
            if (wcscmp(english, L"Settings") == 0) return L"Настройки";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Менеджер Пресетов";
            if (wcscmp(english, L"Count Up") == 0) return L"Счетчик";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Настройки запуска";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Запускать с Windows";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Установить обратный отсчет";
            if (wcscmp(english, L"Set Time") == 0) return L"Установить обратный отсчет";
            if (wcscmp(english, L"Timer Control") == 0) return L"Таймер управления";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Помодоро";
            if (wcscmp(english, L"Focus: %ls") == 0) return L"Фокус: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"Короткий перерыв: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"Длительный перерыв: %ls";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Количество циклов: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Всегда на переднем плане";
            if (wcscmp(english, L"Save Preset") == 0) return L"Сохранить Пресет";
            if (wcscmp(english, L"Load Preset") == 0) return L"Загрузить Пресет";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Удалить Пресет";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Создать Новый Пресет";
            if (wcscmp(english, L"Preset Name") == 0) return L"Имя Пресета";
            if (wcscmp(english, L"Select Preset") == 0) return L"Выбрать Пресет";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Подтвердить Удаление";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"Вы уверены, что хотите удалить этот пресет?";
            return english;

        case APP_LANG_PORTUGUESE:
            if (wcscmp(english, L"Set Time") == 0) return L"Definir tempo";
            if (wcscmp(english, L"Edit Mode") == 0) return L"Modo de edição";
            if (wcscmp(english, L"Show Current Time") == 0) return L"Mostrar hora atual";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"Formato 24 horas";
            if (wcscmp(english, L"Show Seconds") == 0) return L"Mostrar segundos";
            if (wcscmp(english, L"Time Display") == 0) return L"Exibição de tempo";
            if (wcscmp(english, L"Timeout Action") == 0) return L"Ação de timeout";
            if (wcscmp(english, L"Show Message") == 0) return L"Mostrar mensagem";
            if (wcscmp(english, L"Browse...") == 0) return L"Navegar...";
            if (wcscmp(english, L"Open File") == 0) return L"Abrir arquivo";
            if (wcscmp(english, L"Open: %hs") == 0) return L"Abrir: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"Bloquear tela";
            if (wcscmp(english, L"Shutdown") == 0) return L"Desligar";
            if (wcscmp(english, L"Restart") == 0) return L"Reiniciar";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"Modificar opções";
            if (wcscmp(english, L"Customize") == 0) return L"Personalizar";
            if (wcscmp(english, L"Color") == 0) return L"Cor";
            if (wcscmp(english, L"Font") == 0) return L"Fonte";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versão: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"Feedback";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"Verificar atualizações";
            if (wcscmp(english, L"About") == 0) return L"Sobre";
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Exit") == 0) return L"Sair";
            if (wcscmp(english, L"Tempo esgotado!") == 0) return L"Tempo esgotado!";
            if (wcscmp(english, L"Formato de entrada") == 0) return L"Formato de entrada";
            if (wcscmp(english, L"Entrada inválida") == 0) return L"Entrada inválida";
            if (wcscmp(english, L"Erro") == 0) return L"Erro";
            if (wcscmp(english, L"Falha ao carregar fonte: %hs") == 0) return L"Falha ao carregar fonte: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Insira números separados por espaços\nExemplo: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"Sobre";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Versão: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Verificar atualizações";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Configurado para não exibir na inicialização";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Configurado como cronômetro na inicialização";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Configurado como contagem regressiva na inicialização";
            if (wcscmp(english, L"Settings") == 0) return L"Configurações";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gerenciador de Predefinições";
            if (wcscmp(english, L"Count Up") == 0) return L"Contagem ascendente";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Configurações de inicialização";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Iniciar com o Windows";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Establecer cuenta regresiva";
            if (wcscmp(english, L"Set Time") == 0) return L"Establecer cuenta regresiva";
            if (wcscmp(english, L"Timer Control") == 0) return L"Controle do Timer";
            if (wcscmp(english, L"Pomodoro") == 0) return L"Pomodoro";
            if (wcscmp(english, L"Focus: %ls") == 0) return L"Foco: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"Pausa Curta: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"Pausa Longa: %ls";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"Contagem de Ciclos: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"Sempre Visível";
            if (wcscmp(english, L"Save Preset") == 0) return L"Salvar Predefinição";
            if (wcscmp(english, L"Load Preset") == 0) return L"Carregar Predefinição";
            if (wcscmp(english, L"Delete Preset") == 0) return L"Excluir Predefinição";
            if (wcscmp(english, L"Create New Preset") == 0) return L"Criar Nova Predefinição";
            if (wcscmp(english, L"Preset Name") == 0) return L"Nome da Predefinição";
            if (wcscmp(english, L"Select Preset") == 0) return L"Selecionar Predefinição";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"Confirmer Exclusão";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"Tem certeza que deseja excluir esta predefinição?";
            return english;

        case APP_LANG_JAPANESE:
            if (wcscmp(english, L"Set Countdown") == 0) return L"カウントダウン";
            if (wcscmp(english, L"Set Time") == 0) return L"カウントダウン";
            if (wcscmp(english, L"Edit Mode") == 0) return L"編集モード";
            if (wcscmp(english, L"Show Current Time") == 0) return L"現在時刻を表示";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24時間表示";
            if (wcscmp(english, L"Show Seconds") == 0) return L"秒を表示";
            if (wcscmp(english, L"Time Display") == 0) return L"時間表示";
            if (wcscmp(english, L"Timeout Action") == 0) return L"タイムアウト動作";
            if (wcscmp(english, L"Show Message") == 0) return L"メッセージを表示";
            if (wcscmp(english, L"Browse...") == 0) return L"参照...";
            if (wcscmp(english, L"Open File") == 0) return L"ファイルを開く";
            if (wcscmp(english, L"Open: %hs") == 0) return L"開く: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"画面をロック";
            if (wcscmp(english, L"Shutdown") == 0) return L"シャットダウン";
            if (wcscmp(english, L"Restart") == 0) return L"再起動";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"時間オプションを変更";
            if (wcscmp(english, L"Customize") == 0) return L"カスタマイズ";
            if (wcscmp(english, L"Color") == 0) return L"色";
            if (wcscmp(english, L"Font") == 0) return L"フォント";
            if (wcscmp(english, L"Version: %hs") == 0) return L"バージョン: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"フィードバック";
            if (wcscmp(english, L"Check for Updates") == 0) return L"更新を確認";
            if (wcscmp(english, L"About") == 0) return L"について";
            if (wcscmp(english, L"Reset") == 0) return L"リセット";
            if (wcscmp(english, L"Exit") == 0) return L"終了";
            if (wcscmp(english, L"Time's up!") == 0) return L"時間切れです!";
            if (wcscmp(english, L"Start") == 0) return L"開始";
            if (wcscmp(english, L"Pause") == 0) return L"一時停止";
            if (wcscmp(english, L"Resume") == 0) return L"再開";
            if (wcscmp(english, L"Start Over") == 0) return L"最初からやり直す";
            if (wcscmp(english, L"Count Up") == 0) return L"カウントアップ";
            if (wcscmp(english, L"Countdown") == 0) return L"カウントダウン";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"すべてのポモドーロサイクルが完了しました!";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"休憩終了！再び集中する時間です。";
            if (wcscmp(english, L"Failed to open file") == 0) return L"ファイルを開けませんでした";
            if (wcscmp(english, L"Error") == 0) return L"エラー";
            if (wcscmp(english, L"時間切れです!") == 0) return L"時間切れです!";
            if (wcscmp(english, L"入力形式") == 0) return L"入力形式";
            if (wcscmp(english, L"無効な入力") == 0) return L"無効な入力";
            if (wcscmp(english, L"エラー") == 0) return L"エラー";
            if (wcscmp(english, L"フォントの読み込みに失敗しました: %hs") == 0) return L"フォントの読み込みに失敗しました: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25分\n25h   = 25時間\n25s   = 25秒\n25 30 = 25分30秒\n25 30m = 25時間30分\n1 30 20 = 1時間30分20秒";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"日本語で入力してください\n例: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"について";
            if (wcscmp(english, L"Version: %hs") == 0) return L"バージョン: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"更新を確認";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"起動時に非表示に設定";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"起動時にストップウォッチに設定";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"起動時にカウントダウンに設定";
            if (wcscmp(english, L"Settings") == 0) return L"設定";
            if (wcscmp(english, L"Preset Manager") == 0) return L"プリセットマネージャー";
            if (wcscmp(english, L"Count Up") == 0) return L"カウントアップ";
            if (wcscmp(english, L"Startup Settings") == 0) return L"起動設定";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Windowsと同時に起動";
            if (wcscmp(english, L"Set Countdown") == 0) return L"カウントダウン";
            if (wcscmp(english, L"Set Time") == 0) return L"カウントダウン";
            if (wcscmp(english, L"Timer Control") == 0) return L"タイマー制御";
            if (wcscmp(english, L"Pomodoro") == 0) return L"ポモドーロ";
            if (wcscmp(english, L"Focus: %ls") == 0) return L"集中: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"短い休憩: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"長い休憩: %ls";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"ループ回数: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"常に最前面";
            if (wcscmp(english, L"Save Preset") == 0) return L"プリセットを保存";
            if (wcscmp(english, L"Load Preset") == 0) return L"プリセットを読み込む";
            if (wcscmp(english, L"Delete Preset") == 0) return L"プリセットを削除";
            if (wcscmp(english, L"Create New Preset") == 0) return L"新しいプリセットを作成";
            if (wcscmp(english, L"Preset Name") == 0) return L"プリセット名";
            if (wcscmp(english, L"Select Preset") == 0) return L"プリセットを選択";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"削除の確認";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"このプリセットを削除してもよろしいですか？";
            return english;

        case APP_LANG_KOREAN:
            if (wcscmp(english, L"Set Countdown") == 0) return L"카운트다운";
            if (wcscmp(english, L"Set Time") == 0) return L"카운트다운";
            if (wcscmp(english, L"Time's up!") == 0) return L"시간이 종료되었습니다!";
            if (wcscmp(english, L"Show Current Time") == 0) return L"현재 시간 표시";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24시간 형식";
            if (wcscmp(english, L"Show Seconds") == 0) return L"초 표시";
            if (wcscmp(english, L"Time Display") == 0) return L"시간 표시";
            if (wcscmp(english, L"Count Up") == 0) return L"카운트업";
            if (wcscmp(english, L"Countdown") == 0) return L"카운트다운";
            if (wcscmp(english, L"Start") == 0) return L"시작";
            if (wcscmp(english, L"Pause") == 0) return L"일시정지";
            if (wcscmp(english, L"Resume") == 0) return L"계속";
            if (wcscmp(english, L"Restart") == 0) return L"다시 시작";
            if (wcscmp(english, L"Start Over") == 0) return L"처음부터 다시";
            if (wcscmp(english, L"Edit Mode") == 0) return L"편집 모드";
            if (wcscmp(english, L"Show Message") == 0) return L"메시지 표시";
            if (wcscmp(english, L"Lock Screen") == 0) return L"화면 잠금";
            if (wcscmp(english, L"Shutdown") == 0) return L"종료";
            if (wcscmp(english, L"Timeout Action") == 0) return L"시간 초과 동작";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"시간 옵션 수정";
            if (wcscmp(english, L"Customize") == 0) return L"사용자 지정";
            if (wcscmp(english, L"Color") == 0) return L"색상";
            if (wcscmp(english, L"Font") == 0) return L"글꼴";
            if (wcscmp(english, L"Version: %hs") == 0) return L"버전: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"피드백";
            if (wcscmp(english, L"Check for Updates") == 0) return L"업데이트 확인";
            if (wcscmp(english, L"About") == 0) return L"정보";
            if (wcscmp(english, L"Reset") == 0) return L"재설정";
            if (wcscmp(english, L"Exit") == 0) return L"종료";
            if (wcscmp(english, L"Settings") == 0) return L"설정";
            if (wcscmp(english, L"Preset Manager") == 0) return L"프리셋 관리자";
            if (wcscmp(english, L"Startup Settings") == 0) return L"시작 설정";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Windows와 함께 시작";
            if (wcscmp(english, L"All Pomodoro cycles completed!") == 0) return L"모든 뽀모도로 사이클이 완료되었습니다!";
            if (wcscmp(english, L"Break over! Time to focus again.") == 0) return L"휴식 종료! 다시 집중할 시간입니다.";
            if (wcscmp(english, L"Error") == 0) return L"오류";
            if (wcscmp(english, L"Failed to open file") == 0) return L"파일을 열지 못했습니다";
            if (wcscmp(english, L"Timer Control") == 0) return L"타이머 제어";
            if (wcscmp(english, L"Pomodoro") == 0) return L"포모도로";
            if (wcscmp(english, L"Focus: %ls") == 0) return L"집중: %ls";
            if (wcscmp(english, L"Short Break: %ls") == 0) return L"짧은 휴식: %ls";
            if (wcscmp(english, L"Long Break: %ls") == 0) return L"긴 휴식: %ls";
            if (wcscmp(english, L"Loop Count: %d") == 0) return L"반복 횟수: %d";
            if (wcscmp(english, L"Always on Top") == 0) return L"항상 위에";
            if (wcscmp(english, L"Save Preset") == 0) return L"프리셋 저장";
            if (wcscmp(english, L"Load Preset") == 0) return L"프리셋 불러오기";
            if (wcscmp(english, L"Delete Preset") == 0) return L"프리셋 삭제";
            if (wcscmp(english, L"Create New Preset") == 0) return L"새 프리셋 만들기";
            if (wcscmp(english, L"Preset Name") == 0) return L"프리셋 이름";
            if (wcscmp(english, L"Select Preset") == 0) return L"프리셋 선택";
            if (wcscmp(english, L"Confirm Delete") == 0) return L"삭제 확인";
            if (wcscmp(english, L"Are you sure you want to delete this preset?") == 0) return L"이 프리셋을 삭제하시겠습니까?";
            return english;

        case APP_LANG_ENGLISH:
        default:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Set Countdown";
            if (wcscmp(english, L"Set Time") == 0) return L"Set Countdown";
            return english;
    }
    return english;
}
