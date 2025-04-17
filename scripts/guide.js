// 指南页面特有功能
document.addEventListener('DOMContentLoaded', function() {
    // 添加指南页面的翻译
    addGuideTranslations();
});

// 添加指南页面特定的翻译
function addGuideTranslations() {
    // 检查当前语言设置
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    
    // 如果当前语言是英文，则添加英文翻译
    if (currentLang === 'en') {
        // 翻译页面标题
        const pageTitle = document.getElementById('page-title');
        if (pageTitle) {
            pageTitle.textContent = 'Catime - Guide';
        }
        
        // 翻译meta描述
        const metaDescription = document.getElementById('meta-description');
        if (metaDescription) {
            metaDescription.setAttribute('content', 'Catime Guide - Learn about the latest time management tips, software updates, and user stories.');
        }
        
        // 翻译导航链接
        document.querySelectorAll('.nav-links li a').forEach(link => {
            if (link.textContent === '首页') link.textContent = 'Home';
            if (link.textContent === '指南') link.textContent = 'Guide';
            if (link.textContent === '关于') link.textContent = 'About';
            if (link.querySelector('span') && link.querySelector('span').textContent === '支持项目') {
                link.querySelector('span').textContent = 'Support';
            }
            if (link.querySelector('span') && link.querySelector('span').textContent === '下载') {
                link.querySelector('span').textContent = 'Download';
            }
        });
        
        // 翻译指南hero区域
        const guideHeroTitle = document.querySelector('.guide-hero-title');
        if (guideHeroTitle) {
            const catimeText = guideHeroTitle.querySelector('.catime-text');
            const guideAccent = guideHeroTitle.querySelector('.guide-accent');
            if (guideAccent) guideAccent.textContent = ' User Guide';
        }
        
        // 翻译主要内容
        translateGuideElements();
        
        // 翻译滚动进度提示
        const scrollTooltip = document.querySelector('.scroll-progress-tooltip');
        if (scrollTooltip) {
            scrollTooltip.textContent = 'Back to Top';
        }
        
        // 翻译页脚
        translateFooter();
        
        // 翻译特殊部分的内容
        translateSpecialElements();
    }
}

// 翻译指南页面内容元素
function translateGuideElements() {
    // 翻译标题和段落
    const translations = {
        "Hi~ 我的朋友！": "Hi~ My Friend!",
        "这里是 <a href=\"https://vladelaina.com/\" target=\"_blank\" rel=\"noopener noreferrer\" class=\"bilibili-link\"><span class=\"highlight\">Vladelaina</span></a>，Catime 的开发者，也是一名普通的大学生。": "I'm <a href=\"https://vladelaina.com/\" target=\"_blank\" rel=\"noopener noreferrer\" class=\"bilibili-link\"><span class=\"highlight\">Vladelaina</span></a>, the developer of Catime and also an ordinary college student.",
        "很高兴能在这里和你见面！": "Nice to meet you here!",
        "欢迎关注我的哔哩哔哩：": "Feel free to follow me on Bilibili: ",
        "关于我的名字": "About My Name",
        "\"Vladelaina\" 是结合了我最喜欢的两部动漫女主角的名字——《": "\"Vladelaina\" combines the names of my favorite anime heroines — ",
        "》中的 <strong>Vladilena Milizé</strong> 和《": " <strong>Vladilena Milizé</strong> from <a href=\"https://en.wikipedia.org/wiki/86_(novel_series)\" target=\"_blank\" rel=\"noopener noreferrer\">86 -Eighty Six-</a> and ",
        "》中的 <strong>Elaina</strong>。": " <strong>Elaina</strong> from <a href=\"https://en.wikipedia.org/wiki/Wandering_Witch:_The_Journey_of_Elaina\" target=\"_blank\" rel=\"noopener noreferrer\">Wandering Witch: The Journey of Elaina</a>.",
        "如果你还没看过这两部作品，真心推荐你去看看，剧情和角色都非常精彩，绝对值得一追！": "If you haven't watched these two works yet, I sincerely recommend them. The plots and characters are fantastic and definitely worth following!",
        "接下来，就让我为你介绍一下 Catime 的使用指南吧～": "Now, let me introduce you to the Catime user guide~",
        "<i class=\"fas fa-rocket\"></i> 开始使用": "<i class=\"fas fa-rocket\"></i> Getting Started",
        "软件无需安装，首次双击打开的时候在顶部显示一个25分钟的置顶倒计时": "No installation required. When you first open it, a 25-minute countdown timer appears at the top of the screen.",
        "简单": "Simple",
        "无需安装": "No Install",
        "小贴士": "Tip",
        "建议你先将 Catime 的托盘图标从隐藏区域拖到任务栏上，这样使用会更方便。": "It's recommended to drag the Catime icon from the hidden area to the taskbar for more convenient access.",
        "<i class=\"fas fa-mouse-pointer\"></i> 基本操作": "<i class=\"fas fa-mouse-pointer\"></i> Basic Operations",
        "Catime 的所有操作都通过点击任务栏图标完成：": "All Catime operations are performed by clicking the taskbar icon:",
        "左键": "Left Click",
        "打开时间管理界面": "Open time management interface",
        "右键": "Right Click",
        "打开设置菜单": "Open settings menu",
        "<i class=\"fas fa-sliders-h\"></i> 自定义功能": "<i class=\"fas fa-sliders-h\"></i> Customization",
        "我的朋友，相信你已经迫不及待地想移动一下窗口的位置了吧？比如把它放到屏幕左下角。这个时候只需": "My friend, I bet you're eager to move the window position, like to the bottom left of the screen. To do this, just ",
        "右键点击托盘图标，选择\"编辑模式\"": "right-click the tray icon and select \"Edit Mode\"",
        "。此时，": ". At this point, ",
        "窗口会变成亚克力背景": "the window will change to an acrylic background",
        "，你就可以用": " and you can use ",
        "鼠标拖拽来调整位置，滚轮调整大小": "mouse drag to adjust position, scroll wheel to resize",
        "。调整完毕后，": ". After adjusting, ",
        "鼠标在窗口上右键即可关闭编辑模式": "right-click on the window to exit edit mode",
        "～": ".",
        "恭喜完成!": "Congratulations!",
        "至此，恭喜🎉你已经掌握了Catime的核心内容，剩下的部分基本都是字面意思😉。": "Congratulations! 🎉 You've now mastered the core features of Catime. The rest is pretty self-explanatory! 😉"
    };
    
    // 应用翻译到元素
    for (const [key, value] of Object.entries(translations)) {
        // 选择所有可能包含文本的元素
        const elements = document.querySelectorAll('h1, h2, h3, h4, p, span, div.tip-content h4, div.conclusion-content h3, div.operation-details h3, div.feature-content p, div.anime-icons span');
        
        elements.forEach(el => {
            // 完全匹配
            if (el.innerHTML === key) {
                el.innerHTML = value;
            } 
            // 包含匹配（处理带有HTML的内容）
            else if (el.innerHTML && el.innerHTML.includes(key)) {
                // 替换时注意保留HTML
                el.innerHTML = el.innerHTML.replace(new RegExp(escapeRegExp(key), 'g'), value);
            }
        });
    }
}

// 转义正则表达式中的特殊字符
function escapeRegExp(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

// 翻译特殊元素，这些元素可能无法通过普通选择器匹配
function translateSpecialElements() {
    // 翻译特性标签
    document.querySelectorAll('.feature-tag').forEach(tag => {
        if (tag.textContent === '简单') tag.textContent = 'Simple';
        if (tag.textContent === '无需安装') tag.textContent = 'No Install';
    });
    
    // 翻译动漫图标
    document.querySelectorAll('.anime-icon').forEach(icon => {
        if (icon.textContent === '魔女之旅') icon.textContent = 'Wandering Witch';
    });
    
    // 翻译feature-highlight中的span内容
    const featureHighlight = document.querySelector('.feature-highlight p');
    if (featureHighlight) {
        const spans = featureHighlight.querySelectorAll('span');
        spans.forEach(span => {
            // 针对特定内容进行翻译
            if (span.textContent.includes('右键点击托盘图标，选择"编辑模式"')) {
                span.textContent = span.textContent.replace('右键点击托盘图标，选择"编辑模式"', 'right-click the tray icon and select "Edit Mode"');
            }
            if (span.textContent.includes('窗口会变成亚克力背景')) {
                span.textContent = span.textContent.replace('窗口会变成亚克力背景', 'the window will change to an acrylic background');
            }
            if (span.textContent.includes('鼠标拖拽来调整位置，滚轮调整大小')) {
                span.textContent = span.textContent.replace('鼠标拖拽来调整位置，滚轮调整大小', 'mouse drag to adjust position, scroll wheel to resize');
            }
            if (span.textContent.includes('鼠标在窗口上右键即可关闭编辑模式')) {
                span.textContent = span.textContent.replace('鼠标在窗口上右键即可关闭编辑模式', 'right-click on the window to exit edit mode');
            }
        });
    }
}

// 翻译页脚
function translateFooter() {
    const footerContent = document.querySelector('.main-footer .container');
    if (footerContent) {
        // 遍历所有段落
        footerContent.querySelectorAll('p').forEach(p => {
            const text = p.innerHTML;
            if (text.includes('基于')) {
                p.innerHTML = 'Open-sourced under <a href="https://github.com/vladelaina/Catime/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">Apache 2.0</a> license.';
            } else if (text.includes('图标画师')) {
                p.innerHTML = 'Icon artist: <a href="https://space.bilibili.com/26087398" target="_blank" rel="noopener noreferrer">猫屋敷梨梨Official</a>';
            }
        });
        
        // 翻译问题反馈链接
        const feedbackLink = footerContent.querySelector('.footer-links a');
        if (feedbackLink && feedbackLink.textContent === '问题反馈') {
            feedbackLink.textContent = 'Feedback';
        }
    }
}
