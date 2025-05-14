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
 * @brief 定义语言字符串键值对结构
 */
typedef struct {
    const wchar_t* english;  // 英文键
    const wchar_t* translation;  // 翻译值
} LocalizedString;

// 简体中文翻译表
static const LocalizedString CHINESE_SIMP_STRINGS[] = {
    {L"Set Countdown", L"倒计时"},
    {L"Set Time", L"倒计时"},
    {L"Time's up!", L"时间到啦！"},
    {L"Show Current Time", L"显示当前时间"},
    {L"24-Hour Format", L"24小时制"},
    {L"Show Seconds", L"显示秒数"},
    {L"Time Display", L"时间显示"},
    {L"Count Up", L"正计时"},
    {L"Countdown", L"倒计时"},
    {L"Start", L"开始"},
    {L"Pause", L"暂停"},
    {L"Resume", L"继续"},
    {L"Start Over", L"重新开始"},
    {L"Restart", L"重启"},
    {L"Edit Mode", L"编辑模式"},
    {L"Show Message", L"显示消息"},
    {L"Lock Screen", L"锁定屏幕"},
    {L"Shutdown", L"关机"},
    {L"Timeout Action", L"超时动作"},
    {L"Modify Time Options", L"倒计时预设"},
    {L"Customize", L"自定义"},
    {L"Color", L"颜色"},
    {L"Font", L"字体"},
    {L"Version: %hs", L"版本: %hs"},
    {L"Feedback", L"反馈"},
    {L"Check for Updates", L"检查更新"},
    {L"About", L"关于"},
    {L"User Guide", L"使用指南"},
    {L"Reset", L"重置"},
    {L"Exit", L"退出"},
    {L"Settings", L"设置"},
    {L"Preset Manager", L"预设管理"},
    {L"Startup Settings", L"启动设置"},
    {L"Start with Windows", L"开机自启动"},
    {L"All Pomodoro cycles completed!", L"所有番茄钟循环完成！"},
    {L"Break over! Time to focus again.", L"休息结束！重新开始工作！"},
    {L"Error", L"错误"},
    {L"Failed to open file", L"无法打开文件"},
    {L"Timer Control", L"计时管理"},
    {L"Pomodoro", L"番茄时钟"},
    {L"Loop Count: %d", L"循环次数: %d"},
    {L"Always on Top", L"置顶"},
    {L"Save Preset", L"保存预设"},
    {L"Load Preset", L"加载预设"},
    {L"Delete Preset", L"删除预设"},
    {L"Create New Preset", L"创建新预设"},
    {L"Preset Name", L"预设名称"},
    {L"Select Preset", L"选择预设"},
    {L"Confirm Delete", L"确认删除"},
    {L"Are you sure you want to delete this preset?", L"确定要删除这个预设吗？"},
    {L"Open File/Software", L"打开文件/软件"},
    {L"No Display", L"不显示"},
    {L"Preset Management", L"预设管理"},
    {L"Color Value", L"颜色值"},
    {L"Color Panel", L"颜色面板"},
    {L"More", L"更多"},
    {L"Help", L"帮助"},
    {L"Working: %d/%d", L"工作中: %d/%d"},
    {L"Short Break: %d/%d", L"短休息: %d/%d"},
    {L"Long Break", L"长休息"},
    {L"Time to focus!", L"专注时间到了！"},
    {L"Time for a break!", L"休息时间到了！"},
    {L"Completed: %d/%d", L"已完成: %d/%d"},
    {L"Browse...", L"浏览..."},
    {L"Open Website", L"打开网站"},
    {L"Combination", L"组合"},
    {L"Set to No Display on Startup", L"设定为启动时不显示"},
    {L"Set to Stopwatch on Startup", L"设定为启动时正计时"},
    {L"Set to Countdown on Startup", L"设定为启动时倒计时"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"请输入以空格分隔的数字\n范例: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分钟30秒\n25 30m = 25小时30分钟\n1 30 20 = 1小时30分钟20秒", 
     L"25    = 25分钟\n25h   = 25小时\n25s   = 25秒\n25 30 = 25分钟30秒\n25 30m = 25小时30分钟\n1 30 20 = 1小时30分钟20秒"},
    {NULL, NULL}  // 表示结束
};

// 繁体中文翻译表
static const LocalizedString CHINESE_TRAD_STRINGS[] = {
    {L"Set Countdown", L"倒計時"},
    {L"Set Time", L"倒計時"},
    {L"Time's up!", L"時間到了！"},
    {L"Show Current Time", L"顯示目前時間"},
    {L"24-Hour Format", L"24小時制"},
    {L"Show Seconds", L"顯示秒數"},
    {L"Time Display", L"時間顯示"},
    {L"Count Up", L"正計時"},
    {L"Countdown", L"倒計時"},
    {L"Start", L"開始"},
    {L"Pause", L"暫停"},
    {L"Resume", L"繼續"},
    {L"Start Over", L"重新開始"},
    {L"Restart", L"重新啟動"},
    {L"Edit Mode", L"編輯模式"},
    {L"Show Message", L"顯示訊息"},
    {L"Lock Screen", L"鎖定螢幕"},
    {L"Shutdown", L"關機"},
    {L"Timeout Action", L"逾時動作"},
    {L"Modify Time Options", L"倒計時預設"},
    {L"Customize", L"自訂"},
    {L"Color", L"顏色"},
    {L"Font", L"字型"},
    {L"Version: %hs", L"版本: %hs"},
    {L"Feedback", L"意見回饋"},
    {L"Check for Updates", L"檢查更新"},
    {L"About", L"關於"},
    {L"User Guide", L"使用指南"},
    {L"Reset", L"重設"},
    {L"Exit", L"結束"},
    {L"Settings", L"設定"},
    {L"Preset Manager", L"預設管理"},
    {L"Startup Settings", L"啟動設定"},
    {L"Start with Windows", L"開機時啟動"},
    {L"All Pomodoro cycles completed!", L"所有番茄鐘循環已完成！"},
    {L"Break over! Time to focus again.", L"休息結束！該專注了！"},
    {L"Error", L"錯誤"},
    {L"Failed to open file", L"無法開啟檔案"},
    {L"Timer Control", L"計時器控制"},
    {L"Pomodoro", L"番茄鐘"},
    {L"Loop Count: %d", L"循環次數: %d"},
    {L"Always on Top", L"視窗置頂"},
    {L"Save Preset", L"儲存預設"},
    {L"Load Preset", L"載入預設"},
    {L"Delete Preset", L"刪除預設"},
    {L"Create New Preset", L"建立新預設"},
    {L"Preset Name", L"預設名稱"},
    {L"Select Preset", L"選擇預設"},
    {L"Confirm Delete", L"確認刪除"},
    {L"Are you sure you want to delete this preset?", L"確定要刪除此預設嗎？"},
    {L"Open File/Software", L"開啟檔案/軟體"},
    {L"No Display", L"不顯示"},
    {L"Preset Management", L"預設管理"},
    {L"Color Value", L"顏色數值"},
    {L"Color Panel", L"顏色面板"},
    {L"More", L"更多"},
    {L"Help", L"說明"},
    {L"Working: %d/%d", L"工作中: %d/%d"},
    {L"Short Break: %d/%d", L"短休息: %d/%d"},
    {L"Long Break", L"長休息"},
    {L"Time to focus!", L"該專注了！"},
    {L"Time for a break!", L"該休息了！"},
    {L"Completed: %d/%d", L"已完成: %d/%d"},
    {L"Browse...", L"瀏覽..."},
    {L"Open Website", L"開啟網站"},
    {L"Combination", L"組合"},
    {L"Set to No Display on Startup", L"設定為啟動時不顯示"},
    {L"Set to Stopwatch on Startup", L"設定為啟動時正計時"},
    {L"Set to Countdown on Startup", L"設定為啟動時倒計時"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"請輸入以空格分隔的數字\n範例: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分鐘30秒\n25 30m = 25小時30分鐘\n1 30 20 = 1小時30分鐘20秒", 
     L"25    = 25分鐘\n25h   = 25小時\n25s   = 25秒\n25 30 = 25分鐘30秒\n25 30m = 25小時30分鐘\n1 30 20 = 1小時30分鐘20秒"},
    {NULL, NULL}  // 表示结束
};

// 西班牙语翻译表
static const LocalizedString SPANISH_STRINGS[] = {
    {L"Set Countdown", L"Cuenta regresiva"},
    {L"Set Time", L"Establecer tiempo"},
    {L"Time's up!", L"¡Tiempo terminado!"},
    {L"Show Current Time", L"Mostrar hora actual"},
    {L"24-Hour Format", L"Formato 24 horas"},
    {L"Show Seconds", L"Mostrar segundos"},
    {L"Time Display", L"Visualización de tiempo"},
    {L"Count Up", L"Cronómetro"},
    {L"Countdown", L"Cuenta regresiva"},
    {L"Start", L"Iniciar"},
    {L"Pause", L"Pausar"},
    {L"Resume", L"Reanudar"},
    {L"Start Over", L"Comenzar de nuevo"},
    {L"Restart", L"Reiniciar"},
    {L"Edit Mode", L"Modo de edición"},
    {L"Show Message", L"Mostrar mensaje"},
    {L"Lock Screen", L"Bloquear pantalla"},
    {L"Shutdown", L"Apagar"},
    {L"Timeout Action", L"Acción al finalizar"},
    {L"Modify Time Options", L"Preajustes de cuenta regresiva"},
    {L"Customize", L"Personalizar"},
    {L"Color", L"Color"},
    {L"Font", L"Fuente"},
    {L"Version: %hs", L"Versión: %hs"},
    {L"Feedback", L"Comentarios"},
    {L"Check for Updates", L"Buscar actualizaciones"},
    {L"About", L"Acerca de"},
    {L"User Guide", L"Guía de uso"},
    {L"Reset", L"Restablecer"},
    {L"Exit", L"Salir"},
    {L"Settings", L"Configuración"},
    {L"Preset Manager", L"Gestor de ajustes"},
    {L"Startup Settings", L"Configuración de inicio"},
    {L"Start with Windows", L"Iniciar con Windows"},
    {L"All Pomodoro cycles completed!", L"¡Todos los ciclos Pomodoro completados!"},
    {L"Break over! Time to focus again.", L"¡Descanso terminado! ¡Hora de volver a concentrarse!"},
    {L"Error", L"Error"},
    {L"Failed to open file", L"Error al abrir el archivo"},
    {L"Timer Control", L"Control del temporizador"},
    {L"Pomodoro", L"Pomodoro"},
    {L"Loop Count: %d", L"Número de ciclos: %d"},
    {L"Always on Top", L"Siempre visible"},
    {L"Save Preset", L"Guardar ajuste"},
    {L"Load Preset", L"Cargar ajuste"},
    {L"Delete Preset", L"Eliminar ajuste"},
    {L"Create New Preset", L"Crear nuevo ajuste"},
    {L"Preset Name", L"Nombre del ajuste"},
    {L"Select Preset", L"Seleccionar ajuste"},
    {L"Confirm Delete", L"Confirmar eliminación"},
    {L"Are you sure you want to delete this preset?", L"¿Está seguro de que desea eliminar este ajuste?"},
    {L"Open File/Software", L"Abrir archivo/programa"},
    {L"No Display", L"Sin mostrar"},
    {L"Preset Management", L"Gestión de ajustes"},
    {L"Color Value", L"Valor del color"},
    {L"Color Panel", L"Panel de colores"},
    {L"More", L"Más"},
    {L"Help", L"Ayuda"},
    {L"Working: %d/%d", L"Trabajando: %d/%d"},
    {L"Short Break: %d/%d", L"Descanso corto: %d/%d"},
    {L"Long Break", L"Descanso largo"},
    {L"Time to focus!", L"¡Hora de concentrarse!"},
    {L"Time for a break!", L"¡Hora de descansar!"},
    {L"Completed: %d/%d", L"Completado: %d/%d"},
    {L"Browse...", L"Examinar..."},
    {L"Open Website", L"Abrir sitio web"},
    {L"Combination", L"Combinación"},
    {L"Set to No Display on Startup", L"No mostrar al inicio"},
    {L"Set to Stopwatch on Startup", L"Iniciar como cronómetro"},
    {L"Set to Countdown on Startup", L"Iniciar como cuenta regresiva"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"Ingrese números separados por espacios\nEjemplo: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds", 
     L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos"},
    {NULL, NULL}  // 表示结束
};

// 法语翻译表
static const LocalizedString FRENCH_STRINGS[] = {
    {L"Set Countdown", L"Compte à rebours"},
    {L"Set Time", L"Régler le temps"},
    {L"Time's up!", L"Temps écoulé !"},
    {L"Show Current Time", L"Afficher l'heure actuelle"},
    {L"24-Hour Format", L"Format 24 heures"},
    {L"Show Seconds", L"Afficher les secondes"},
    {L"Time Display", L"Affichage du temps"},
    {L"Count Up", L"Chronomètre"},
    {L"Countdown", L"Compte à rebours"},
    {L"Start", L"Démarrer"},
    {L"Pause", L"Pause"},
    {L"Resume", L"Reprendre"},
    {L"Start Over", L"Recommencer"},
    {L"Restart", L"Redémarrer"},
    {L"Edit Mode", L"Mode édition"},
    {L"Show Message", L"Afficher le message"},
    {L"Lock Screen", L"Verrouiller l'écran"},
    {L"Shutdown", L"Éteindre"},
    {L"Timeout Action", L"Action à l'expiration"},
    {L"Modify Time Options", L"Préréglages de minuterie"},
    {L"Customize", L"Personnaliser"},
    {L"Color", L"Couleur"},
    {L"Font", L"Police"},
    {L"Version: %hs", L"Version : %hs"},
    {L"Feedback", L"Commentaires"},
    {L"Check for Updates", L"Vérifier les mises à jour"},
    {L"About", L"À propos"},
    {L"User Guide", L"Guide d'utilisation"},
    {L"Reset", L"Réinitialiser"},
    {L"Exit", L"Quitter"},
    {L"Settings", L"Paramètres"},
    {L"Preset Manager", L"Gestionnaire de préréglages"},
    {L"Startup Settings", L"Paramètres de démarrage"},
    {L"Start with Windows", L"Démarrer avec Windows"},
    {L"All Pomodoro cycles completed!", L"Tous les cycles Pomodoro sont terminés !"},
    {L"Break over! Time to focus again.", L"Pause terminée ! C'est l'heure de se concentrer !"},
    {L"Error", L"Erreur"},
    {L"Failed to open file", L"Échec de l'ouverture du fichier"},
    {L"Timer Control", L"Contrôle du minuteur"},
    {L"Pomodoro", L"Pomodoro"},
    {L"Loop Count: %d", L"Nombre de cycles : %d"},
    {L"Always on Top", L"Toujours au premier plan"},
    {L"Save Preset", L"Enregistrer le préréglage"},
    {L"Load Preset", L"Charger le préréglage"},
    {L"Delete Preset", L"Supprimer le préréglage"},
    {L"Create New Preset", L"Créer un nouveau préréglage"},
    {L"Preset Name", L"Nom du préréglage"},
    {L"Select Preset", L"Sélectionner un préréglage"},
    {L"Confirm Delete", L"Confirmer la suppression"},
    {L"Are you sure you want to delete this preset?", L"Êtes-vous sûr de vouloir supprimer ce préréglage ?"},
    {L"Open File/Software", L"Ouvrir un fichier/logiciel"},
    {L"No Display", L"Aucun affichage"},
    {L"Preset Management", L"Gestion des préréglages"},
    {L"Color Value", L"Valeur de couleur"},
    {L"Color Panel", L"Palette de couleurs"},
    {L"More", L"Plus"},
    {L"Help", L"Aide"},
    {L"Working: %d/%d", L"Travail : %d/%d"},
    {L"Short Break: %d/%d", L"Pause courte : %d/%d"},
    {L"Long Break", L"Pausa longa"},
    {L"Time to focus!", L"Hora de focar!"},
    {L"Time for a break!", L"Hora da pausa!"},
    {L"Completed: %d/%d", L"Concluído: %d/%d"},
    {L"Browse...", L"Parcourir..."},
    {L"Open Website", L"Abrir site"},
    {L"Combination", L"Combinaison"},
    {L"Set to No Display on Startup", L"Não exibir na inicialização"},
    {L"Set to Stopwatch on Startup", L"Iniciar como cronômetro"},
    {L"Set to Countdown on Startup", L"Iniciar como contagem regressiva"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"Digite números separados por espaços\nExemplo: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds", 
     L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos"},
    {NULL, NULL}  // 表示结束
};

// 德语翻译表
static const LocalizedString GERMAN_STRINGS[] = {
    {L"Set Countdown", L"Countdown einstellen"},
    {L"Set Time", L"Zeit einstellen"},
    {L"Time's up!", L"Zeit ist um!"},
    {L"Show Current Time", L"Aktuelle Zeit anzeigen"},
    {L"24-Hour Format", L"24-Stunden-Format"},
    {L"Show Seconds", L"Sekunden anzeigen"},
    {L"Time Display", L"Zeitanzeige"},
    {L"Count Up", L"Stoppuhr"},
    {L"Countdown", L"Countdown"},
    {L"Start", L"Start"},
    {L"Pause", L"Pause"},
    {L"Resume", L"Fortsetzen"},
    {L"Start Over", L"Neu starten"},
    {L"Restart", L"Neustart"},
    {L"Edit Mode", L"Bearbeitungsmodus"},
    {L"Show Message", L"Nachricht anzeigen"},
    {L"Lock Screen", L"Bildschirm sperren"},
    {L"Shutdown", L"Herunterfahren"},
    {L"Timeout Action", L"Zeitüberschreitungsaktion"},
    {L"Modify Time Options", L"Countdown-Voreinstellungen"},
    {L"Customize", L"Anpassen"},
    {L"Color", L"Farbe"},
    {L"Font", L"Schriftart"},
    {L"Version: %hs", L"Version: %hs"},
    {L"Feedback", L"Feedback"},
    {L"Check for Updates", L"Nach Updates suchen"},
    {L"About", L"Über"},
    {L"User Guide", L"Benutzerhandbuch"},
    {L"Reset", L"Zurücksetzen"},
    {L"Exit", L"Beenden"},
    {L"Settings", L"Einstellungen"},
    {L"Preset Manager", L"Voreinstellungen verwalten"},
    {L"Startup Settings", L"Starteinstellungen"},
    {L"Start with Windows", L"Mit Windows starten"},
    {L"All Pomodoro cycles completed!", L"Alle Pomodoro-Zyklen abgeschlossen!"},
    {L"Break over! Time to focus again.", L"Pause vorbei! Zeit, sich wieder zu konzentrieren!"},
    {L"Error", L"Fehler"},
    {L"Failed to open file", L"Datei konnte nicht geöffnet werden"},
    {L"Timer Control", L"Timer-Steuerung"},
    {L"Pomodoro", L"Pomodoro"},
    {L"Loop Count: %d", L"Anzahl der Zyklen: %d"},
    {L"Always on Top", L"Immer im Vordergrund"},
    {L"Save Preset", L"Voreinstellung speichern"},
    {L"Load Preset", L"Voreinstellung laden"},
    {L"Delete Preset", L"Voreinstellung löschen"},
    {L"Create New Preset", L"Neue Voreinstellung erstellen"},
    {L"Preset Name", L"Name der Voreinstellung"},
    {L"Select Preset", L"Voreinstellung auswählen"},
    {L"Confirm Delete", L"Löschen bestätigen"},
    {L"Are you sure you want to delete this preset?", L"Möchten Sie diese Voreinstellung wirklich löschen?"},
    {L"Open File/Software", L"Datei/Programm öffnen"},
    {L"No Display", L"Keine Anzeige"},
    {L"Preset Management", L"Voreinstellungsverwaltung"},
    {L"Color Value", L"Farbwert"},
    {L"Color Panel", L"Farbauswahl"},
    {L"More", L"Mehr"},
    {L"Help", L"Hilfe"},
    {L"Working: %d/%d", L"Arbeitsphase: %d/%d"},
    {L"Short Break: %d/%d", L"Kurze Pause: %d/%d"},
    {L"Long Break", L"Lange Pause"},
    {L"Time to focus!", L"Zeit zum Konzentrieren!"},
    {L"Time for a break!", L"Zeit für eine Pause!"},
    {L"Completed: %d/%d", L"Abgeschlossen: %d/%d"},
    {L"Browse...", L"Durchsuchen..."},
    {L"Open Website", L"Webseite öffnen"},
    {L"Combination", L"Kombination"},
    {L"Set to No Display on Startup", L"Beim Start nicht anzeigen"},
    {L"Set to Stopwatch on Startup", L"Beim Start als Stoppuhr starten"},
    {L"Set to Countdown on Startup", L"Beim Start als Countdown starten"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"Geben Sie Zahlen durch Leerzeichen getrennt ein\nBeispiel: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds", 
     L"25    = 25 Minuten\n25h   = 25 Stunden\n25s   = 25 Sekunden\n25 30 = 25 Minuten 30 Sekunden\n25 30m = 25 Stunden 30 Minuten\n1 30 20 = 1 Stunde 30 Minuten 20 Sekunden"},
    {NULL, NULL}  // 表示结束
};

// 俄语翻译表
static const LocalizedString RUSSIAN_STRINGS[] = {
    {L"Set Countdown", L"Обратный отсчёт"},
    {L"Set Time", L"Установить время"},
    {L"Time's up!", L"Время истекло!"},
    {L"Show Current Time", L"Показать текущее время"},
    {L"24-Hour Format", L"24-часовой формат"},
    {L"Show Seconds", L"Показывать секунды"},
    {L"Time Display", L"Отображение времени"},
    {L"Count Up", L"Секундомер"},
    {L"Countdown", L"Обратный отсчёт"},
    {L"Start", L"Старт"},
    {L"Pause", L"Пауза"},
    {L"Resume", L"Продолжить"},
    {L"Start Over", L"Начать заново"},
    {L"Restart", L"Перезапустить"},
    {L"Edit Mode", L"Режим редактирования"},
    {L"Show Message", L"Показать сообщение"},
    {L"Lock Screen", L"Заблокировать экран"},
    {L"Shutdown", L"Выключить компьютер"},
    {L"Timeout Action", L"Действие по истечении времени"},
    {L"Modify Time Options", L"Пресеты таймера"},
    {L"Customize", L"Настроить"},
    {L"Color", L"Цвет"},
    {L"Font", L"Шрифт"},
    {L"Version: %hs", L"Версия: %hs"},
    {L"Feedback", L"Обратная связь"},
    {L"Check for Updates", L"Проверить обновления"},
    {L"About", L"О программе"},
    {L"User Guide", L"Пользовательское руководство"},
    {L"Reset", L"Сбросить"},
    {L"Exit", L"Выход"},
    {L"Settings", L"Настройки"},
    {L"Preset Manager", L"Управление пресетами"},
    {L"Startup Settings", L"Настройки запуска"},
    {L"Start with Windows", L"Запускать с Windows"},
    {L"All Pomodoro cycles completed!", L"Все циклы Помодоро завершены!"},
    {L"Break over! Time to focus again.", L"Перерыв окончен! Время снова сосредоточиться!"},
    {L"Error", L"Ошибка"},
    {L"Failed to open file", L"Не удалось открыть файл"},
    {L"Timer Control", L"Управление таймером"},
    {L"Pomodoro", L"Помодоро"},
    {L"Loop Count: %d", L"Количество циклов: %d"},
    {L"Always on Top", L"Поверх всех окон"},
    {L"Save Preset", L"Сохранить пресет"},
    {L"Load Preset", L"Загрузить пресет"},
    {L"Delete Preset", L"Удалить пресет"},
    {L"Create New Preset", L"Создать новый пресет"},
    {L"Preset Name", L"Название пресета"},
    {L"Select Preset", L"Выбрать пресет"},
    {L"Confirm Delete", L"Подтвердить удаление"},
    {L"Are you sure you want to delete this preset?", L"Вы уверены, что хотите удалить этот пресет?"},
    {L"Open File/Software", L"Открыть файл/программу"},
    {L"No Display", L"Не отображать"},
    {L"Preset Management", L"Управление пресетами"},
    {L"Color Value", L"Значение цвета"},
    {L"Color Panel", L"Палитра цветов"},
    {L"More", L"Ещё"},
    {L"Help", L"Справка"},
    {L"Working: %d/%d", L"Работа: %d/%d"},
    {L"Short Break: %d/%d", L"Короткий перерыв: %d/%d"},
    {L"Long Break", L"Длинный перерыв"},
    {L"Time to focus!", L"Время сосредоточиться!"},
    {L"Time for a break!", L"Время для перерыва!"},
    {L"Completed: %d/%d", L"Завершено: %d/%d"},
    {L"Browse...", L"Обзор..."},
    {L"Open Website", L"Открыть сайт"},
    {L"Combination", L"Комбинация"},
    {L"Set to No Display on Startup", L"Не показывать при запуске"},
    {L"Set to Stopwatch on Startup", L"Запускать как секундомер"},
    {L"Set to Countdown on Startup", L"Запускать как обратный отсчёт"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"Введите числа через пробел\nПример: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds", 
     L"25    = 25 минут\n25h   = 25 часов\n25s   = 25 секунд\n25 30 = 25 минут 30 секунд\n25 30m = 25 часов 30 минут\n1 30 20 = 1 час 30 минут 20 секунд"},
    {NULL, NULL}  // 表示结束
};

// 葡萄牙语翻译表
static const LocalizedString PORTUGUESE_STRINGS[] = {
    {L"Set Countdown", L"Contagem regressiva"},
    {L"Set Time", L"Definir tempo"},
    {L"Time's up!", L"Tempo esgotado!"},
    {L"Show Current Time", L"Mostrar hora atual"},
    {L"24-Hour Format", L"Formato 24 horas"},
    {L"Show Seconds", L"Mostrar segundos"},
    {L"Time Display", L"Exibição de tempo"},
    {L"Count Up", L"Cronômetro"},
    {L"Countdown", L"Contagem regressiva"},
    {L"Start", L"Iniciar"},
    {L"Pause", L"Pausar"},
    {L"Resume", L"Retomar"},
    {L"Start Over", L"Recomeçar"},
    {L"Restart", L"Reiniciar"},
    {L"Edit Mode", L"Modo de edição"},
    {L"Show Message", L"Mostrar mensagem"},
    {L"Lock Screen", L"Bloquear tela"},
    {L"Shutdown", L"Desligar"},
    {L"Timeout Action", L"Ação ao finalizar"},
    {L"Modify Time Options", L"Predefinições de timer"},
    {L"Customize", L"Personalizar"},
    {L"Color", L"Cor"},
    {L"Font", L"Fonte"},
    {L"Version: %hs", L"Versão: %hs"},
    {L"Feedback", L"Feedback"},
    {L"Check for Updates", L"Verificar atualizações"},
    {L"About", L"Sobre"},
    {L"User Guide", L"Guia de uso"},
    {L"Reset", L"Redefinir"},
    {L"Exit", L"Sair"},
    {L"Settings", L"Configurações"},
    {L"Preset Manager", L"Gerenciador de predefinições"},
    {L"Startup Settings", L"Configurações de inicialização"},
    {L"Start with Windows", L"Iniciar com o Windows"},
    {L"All Pomodoro cycles completed!", L"Todos os ciclos Pomodoro concluídos!"},
    {L"Break over! Time to focus again.", L"Intervalo terminado! Hora de focar novamente!"},
    {L"Error", L"Erro"},
    {L"Failed to open file", L"Falha ao abrir arquivo"},
    {L"Timer Control", L"Controle do temporizador"},
    {L"Pomodoro", L"Pomodoro"},
    {L"Loop Count: %d", L"Número de ciclos: %d"},
    {L"Always on Top", L"Sempre visível"},
    {L"Save Preset", L"Salvar predefinição"},
    {L"Load Preset", L"Carregar predefinição"},
    {L"Delete Preset", L"Excluir predefinição"},
    {L"Create New Preset", L"Criar nova predefinição"},
    {L"Preset Name", L"Nome da predefinição"},
    {L"Select Preset", L"Selecionar predefinição"},
    {L"Confirm Delete", L"Confirmar exclusão"},
    {L"Are you sure you want to delete this preset?", L"Tem certeza que deseja excluir esta predefinição?"},
    {L"Open File/Software", L"Abrir arquivo/programa"},
    {L"No Display", L"Não exibir"},
    {L"Preset Management", L"Gerenciamento de predefinições"},
    {L"Color Value", L"Valor da cor"},
    {L"Color Panel", L"Painel de cores"},
    {L"More", L"Mais"},
    {L"Help", L"Ajuda"},
    {L"Working: %d/%d", L"Trabalhando: %d/%d"},
    {L"Short Break: %d/%d", L"Pausa curta: %d/%d"},
    {L"Long Break", L"Pausa longa"},
    {L"Time to focus!", L"Hora de focar!"},
    {L"Time for a break!", L"Hora da pausa!"},
    {L"Completed: %d/%d", L"Concluído: %d/%d"},
    {L"Browse...", L"Procurar..."},
    {L"Open Website", L"Abrir site"},
    {L"Combination", L"Combinação"},
    {L"Set to No Display on Startup", L"Não exibir na inicialização"},
    {L"Set to Stopwatch on Startup", L"Iniciar como cronômetro"},
    {L"Set to Countdown on Startup", L"Iniciar como contagem regressiva"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"Digite números separados por espaços\nExemplo: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds", 
     L"25    = 25 minutos\n25h   = 25 horas\n25s   = 25 segundos\n25 30 = 25 minutos 30 segundos\n25 30m = 25 horas 30 minutos\n1 30 20 = 1 hora 30 minutos 20 segundos"},
    {NULL, NULL}  // 表示结束
};

// 日语翻译表
static const LocalizedString JAPANESE_STRINGS[] = {
    {L"Set Countdown", L"カウントダウン設定"},
    {L"Set Time", L"時間設定"},
    {L"Time's up!", L"時間になりました！"},
    {L"Show Current Time", L"現在時刻を表示"},
    {L"24-Hour Format", L"24時間表示"},
    {L"Show Seconds", L"秒を表示"},
    {L"Time Display", L"時間表示"},
    {L"Count Up", L"ストップウォッチ"},
    {L"Countdown", L"カウントダウン"},
    {L"Start", L"開始"},
    {L"Pause", L"一時停止"},
    {L"Resume", L"再開"},
    {L"Start Over", L"最初からやり直す"},
    {L"Restart", L"再起動"},
    {L"Edit Mode", L"編集モード"},
    {L"Show Message", L"メッセージを表示"},
    {L"Lock Screen", L"画面をロック"},
    {L"Shutdown", L"シャットダウン"},
    {L"Timeout Action", L"タイムアウト時の動作"},
    {L"Modify Time Options", L"タイマープリセット"},
    {L"Customize", L"カスタマイズ"},
    {L"Color", L"色"},
    {L"Font", L"フォント"},
    {L"Version: %hs", L"バージョン: %hs"},
    {L"Feedback", L"フィードバック"},
    {L"Check for Updates", L"アップデートを確認"},
    {L"About", L"このアプリについて"},
    {L"User Guide", L"使用ガイド"},
    {L"Reset", L"リセット"},
    {L"Exit", L"終了"},
    {L"Settings", L"設定"},
    {L"Preset Manager", L"プリセット管理"},
    {L"Startup Settings", L"起動設定"},
    {L"Start with Windows", L"Windowsの起動時に実行"},
    {L"All Pomodoro cycles completed!", L"全てのポモドーロサイクルが完了しました！"},
    {L"Break over! Time to focus again.", L"休憩終了！集中タイムの開始です！"},
    {L"Error", L"エラー"},
    {L"Failed to open file", L"ファイルを開けませんでした"},
    {L"Timer Control", L"タイマー制御"},
    {L"Pomodoro", L"ポモドーロ"},
    {L"Loop Count: %d", L"繰り返し回数: %d"},
    {L"Always on Top", L"常に最前面に表示"},
    {L"Save Preset", L"プリセットを保存"},
    {L"Load Preset", L"プリセットを読み込む"},
    {L"Delete Preset", L"プリセットを削除"},
    {L"Create New Preset", L"新規プリセットを作成"},
    {L"Preset Name", L"プリセット名"},
    {L"Select Preset", L"プリセットを選択"},
    {L"Confirm Delete", L"削除の確認"},
    {L"Are you sure you want to delete this preset?", L"このプリセットを削除してもよろしいですか？"},
    {L"Open File/Software", L"ファイル/ソフトウェアを開く"},
    {L"No Display", L"非表示"},
    {L"Preset Management", L"プリセット管理"},
    {L"Color Value", L"色の値"},
    {L"Color Panel", L"カラーパネル"},
    {L"More", L"その他"},
    {L"Help", L"ヘルプ"},
    {L"Working: %d/%d", L"作業中: %d/%d"},
    {L"Short Break: %d/%d", L"小休憩: %d/%d"},
    {L"Long Break", L"長休憩"},
    {L"Time to focus!", L"集中タイムの開始です！"},
    {L"Time for a break!", L"休憩時間です！"},
    {L"Completed: %d/%d", L"完了: %d/%d"},
    {L"Browse...", L"参照..."},
    {L"Open Website", L"ウェブサイトを開く"},
    {L"Combination", L"コンビネーション"},
    {L"Set to No Display on Startup", L"起動時に非表示で開始"},
    {L"Set to Stopwatch on Startup", L"起動時にストップウォッチで開始"},
    {L"Set to Countdown on Startup", L"起動時にカウントダウンで開始"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"数字をスペースで区切って入力してください\n例: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds", 
     L"25    = 25分\n25h   = 25時間\n25s   = 25秒\n25 30 = 25分30秒\n25 30m = 25時間30分\n1 30 20 = 1時間30分20秒"},
    {NULL, NULL}  // 表示结束
};

// 韩语翻译表
static const LocalizedString KOREAN_STRINGS[] = {
    {L"Set Countdown", L"타이머 설정"},
    {L"Set Time", L"시간 설정"},
    {L"Time's up!", L"시간이 다 되었습니다!"},
    {L"Show Current Time", L"현재 시간 표시"},
    {L"24-Hour Format", L"24시간 형식"},
    {L"Show Seconds", L"초 표시"},
    {L"Time Display", L"시간 표시"},
    {L"Count Up", L"스톱워치"},
    {L"Countdown", L"타이머"},
    {L"Start", L"시작"},
    {L"Pause", L"일시정지"},
    {L"Resume", L"계속하기"},
    {L"Start Over", L"처음부터 시작"},
    {L"Restart", L"다시 시작"},
    {L"Edit Mode", L"편집 모드"},
    {L"Show Message", L"메시지 표시"},
    {L"Lock Screen", L"화면 잠금"},
    {L"Shutdown", L"시스템 종료"},
    {L"Timeout Action", L"시간 종료 시 동작"},
    {L"Modify Time Options", L"타이머 프리셋"},
    {L"Customize", L"사용자 지정"},
    {L"Color", L"색상"},
    {L"Font", L"글꼴"},
    {L"Version: %hs", L"버전: %hs"},
    {L"Feedback", L"피드백"},
    {L"Check for Updates", L"업데이트 확인"},
    {L"About", L"프로그램 정보"},
    {L"User Guide", L"사용 설명서"},
    {L"Reset", L"초기화"},
    {L"Exit", L"종료"},
    {L"Settings", L"설정"},
    {L"Preset Manager", L"프리셋 관리"},
    {L"Startup Settings", L"시작 설정"},
    {L"Start with Windows", L"Windows 시작 시 자동 실행"},
    {L"All Pomodoro cycles completed!", L"모든 뽀모도로 사이클이 완료되었습니다!"},
    {L"Break over! Time to focus again.", L"휴식 종료! 다시 집중할 시간입니다!"},
    {L"Error", L"오류"},
    {L"Failed to open file", L"파일을 열 수 없습니다"},
    {L"Timer Control", L"타이머 제어"},
    {L"Pomodoro", L"뽀모도로"},
    {L"Loop Count: %d", L"반복 횟수: %d"},
    {L"Always on Top", L"항상 위에 표시"},
    {L"Save Preset", L"프리셋 저장"},
    {L"Load Preset", L"프리셋 불러오기"},
    {L"Delete Preset", L"프리셋 삭제"},
    {L"Create New Preset", L"새 프리셋 만들기"},
    {L"Preset Name", L"프리셋 이름"},
    {L"Select Preset", L"프리셋 선택"},
    {L"Confirm Delete", L"삭제 확인"},
    {L"Are you sure you want to delete this preset?", L"이 프리셋을 삭제하시겠습니까?"},
    {L"Open File/Software", L"파일 열기/프로그램"},
    {L"No Display", L"표시하지 않음"},
    {L"Preset Management", L"프리셋 관리"},
    {L"Color Value", L"색상 값"},
    {L"Color Panel", L"색상 패널"},
    {L"More", L"더 보기"},
    {L"Help", L"도움말"},
    {L"Working: %d/%d", L"작업 중: %d/%d"},
    {L"Short Break: %d/%d", L"짧은 휴식: %d/%d"},
    {L"Long Break", L"긴 휴식"},
    {L"Time to focus!", L"집중할 시간입니다!"},
    {L"Time for a break!", L"휴식 시간입니다!"},
    {L"Completed: %d/%d", L"완료: %d/%d"},
    {L"Browse...", L"찾아보기..."},
    {L"Open Website", L"웹사이트 열기"},
    {L"Combination", L"조합"},
    {L"Set to No Display on Startup", L"시작 시 표시하지 않음"},
    {L"Set to Stopwatch on Startup", L"시작 시 스톱워치로 실행"},
    {L"Set to Countdown on Startup", L"시작 시 타이머로 실행"},
    {L"Enter numbers separated by spaces\nExample: 25 10 5", L"숫자를 공백으로 구분하여 입력하세요\n예시: 25 10 5"},
    {L"25    = 25 minutes\n25h   = 25 hours\n25s   = 25 seconds\n25 30 = 25 minutes 30 seconds\n25 30m = 25 hours 30 minutes\n1 30 20 = 1 hour 30 minutes 20 seconds", 
     L"25    = 25분\n25h   = 25시간\n25s   = 25초\n25 30 = 25분 30초\n25 30m = 25시간 30분\n1 30 20 = 1시간 30분 20초"},
    {NULL, NULL}  // 表示结束
};

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
 * @brief 在指定语言的翻译表中查找对应翻译
 * 
 * @param strings 翻译表数组
 * @param english 英文原文
 * @return const wchar_t* 找到的翻译，如果未找到则返回NULL
 */
static const wchar_t* FindTranslation(const LocalizedString* strings, const wchar_t* english) {
    for (int i = 0; strings[i].english != NULL; i++) {
        if (wcscmp(english, strings[i].english) == 0) {
            return strings[i].translation;
        }
    }
    return NULL;
}

/**
 * @brief 获取本地化字符串
 * @param chinese 简体中文版本的字符串
 * @param english 英语版本的字符串
 * @return const wchar_t* 当前语言对应的字符串指针
 * 
 * 根据当前语言设置返回对应语言的字符串。
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    // 首次调用时自动检测系统语言
    static BOOL initialized = FALSE;
    if (!initialized) {
        DetectSystemLanguage();
        initialized = TRUE;
    }

    const wchar_t* translation = NULL;

    // 根据当前语言返回对应字符串
    switch (CURRENT_LANGUAGE) {
        case APP_LANG_CHINESE_SIMP:
            translation = FindTranslation(CHINESE_SIMP_STRINGS, english);
            if (translation) return translation;
            return chinese;
            
        case APP_LANG_CHINESE_TRAD:
            translation = FindTranslation(CHINESE_TRAD_STRINGS, english);
            if (translation) return translation;
            return chinese;

        case APP_LANG_SPANISH:
            translation = FindTranslation(SPANISH_STRINGS, english);
            if (translation) return translation;
            break;

        case APP_LANG_FRENCH:
            translation = FindTranslation(FRENCH_STRINGS, english);
            if (translation) return translation;
            break;

        case APP_LANG_GERMAN:
            translation = FindTranslation(GERMAN_STRINGS, english);
            if (translation) return translation;
            break;

        case APP_LANG_RUSSIAN:
            translation = FindTranslation(RUSSIAN_STRINGS, english);
            if (translation) return translation;
            break;

        case APP_LANG_PORTUGUESE:
            translation = FindTranslation(PORTUGUESE_STRINGS, english);
            if (translation) return translation;
            break;

        case APP_LANG_JAPANESE:
            translation = FindTranslation(JAPANESE_STRINGS, english);
            if (translation) return translation;
            break;

        case APP_LANG_KOREAN:
            translation = FindTranslation(KOREAN_STRINGS, english);
            if (translation) return translation;
            break;
    }

    // 默认返回英文
    return english;
}
