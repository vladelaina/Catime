document.addEventListener('DOMContentLoaded', function() {
    CatimeUI.setupVideoCoverPlayer({
        preloadSrc: 'assets/bilibili video cover2.webp'
    });
});

localizeGuidePage();

function localizeGuidePage() {
    if (!window.CatimeLocale?.isChinese()) return;

    document.title = 'Catime - 使用指南';
    document.querySelector('meta[name="description"]')?.setAttribute(
        'content',
        'Catime 使用指南：了解基础操作、计时方式与个性化设置。',
    );

    const translations = [
        ['.guide-hero-title .guide-accent', '使用指南'],
        ['#intro > .animated-title', '嗨，我的朋友！'],
        ['.author-info p:nth-child(1)', '我是 <a href="https://vladelaina.com/" target="_blank" rel="noopener noreferrer" class="bilibili-link"><span class="highlight">Vladelaina</span></a>，Catime 的开发者，也是一名普通大学生。'],
        ['.author-info p:nth-child(2)', '很高兴在这里见到你！'],
        ['.author-info p:nth-child(3)', '欢迎在哔哩哔哩关注我：<a href="https://space.bilibili.com/1862395225" target="_blank" rel="noopener noreferrer" class="bilibili-link"><i class="fab fa-bilibili"></i> Vladelaina</a>'],
        ['.info-card h3', '<i class="fas fa-info-circle"></i> 关于我的名字'],
        ['.info-card p:nth-of-type(1)', '“Vladelaina” 来自我喜欢的两位动画角色：<a href="https://en.wikipedia.org/wiki/86_(novel_series)" target="_blank" rel="noopener noreferrer">《86 -不存在的战区-》</a>中的 Vladilena Milizé，以及<a href="https://en.wikipedia.org/wiki/Wandering_Witch:_The_Journey_of_Elaina" target="_blank" rel="noopener noreferrer">《魔女之旅》</a>中的 Elaina。'],
        ['.info-card p:nth-of-type(2)', '如果你还没有看过这两部作品，我很推荐去了解一下，它们的故事和角色都很值得。'],
        ['.guide-intro', '接下来，让我为你介绍 Catime 的使用方法吧。'],
        ['#intro > h3.animated-title', '<i class="fas fa-play-circle"></i> 演示视频'],
        ['#intro > .section-intro', '下面的视频可以帮助你更直观地了解 Catime：'],
        ['#start > .animated-title', '<i class="fas fa-rocket"></i> 开始使用'],
        ['#start .feature-content p', '无需安装，第一次打开时，屏幕顶部会显示一个 25 分钟倒计时。'],
        ['#start .feature-tag:nth-child(1)', '简单'],
        ['#start .feature-tag:nth-child(2)', '便携'],
        ['#start .tip-content h4', '提示'],
        ['#start .tip-content p', '建议将 Catime 托盘图标从隐藏区域拖到任务栏，之后操作会更方便。'],
        ['#operations > .animated-title', '<i class="fas fa-mouse-pointer"></i> 基础操作'],
        ['#operations > .section-intro', 'Catime 的所有操作都通过点击任务栏图标完成：'],
        ['#operations .operation-card:nth-child(1) h3', '左键单击'],
        ['#operations .operation-card:nth-child(1) p', '打开时间管理界面'],
        ['#operations .operation-card:nth-child(2) h3', '右键单击'],
        ['#operations .operation-card:nth-child(2) p', '打开设置菜单'],
        ['#customization > .animated-title', '<i class="fas fa-sliders-h"></i> 个性化设置'],
        ['#customization .feature-highlight p', '想把计时器移动到屏幕左下角吗？右键单击托盘图标并选择“编辑模式”，窗口会显示亚克力背景。此时可以拖动窗口调整位置，并使用滚轮调节大小；完成后，在窗口上右键单击即可退出编辑模式。'],
        ['#customization .conclusion-content h3', '恭喜！'],
        ['#customization .conclusion-content p', '到这里，你已经掌握了 Catime 的核心用法，其他功能也可以很自然地探索使用。'],
    ];

    translations.forEach(([selector, value]) => {
        const element = document.querySelector(selector);
        if (!element) return;
        if (value.includes('<')) element.innerHTML = value;
        else element.textContent = value;
    });
}


