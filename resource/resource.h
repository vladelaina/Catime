#define CATIME_VERSION "1.0.3.1"  
#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_MEDIA_STOP 0xB2
#define KEYEVENTF_KEYUP 0x0002

#define UNICODE
#define _UNICODE

#define UPDATE_URL_GITHUB    "https://github.com/vladelaina/Catime/releases"
#define UPDATE_URL_123PAN    "https://www.123684.com/s/ruESjv-2CZUA"
#define UPDATE_URL_LANZOU    "https://wwrx.lanzoup.com/b00hqiiskj"
#define FEEDBACK_URL        "https://www.bilibili.com/video/BV1ztFeeQEYP"
#define FEEDBACK_URL_GITHUB  "https://github.com/vladelaina/Catime/issues"
#define FEEDBACK_URL_BILIBILI "https://message.bilibili.com/#/whisper/mid1862395225"
#define IDI_CATIME 101 

// Font Menu IDs
#define CLOCK_IDC_FONT_VICTORMONO        344  // Font 选项: VictorMono NFP Medium.ttf
#define CLOCK_IDC_FONT_LIBERATION        338  // Font 选项: LiterationSerif Nerd Font.ttf
#define CLOCK_IDC_FONT_ZEDMONO          345  // Font 选项: ZedMono NF.ttf
#define CLOCK_IDC_FONT_RECMONO          342  // Font 选项: RecMonoCasual Nerd Font Mono.ttf
#define CLOCK_IDC_FONT_IOSEVKA_TERM     334  // Font 选项: IosevkaTermSlab NFP Medium.ttf
#define CLOCK_IDC_FONT_ENVYCODE         321  // Font 选项: EnvyCodeR Nerd Font.ttf
#define CLOCK_IDC_FONT_DADDYTIME        319  // Font 选项: DaddyTimeMono Nerd Font Propo.ttf
#define CLOCK_IDC_FONT_PROFONT          307  // Font 选项: ProFont IIx Nerd Font.ttf
#define CLOCK_IDC_FONT_HEAVYDATA        306  // Font 选项: HeavyData Nerd Font.ttf
#define CLOCK_IDC_FONT_BIGBLUE          303  // Font 选项: BigBlueTermPlus Nerd Font.ttf
#define CLOCK_IDC_FONT_PROGGYCLEAN      313  // Font 选项: ProggyCleanSZ Nerd Font Propo.ttf
#define CLOCK_IDC_FONT_DEPARTURE        320  // Font 选项: DepartureMono Nerd Font Propo.ttf
#define CLOCK_IDC_FONT_TERMINESS        343  // Font 选项: Terminess Nerd Font Propo.ttf
#define CLOCK_IDC_FONT_GOHUFONT         346  // Font 选项: GohuFont uni11 Nerd Font Mono.ttf

// Font Resource IDs
#define IDR_FONT_VICTORMONO     444
#define IDR_FONT_LIBERATION     438
#define IDR_FONT_ZEDMONO        445
#define IDR_FONT_RECMONO        442
#define IDR_FONT_IOSEVKA_TERM   434
#define IDR_FONT_ENVYCODE       421
#define IDR_FONT_DADDYTIME      419
#define IDR_FONT_PROFONT        407
#define IDR_FONT_HEAVYDATA      406
#define IDR_FONT_BIGBLUE        403
#define IDR_FONT_PROGGYCLEAN    413
#define IDR_FONT_DEPARTURE      420
#define IDR_FONT_TERMINESS      443
#define IDR_FONT_GOHUFONT       446

#define CLOCK_IDD_DIALOG1                1002 // 已经在 main.c 中定义
#define CLOCK_IDC_EDIT                   108  // 已经在 main.c 中定义
#define CLOCK_IDC_BUTTON_OK              109  // 已经在 main.c 中定义

// 如果需要单独的颜色对话框，可以新增资源定义
#define CLOCK_IDD_COLOR_DIALOG           1003

// ... existing resource definitions ... 

#define CLOCK_IDM_OPEN_FILE        125 
#define CLOCK_IDM_RECENT_FILE_1    126
#define CLOCK_IDM_RECENT_FILE_2    127
#define CLOCK_IDM_RECENT_FILE_3    128
#define CLOCK_IDM_BROWSE_FILE      129 
#define CLOCK_IDM_ABOUT            130 

// 番茄时钟菜单项
#define CLOCK_IDM_POMODORO            500  // 主菜单项
#define CLOCK_IDM_POMODORO_START      501  // 开始/暂停
#define CLOCK_IDM_POMODORO_WORK       502  // 工作时间设置
#define CLOCK_IDM_POMODORO_BREAK      503  // 短休息设置
#define CLOCK_IDM_POMODORO_LBREAK     504  // 长休息设置
#define CLOCK_IDM_POMODORO_RESET      505  // 重新开始
#define CLOCK_IDM_VERSION 131
#define CLOCK_IDM_CHECK_UPDATE 133   
#define CLOCK_IDM_UPDATE_GITHUB    134
#define CLOCK_IDM_UPDATE_123PAN    135
#define CLOCK_IDM_UPDATE_LANZOU    136
#define CLOCK_IDM_FEEDBACK 132   
#define CLOCK_IDM_LANGUAGE_MENU    160
#define CLOCK_IDM_LANG_CHINESE     161
#define CLOCK_IDM_LANG_ENGLISH     162
#define CLOCK_IDM_LANG_CHINESE_TRAD  163
#define CLOCK_IDM_LANG_SPANISH       164
#define CLOCK_IDM_LANG_FRENCH        165
#define CLOCK_IDM_LANG_GERMAN        166
#define CLOCK_IDM_LANG_RUSSIAN       167
#define CLOCK_IDM_LANG_PORTUGUESE    168
#define CLOCK_IDM_LANG_JAPANESE      169
#define CLOCK_IDM_LANG_KOREAN        170
#define CLOCK_IDM_FEEDBACK_GITHUB 137
#define CLOCK_IDM_FEEDBACK_BILIBILI 138
#define CLOCK_IDM_COUNT_UP_START     171
#define CLOCK_IDM_COUNT_UP_RESET     172

// 在文件开头的宏定义区域添加
#define CLOCK_IDC_TIMEOUT_BROWSE 140

// 在文件开头的宏定义区域添加这些定义
#define CLOCK_IDC_MODIFY_TIME_OPTIONS 156
#define CLOCK_IDC_MODIFY_DEFAULT_TIME 157

#define CLOCK_IDC_SET_COUNTDOWN_TIME 173  // Define a unique ID for setting countdown time
#define CLOCK_IDC_START_NO_DISPLAY   174  // Define a unique ID for starting with no display
#define CLOCK_IDC_START_COUNT_UP     175  // Define a unique ID for starting count up
#define CLOCK_IDC_AUTO_START         160  // 添加开机自启动菜单ID
#define CLOCK_IDC_START_SHOW_TIME    176  // 修改为新的未使用的ID值

// 添加新的颜色相关的控件ID
#define CLOCK_IDC_COLOR_VALUE        1301
#define CLOCK_IDC_COLOR_PANEL        1302
#define BLUR_OPACITY 192
#define BLUR_TRANSITION_MS 200

#define CLOCK_IDC_EDIT               108
#define CLOCK_IDC_BUTTON_OK          109
#define CLOCK_IDD_DIALOG1            1002
#define CLOCK_ID_TRAY_APP_ICON       1001
#define CLOCK_IDC_CUSTOMIZE_LEFT     112
#define CLOCK_IDC_EDIT_MODE          113
#define CLOCK_IDC_MODIFY_OPTIONS     114

#define CLOCK_IDM_TIMEOUT_ACTION     120
#define CLOCK_IDM_SHOW_MESSAGE       121
#define CLOCK_IDM_LOCK_SCREEN        122
#define CLOCK_IDM_SHUTDOWN           123
#define CLOCK_IDM_RESTART            124
#define CLOCK_IDM_OPEN_FILE          125

#define CLOCK_IDC_FONT_MENU           113
#define MAX_TIME_OPTIONS 10
#define CLOCK_IDM_ABOUT 130  
