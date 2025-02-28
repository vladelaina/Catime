#include <windows.h>
#include <wchar.h>
#include "../include/language.h"

AppLanguage CURRENT_LANGUAGE = APP_LANG_CHINESE_SIMP;

const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    switch (CURRENT_LANGUAGE) {
        case APP_LANG_CHINESE_SIMP:
            if (wcscmp(english, L"Time's up!") == 0) return L"时间到啦！";
            if (wcscmp(english, L"Input Format") == 0) return L"输入格式";
            if (wcscmp(english, L"Invalid Input") == 0) return L"无效输入";
            if (wcscmp(english, L"Error") == 0) return L"错误";
            if (wcscmp(english, L"Failed to load font: %hs") == 0) return L"无法加载字体: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25分钟\n25h   = 25小时\n25s   = 25秒\n25 30 = 25分钟30秒\n25 30m = 25小时30分钟\n1 30 20 = 1小时30分钟20秒";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"请输入用空格分隔的数字\n例如: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"关于";
            if (wcscmp(english, L"Version: %hs") == 0) return L"版本: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"检查更新";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"已设置为启动时不显示";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"已设置为启动时正计时";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"已设置为启动时倒计时";
            if (wcscmp(english, L"Settings") == 0) return L"设置";
            if (wcscmp(english, L"Preset Manager") == 0) return L"预设管理";
            if (wcscmp(english, L"Count Up") == 0) return L"正计时";
            if (wcscmp(english, L"Startup Settings") == 0) return L"启动设置";
            if (wcscmp(english, L"Start with Windows") == 0) return L"开机自启动";
            return chinese;
            
        case APP_LANG_CHINESE_TRAD:
            if (wcscmp(english, L"Time's up!") == 0) return L"時間到啦！";
            if (wcscmp(english, L"Input Format") == 0) return L"輸入格式";
            if (wcscmp(english, L"Invalid Input") == 0) return L"無效輸入";
            if (wcscmp(english, L"Error") == 0) return L"錯誤";
            if (wcscmp(english, L"Failed to load font: %hs") == 0) return L"無法加載字體: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25分鐘\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分鐘30秒\n25 30m = 25小時30分鐘\n1 30 20 = 1小時30分鐘20秒";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"請輸入用空格分隔的數字\n例如: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"關於";
            if (wcscmp(english, L"Version: %hs") == 0) return L"版本: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"檢查更新";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"已設置為啟動時不顯示";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"已設置為啟動時正計時";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"已設置為啟動時倒計時";
            if (wcscmp(english, L"Settings") == 0) return L"設置";
            if (wcscmp(english, L"Preset Manager") == 0) return L"預設管理";
            if (wcscmp(english, L"Count Up") == 0) return L"正計時";
            if (wcscmp(english, L"Startup Settings") == 0) return L"啟動設置";
            if (wcscmp(english, L"Start with Windows") == 0) return L"開機自啟動";
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
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Configuré pour ne pas afficher au démarrage";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Configuré comme chronomètre au démarrage";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Configuré comme compte à rebours au démarrage";
            if (wcscmp(english, L"Settings") == 0) return L"Paramètres";
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
            return english;

        case APP_LANG_RUSSIAN:
            if (wcscmp(english, L"Set Time") == 0) return L"Установить время";
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
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hora 30 minutos 20 segundos") == 0)
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
            return english;

        case APP_LANG_JAPANESE:
            if (wcscmp(english, L"Set Time") == 0) return L"時間設定";
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
            return english;

        case APP_LANG_KOREAN:
            if (wcscmp(english, L"Set Time") == 0) return L"시간 설정";
            if (wcscmp(english, L"Edit Mode") == 0) return L"편집 모드";
            if (wcscmp(english, L"Show Current Time") == 0) return L"현재 시간 표시";
            if (wcscmp(english, L"24-Hour Format") == 0) return L"24시간 형식";
            if (wcscmp(english, L"Show Seconds") == 0) return L"초 표시";
            if (wcscmp(english, L"Time Display") == 0) return L"시간 표시";
            if (wcscmp(english, L"Timeout Action") == 0) return L"시간 초과 동작";
            if (wcscmp(english, L"Show Message") == 0) return L"메시지 표시";
            if (wcscmp(english, L"Browse...") == 0) return L"찾아보기...";
            if (wcscmp(english, L"Open File") == 0) return L"파일 열기";
            if (wcscmp(english, L"Open: %hs") == 0) return L"열기: %hs";
            if (wcscmp(english, L"Lock Screen") == 0) return L"화면 잠금";
            if (wcscmp(english, L"Shutdown") == 0) return L"시스템 종료";
            if (wcscmp(english, L"Restart") == 0) return L"다시 시작";
            if (wcscmp(english, L"Modify Time Options") == 0) return L"시간 옵션 수정";
            if (wcscmp(english, L"Customize") == 0) return L"사용자 지정";
            if (wcscmp(english, L"Color") == 0) return L"색상";
            if (wcscmp(english, L"Font") == 0) return L"글꼴";
            if (wcscmp(english, L"Version: %hs") == 0) return L"버전: %hs";
            if (wcscmp(english, L"Feedback") == 0) return L"피드백";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Check for Updates") == 0) return L"업데이트 확인";
            if (wcscmp(english, L"About") == 0) return L"정보";
            if (wcscmp(english, L"Reset") == 0) return english;
            if (wcscmp(english, L"Exit") == 0) return L"종료";
            if (wcscmp(english, L"시간이 종료되었습니다!") == 0) return L"시간이 종료되었습니다!";
            if (wcscmp(english, L"입력 형식") == 0) return L"입력 형식";
            if (wcscmp(english, L"잘못된 입력") == 0) return L"잘못된 입력";
            if (wcscmp(english, L"오류") == 0) return L"오류";
            if (wcscmp(english, L"글꼴을 불러올 수 없습니다: %hs") == 0) return L"글꼴을 불러올 수 없습니다: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25분\n25h   = 25시간\n25s   = 25초\n25 30 = 25분30초\n25 30m = 25시간30분\n1 30 20 = 1시간30분20초";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"한국어로 입력해주세요\n예: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"정보";
            if (wcscmp(english, L"Version: %hs") == 0) return L"버전: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"업데이트 확인";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"시작 시 표시하지 않도록 설정";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"시작 시 스톱워치로 설정";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"시작 시 카운트다운으로 설정";
            if (wcscmp(english, L"Settings") == 0) return L"설정";
            if (wcscmp(english, L"Preset Manager") == 0) return L"프리셋 관리";
            if (wcscmp(english, L"Count Up") == 0) return L"카운트업";
            if (wcscmp(english, L"Startup Settings") == 0) return L"시작 설정";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Windows와 동시에 시작";
            return english;

        case APP_LANG_ENGLISH:
        default:
            if (wcscmp(english, L"Time's up!") == 0) return L"Time's up!";
            if (wcscmp(english, L"Input Format") == 0) return L"Input Format";
            if (wcscmp(english, L"Invalid Input") == 0) return L"Invalid Input";
            if (wcscmp(english, L"Error") == 0) return L"Error";
            if (wcscmp(english, L"Failed to load font: %hs") == 0) return L"Failed to load font: %hs";
            if (wcscmp(english, L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds") == 0)
                return L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds";
            if (wcscmp(english, L"Enter numbers separated by spaces\nExample: 25 10 5") == 0)
                return L"Enter numbers separated by spaces\nExample: 25 10 5";
            if (wcscmp(english, L"Invalid Color Format") == 0) return L"";
            if (wcscmp(english, L"About") == 0) return L"About";
            if (wcscmp(english, L"Version: %hs") == 0) return L"Version: %hs";
            if (wcscmp(english, L"Check for Updates") == 0) return L"Check for Updates";
            if (wcscmp(english, L"Language") == 0) return english;
            if (wcscmp(english, L"Reset") == 0) return L"Reset";
            if (wcscmp(english, L"Set to No Display on Startup") == 0) return L"Set to No Display on Startup";
            if (wcscmp(english, L"Set to Stopwatch on Startup") == 0) return L"Set to Stopwatch on Startup";
            if (wcscmp(english, L"Set to Countdown on Startup") == 0) return L"Set to Countdown on Startup";
            if (wcscmp(english, L"Settings") == 0) return L"Settings";
            if (wcscmp(english, L"Preset Manager") == 0) return L"Preset Manager";
            if (wcscmp(english, L"Count Up") == 0) return L"Count Up";
            if (wcscmp(english, L"Startup Settings") == 0) return L"Startup Settings";
            if (wcscmp(english, L"Start with Windows") == 0) return L"Start with Windows";
            return english;
    }
    return english;
}

