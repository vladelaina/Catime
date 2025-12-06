document.addEventListener('DOMContentLoaded', function() {
    const videoContainer = document.getElementById('videoContainer');
    if (videoContainer) {
        const videoCover = videoContainer.querySelector('.video-cover');
        const videoFrameContainer = videoContainer.querySelector('.video-frame-container');
        const autoplayIframe = document.getElementById('autoplayIframe');
        
        const preloadImg = new Image();
        preloadImg.src = 'assets/bilibili video cover2.jpg';
        
        videoCover.addEventListener('click', function() {
            videoCover.style.opacity = '0';
            videoFrameContainer.style.display = 'block';
            
            const realSrc = autoplayIframe.getAttribute('data-src');
            autoplayIframe.src = realSrc;
            
            setTimeout(function() {
                videoCover.style.display = 'none';
                autoplayIframe.style.opacity = '1';
            }, 50);
        });
    }
    
    addGuideTranslations();
});

function addGuideTranslations() {
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    
    if (currentLang === 'en') {
        const pageTitle = document.getElementById('page-title');
        if (pageTitle) {
            pageTitle.textContent = 'Catime - Guide';
        }
        
        const metaDescription = document.getElementById('meta-description');
        if (metaDescription) {
            metaDescription.setAttribute('content', 'Catime Guide - Learn about the latest time management tips, software updates, and user stories.');
        }
        
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
        
        const guideHeroTitle = document.querySelector('.guide-hero-title');
        if (guideHeroTitle) {
            const catimeText = guideHeroTitle.querySelector('.catime-text');
            const guideAccent = guideHeroTitle.querySelector('.guide-accent');
            if (guideAccent) guideAccent.textContent = ' User Guide';
        }
        
        translateGuideElements();
        
        translateSpecialElements();
    }
}

function translateGuideElements() {
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
        "至此，恭喜🎉你已经掌握了Catime的核心内容，剩下的部分基本都是字面意思😉。": "Congratulations! 🎉 You've now mastered the core features of Catime. The rest is pretty self-explanatory! 😉",
        "<i class=\"fas fa-play-circle\"></i> 演示视频": "<i class=\"fas fa-play-circle\"></i> Demo Video",
        "这里有一个演示视频，帮助你更好地了解Catime的使用方法：": "Here's a demo video to help you better understand how to use Catime:",
        "返回顶部": "Back to Top",
        "86-不存在的战区-": "86 -Eighty Six-",
        "魔女之旅": "Wandering Witch: The Journey of Elaina",
        "section-intro": "section-intro en"
    };
    
    for (const [key, value] of Object.entries(translations)) {
        const elements = document.querySelectorAll('h1, h2, h3, h4, p, span, div.tip-content h4, div.conclusion-content h3, div.operation-details h3, div.feature-content p, div.anime-icons span, a');
        
        elements.forEach(el => {
            if (el.innerHTML === key) {
                el.innerHTML = value;
            } 
            else if (el.innerHTML && el.innerHTML.includes(key)) {
                el.innerHTML = el.innerHTML.replace(new RegExp(escapeRegExp(key), 'g'), value);
            }
        });
    }

    const infoCard = document.querySelector('.info-card');
    if (infoCard) {
        const paragraphs = infoCard.querySelectorAll('p');
        if (paragraphs.length >= 1) {
            paragraphs[0].innerHTML = "\"Vladelaina\" combines the names of my favorite anime heroines — <strong>Vladilena Milizé</strong> from <a href=\"https://en.wikipedia.org/wiki/86_(novel_series)\" target=\"_blank\" rel=\"noopener noreferrer\">86 -Eighty Six-</a> and <strong>Elaina</strong> from <a href=\"https://en.wikipedia.org/wiki/Wandering_Witch:_The_Journey_of_Elaina\" target=\"_blank\" rel=\"noopener noreferrer\">Wandering Witch: The Journey of Elaina</a>.";
        }
    }

    document.querySelectorAll('a').forEach(link => {
        if (link.href && link.href.includes('86_(novel_series)') && link.textContent.includes('86-不存在的战区-')) {
            link.textContent = '86 -Eighty Six-';
        }
        if (link.href && link.href.includes('Wandering_Witch') && link.textContent.includes('魔女之旅')) {
            link.textContent = 'Wandering Witch: The Journey of Elaina';
        }
    });
}

function escapeRegExp(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function translateSpecialElements() {
    document.querySelectorAll('.feature-tag').forEach(tag => {
        if (tag.textContent === '简单') tag.textContent = 'Simple';
        if (tag.textContent === '无需安装') tag.textContent = 'No Install';
    });
    
    document.querySelectorAll('.anime-icon').forEach(icon => {
        if (icon.textContent === '魔女之旅') icon.textContent = 'Wandering Witch';
        if (icon.textContent === '86') icon.textContent = '86';
    });
    
    const featureHighlight = document.querySelector('.feature-highlight p');
    if (featureHighlight) {
        featureHighlight.innerHTML = `My friend, I bet you're eager to move the window position, like to the bottom left of the screen. To do this, just <span class="text-accent"><i class="fas fa-mouse-pointer icon-animate-rotate"></i> right-click the tray icon and select "Edit Mode"</span>. At this point, <span class="text-primary"><i class="fas fa-window-maximize icon-animate-bounce"></i> the window will change to an acrylic background</span> and you can use <span class="text-success"><i class="fas fa-arrows-alt icon-animate-jump"></i> mouse drag to adjust position, scroll wheel to resize</span>. After adjusting, <span class="text-warning"><i class="fas fa-check-circle icon-animate-pulse"></i> right-click on the window to exit edit mode</span>.`;
    }

    const sectionIntro = document.querySelector('.section-intro');
    if (sectionIntro && sectionIntro.textContent.includes('Catime 的所有操作都通过点击任务栏图标完成')) {
        sectionIntro.textContent = 'All Catime operations are performed by clicking the taskbar icon:';
    }

    document.querySelectorAll('img').forEach(img => {
        if (img.alt === 'First time use') img.alt = 'First time use';
        if (img.alt === 'Move out') img.alt = 'Move out';
        if (img.alt === 'left click') img.alt = 'left click';
        if (img.alt === 'right click') img.alt = 'right click';
        if (img.alt === 'move') img.alt = 'move';
    });
}


