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
            if (wcscmp(english, L"Restart") == 0) return L"重新开始";
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
            if (wcscmp(english, L"Restart") == 0) return L"重新開始";
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
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gestor de preajustes";
            if (wcscmp(english, L"Count Up") == 0) return L"Contar hacia arriba";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Configuración de inicio";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Iniciar con Windows";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Cuenta regresiva";
            if (wcscmp(english, L"Set Time") == 0) return L"Cuenta regresiva";
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
            if (wcscmp(english, L"Format d'entrée") == 0) return L"Format d'entrée";
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
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gestionnaire de préréglages";
            if (wcscmp(english, L"Count Up") == 0) return L"Compte à rebours positif";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Paramètres de démarrage";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Démarrer avec Windows";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Compte à rebours";
            if (wcscmp(english, L"Set Time") == 0) return L"Compte à rebours";
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
            if (wcscmp(english, L"Preset Manager") == 0) return L"Voreinstellungen";
            if (wcscmp(english, L"Count Up") == 0) return L"Aufwärtszählen";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Starteinstellungen";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Mit Windows starten";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Countdown";
            if (wcscmp(english, L"Set Time") == 0) return L"Countdown";
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
            if (wcscmp(english, L"Preset Manager") == 0) return L"Настроить";
            if (wcscmp(english, L"Count Up") == 0) return L"Счетчик";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Настройки запуска";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Запускать с Windows";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Установить обратный отсчет";
            if (wcscmp(english, L"Set Time") == 0) return L"Установить обратный отсчет";
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
            if (wcscmp(english, L"Preset Manager") == 0) return L"Gerenciador de pré-ajustes";
            if (wcscmp(english, L"Count Up") == 0) return L"Contagem ascendente";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Configurações de inicialização";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Iniciar com o Windows";
            if (wcscmp(english, L"Set Countdown") == 0) return L"Establecer cuenta regresiva";
            if (wcscmp(english, L"Set Time") == 0) return L"Establecer cuenta regresiva";
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
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"更新を確認";
            if (wcscmp(english, L"About") == 0) return L"について";
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Exit") == 0) return L"終了";
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
            if (wcscmp(english, L"Preset Manager") == 0) return L"プリセット管理";
            if (wcscmp(english, L"Count Up") == 0) return L"カウントアップ";
            if (wcscmp(english, L"Startup Settings") == 0) return L"起動設定";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Windowsと同時に起動";
            if (wcscmp(english, L"Set Countdown") == 0) return L"カウントダウン";
            if (wcscmp(english, L"Set Time") == 0) return L"カウントダウン";
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
            return english;

        case APP_LANG_ENGLISH:
        default:
            if (wcscmp(english, L"Set Countdown") == 0) return L"Set Countdown";
            if (wcscmp(english, L"Set Time") == 0) return L"Set Countdown";
            return english;
    }
    return english;
}
