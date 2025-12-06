document.addEventListener('DOMContentLoaded', function() {
    const videoContainer = document.getElementById('videoContainer');
    if (videoContainer) {
        const videoCover = videoContainer.querySelector('.video-cover');
        const videoFrameContainer = videoContainer.querySelector('.video-frame-container');
        const autoplayIframe = document.getElementById('autoplayIframe');
        
        const preloadImg = new Image();
        preloadImg.src = 'assets/bilibili video cover.jpg';
        
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
    
    addAboutTranslations();
});

function addAboutTranslations() {
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    
    if (currentLang === 'en') {
        const pageTitle = document.getElementById('page-title');
        if (pageTitle) {
            pageTitle.textContent = 'Catime - About';
        }
        
        const metaDescription = document.getElementById('meta-description');
        if (metaDescription) {
            metaDescription.setAttribute('content', 'Catime About - Learn about the origins of Catime, development stories, and the philosophy behind it.');
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
            const guideAccent = guideHeroTitle.querySelector('.guide-accent');
            if (guideAccent) guideAccent.textContent = 'About';
        }
        
        translateAboutElements();
        
        translateSpecialElements();
    }
}

function translateAboutElements() {
    const translations = {
        "嘿，朋友！": "Hey, Friend!",
        "你可能会有不少好奇：": "You might be curious about:",
        "\"Catime 这个名字是怎么来的？\"": "\"How did the name 'Catime' come about?\"",
        "\"为什么要用 C 语言写？\"": "\"Why was it written in C?\"",
        "\"还有，为什么图标是猫猫泥？\"": "\"And why is the icon a cat?\"",
        "梦开始的地方": "Where the Dream Began",
        "那时候的我，还只是个大二的普通大学生，每天长时间坐在电脑前。你有没有过那种久坐之后，肩膀酸痛到不行的感觉？我可是深有体会。 😫": "Back then, I was just a sophomore in college, spending long hours in front of the computer. Have you ever felt that terrible shoulder pain after sitting for too long? I certainly know that feeling. 😫",
        "当时我心想：\"要是能有一把人体工学椅，或许就能缓解这些问题了吧？\" 🤔": "I thought to myself: \"Maybe an ergonomic chair would help with these issues?\" 🤔",
        "于是，咬牙斥巨资💸 500 多元买了一把（要知道那时候我一个月的生活费也才 1500）。": "So I bit the bullet and spent over 500 yuan on one (which was significant considering my monthly allowance was only 1500).",
        "唔~ 然而买回来才发现，虽然疼痛有稍微减轻一些，但远远谈不上\"解决\"。😕": "Hmm~ However, I soon realized that while the pain was slightly reduced, it was far from being \"solved\". 😕",
        "那一刻我意识到，也许我需要尝试一些不一样的方法，去真正解决这个困扰我的问题。✨": "At that moment, I realized that I might need to try something different to truly solve this problem that was bothering me. ✨",
        "我知道问题的根源是\"久坐\"。": "I knew the root of the problem was \"sitting too long\".",
        "那该怎么才能让我离开桌面、强迫自己活动一下呢？": "So how could I make myself leave the desk and force myself to move around?",
        "于是我开始尝试一些计时器工具，比如 <a href=\"https://github.com/Splode/pomotroid\" target=\"_blank\" rel=\"noopener noreferrer\"> Pomotroid</a>，还有 <a href=\"https://github.com/xujiangjiang/Easy-Cat-Timer\" target=\"_blank\" rel=\"noopener noreferrer\">Easy-Cat-Timer</a> 等这些优秀又开源的番茄时钟和计时器。": "So I started trying some timer tools, like <a href=\"https://github.com/Splode/pomotroid\" target=\"_blank\" rel=\"noopener noreferrer\">Pomotroid</a> and <a href=\"https://github.com/xujiangjiang/Easy-Cat-Timer\" target=\"_blank\" rel=\"noopener noreferrer\">Easy-Cat-Timer</a>, which are excellent open-source pomodoro timers.",
        "但很快我发现——这些工具对我并不管用。": "But I quickly found that these tools didn't work for me.",
        "时间一到，我不是选择忽略，就是直接一键关闭提示。": "When time was up, I would either ignore it or simply close the notification with one click.",
        "那时候我心里冒出个想法：<strong>\"要不干脆自己写一个算了！\"</strong>": "Then an idea popped into my head: <strong>\"Why not just write one myself!\"</strong>",
        "唔~说实话，那时候我还是个小白，除了会点 C，其他几乎啥也不会。": "Well, to be honest, I was still a beginner at that time. Apart from knowing a bit of C, I barely knew anything else.",
        "不过好在这个项目也不算复杂，于是我磕磕绊绊地做了两个多月，中间还重构了一次。": "Fortunately, this project wasn't too complex, so I spent more than two months stumbling through it, even refactoring it once.",
        "一开始我还想着整点花活儿，比如做个角色能从左边走到右边，后来一想，好像也没必要……": "At first, I was thinking about adding some fancy features, like a character that could walk from left to right, but then I thought it wasn't really necessary...",
        "最终我保留了最基础、最实用的部分 —— 简洁的计时器界面和提醒功能。": "In the end, I kept the most basic and practical parts - a clean timer interface and reminder function.",
        "一开始其实我也认真考虑过加一个设置界面，甚至连 UI 图都画好了。": "At first, I seriously considered adding a settings interface, and even drew the UI diagrams.",
        "但当我真正动手的时候才发现——<strong>用 C 写 UI，简直是灾难现场。</strong>": "But when I actually started implementing it, I realized that <strong>writing UI in C was a complete disaster.</strong>",
        "真的是每多写一行都在怀疑人生……": "I was questioning my life choices with every line of code I wrote...",
        "最终只能忍痛割爱，放弃了这个念头。": "In the end, I had to painfully give up on this idea.",
        "本来是懒得写 UI，结果竟然意外实现了\"极简主义设计\"。": "I was just too lazy to write a UI, but unexpectedly ended up achieving a \"minimalist design\".",
        "第一个版本发布的时候，我还顺带肝了个演示视频——主要是因为当时还不支持中文，全英文界面，生怕大家看不懂我在整啥。": "When I released the first version, I also made a demo video - mainly because it didn't support Chinese at that time, and the interface was all in English. I was afraid people wouldn't understand what I was doing.",
        "唔，好吧，视频也救不了——还是有点让人看不懂。": "Hmm, well, the video didn't help much either - it was still a bit confusing.",
        "😂本来只是随手整了个 demo，结果视频一发，评论区居然开始热闹了起来。": "😂 I just made a quick demo, but after posting the video, the comment section suddenly became lively.",
        "我一边怀疑人生一边想：<strong>\"不会吧，不会真有人想用吧？\"</strong>": "I was questioning my life while thinking: <strong>\"No way, are there really people who want to use this?\"</strong>",
        "然后……我竟然开始认真了。": "And then... I actually started to take it seriously.",
        "关于名字的由来": "About the Name",
        "我之前一直在用 <a href=\"https://github.com/xujiangjiang/Easy-Cat-Timer\" target=\"_blank\" rel=\"noopener noreferrer\">Easy-Cat-Timer</a>（C#） ——真的超级好用！": "I had been using <a href=\"https://github.com/xujiangjiang/Easy-Cat-Timer\" target=\"_blank\" rel=\"noopener noreferrer\">Easy-Cat-Timer</a> (C#) before — it was really great!",
        "而且特别特别可爱！！！✨": "And it was super, super cute!!! ✨",
        "不过有点可惜，它已经 <strong>6 年没更新了</strong>。": "But it's a bit unfortunate that it hasn't been updated for <strong>6 years</strong>.",
        "当时我心想：<strong>\"要不我来魔改一下试试？\"</strong>": "I thought to myself: <strong>\"Why don't I try to modify it?\"</strong>",
        "但很快我就意识到……<strong>好吧，C# 对我来说还是太难了 QAQ</strong>": "But I quickly realized... <strong>Well, C# was still too difficult for me QAQ</strong>",
        "所以我干脆用我唯一稍微会一点的 C，自己整一个！": "So I decided to use C, the only language I knew a little bit of, to make one myself!",
        "而 <strong>\"Catime\"</strong> 这个名字，其实是我向 <strong>Easy-Cat-Timer</strong> 的作者——<a href=\"https://space.bilibili.com/222417\" target=\"_blank\" rel=\"noopener noreferrer\">絮酱酱</a> 致敬而取的。": "And the name <strong>\"Catime\"</strong> is actually a tribute to the author of <strong>Easy-Cat-Timer</strong> — <a href=\"https://space.bilibili.com/222417\" target=\"_blank\" rel=\"noopener noreferrer\">xujiangjiang</a>.",
        "她是一位既可爱又坚强的女孩子！！！": "She is a cute and strong girl!!!",
        "尽管她的项目已经停更多年，但她是我梦想开始的地方。祝絮酱酱早日康复~💕": "Although her project hasn't been updated for many years, she was where my dream began. I wish xujiangjiang a speedy recovery~💕",
        "命运有时候真的很巧。某天中午，她忽然更新了一条<a href=\"https://www.bilibili.com/opus/1049961668160782336?spm_id_from=333.1387.0.0\" target=\"_blank\" rel=\"noopener noreferrer\">动态</a>，就像是冥冥中的安排，我也正是借着这条动态，终于联系上了她。": "Fate is sometimes really coincidental. One afternoon, she suddenly posted an <a href=\"https://www.bilibili.com/opus/1049961668160782336?spm_id_from=333.1387.0.0\" target=\"_blank\" rel=\"noopener noreferrer\">update</a>, and it seemed like it was meant to be. Through this post, I was finally able to contact her.",
        "关于 Logo 的故事": "The Story of the Logo",
        "当时我在跟<a href=\"https://space.bilibili.com/475437261\" target=\"_blank\" rel=\"noopener noreferrer\">画师</a>讨论设计logo的时候，其实自己脑子里面没有太多的概念，我只知道想要的图标跟猫咪还有二次元有关，具体呢，其实我也不知道。不过刚好我有一个表情包": "When I was discussing the logo design with the <a href=\"https://space.bilibili.com/475437261\" target=\"_blank\" rel=\"noopener noreferrer\">artist</a>, I didn't have many concrete ideas. I just knew I wanted an icon related to cats and anime, but I wasn't sure about the specifics. Fortunately, I had an emoticon pack",
        "上面刚好有一个猫咪，然后她描了一遍": "There was a cat on it, and she traced it",
        "\"唔~要不我帮你查一下这个图的来源\"": "\"Hmm~ Why don't I help you find the source of this image\"",
        "\"<a href=\"https://space.bilibili.com/26087398?spm_id_from=333.1387.follow.user_card.click\" target=\"_blank\" rel=\"noopener noreferrer\">猫屋敷梨梨Official</a>\"": "\"<a href=\"https://space.bilibili.com/26087398?spm_id_from=333.1387.follow.user_card.click\" target=\"_blank\" rel=\"noopener noreferrer\">Maowushi Lili Official</a>\"",
        "说实话，这是我第一次听到这个名字，然后.....没错，我直接私信up，然后": "To be honest, this was the first time I heard this name, and then... yes, I directly sent a private message to the creator, and then",
        "对啦~小声嘀咕：我之前的画师把我给我删了🤣！！！": "Oh right~ whispers: My previous artist deleted me 🤣!!!",
        "结语": "Conclusion",
        "Catime 能走到今天，离不开每一位小伙伴的<span class=\"emphasis-text\">支持与付出</span>。这不是某一个人的成就，而是我们<span class=\"emphasis-text\">携手共创</span>的成果，是无数心血与热爱的凝聚。": "Catime's journey to this point wouldn't have been possible without the <span class=\"emphasis-text\">support and contributions</span> of each and every one of you. This isn't the achievement of a single person, but the result of our <span class=\"emphasis-text\">collaborative creation</span>, a culmination of countless hours of hard work and passion.",
        "开源的魅力，远不止于代码的共享，更是一场<span class=\"emphasis-text\">知识的交流</span>、<span class=\"emphasis-text\">思想的碰撞</span>与<span class=\"emphasis-text\">创意的汇聚</span>。每一次提交、每一次反馈、每一次细微的优化，背后都承载着一份认真、一份热情，以及对更好工具的共同追求。": "The beauty of open source extends far beyond code sharing – it's about <span class=\"emphasis-text\">knowledge exchange</span>, <span class=\"emphasis-text\">ideation</span>, and <span class=\"emphasis-text\">creative convergence</span>. Each commit, feedback, and optimization carries dedication, passion, and our collective pursuit of better tools.",
        "开源真正的价值，不仅在于技术的持续演进，更在于<span class=\"emphasis-text\">人与人之间真诚的连接与支持</span>。在这样开放、信任的氛围中，我收获了来自社区无数宝贵的<span class=\"primary-text\">建议</span>、<span class=\"primary-text\">鼓励</span>与<span class=\"primary-text\">启发</span>，让 Catime 从一个简单的计时工具，不断成长、持续打磨，变得更加实用与完善。": "The true value of open source lies not just in technological evolution but in <span class=\"emphasis-text\">genuine human connections and support</span>. In this open, trusting environment, I've received countless valuable <span class=\"primary-text\">suggestions</span>, <span class=\"primary-text\">encouragement</span>, and <span class=\"primary-text\">inspiration</span> from the community, helping Catime grow from a simple timer into something more useful and refined.",
        "我的朋友非常感谢你能看到这里，在结束前我想分享一句我特别喜欢的话送给你，出自于李笑来的《斯坦福大学创业成长课》：": "My friend, thank you so much for reading this far. Before concluding, I'd like to share a quote I particularly love from Li Xiaolai's 'Stanford Entrepreneurship Growth Course':",
        "如果你想要的东西还不存在，那就亲自动手将它创造出来。": "If what you want doesn't exist yet, create it yourself.",
        "衷心<span class=\"emphasis-text\">感谢</span>每一位关注、使用与贡献 Catime 的朋友。正是因为你们，开源世界才如此精彩！未来，我会继续用心维护 Catime，<span class=\"primary-text\">倾听大家的声音</span>，<span class=\"primary-text\">不断优化体验</span>，让这个小工具变得更加可靠、实用，也更加贴近每一个使用它的你。": "I sincerely <span class=\"emphasis-text\">thank</span> everyone who has followed, used, and contributed to Catime. It's because of you that the open source world is so wonderful! In the future, I will continue to maintain Catime with care, <span class=\"primary-text\">listen to your feedback</span>, and <span class=\"primary-text\">continuously optimize the experience</span>, making this little tool more reliable, practical, and closer to each of you who uses it.",
        "开源是一段<span class=\"emphasis-text\">没有终点的旅程</span>，真正的意义不仅在于写了多少行代码，更在于我们彼此之间的<span class=\"emphasis-text\">连接与共创</span>。希望在未来的路上，仍能与你们一起前行，让 Catime 与这个社区一起持续成长。": "Open source is a <span class=\"emphasis-text\">journey without an end</span>, and its true meaning lies not just in how many lines of code are written, but in the <span class=\"emphasis-text\">connections and co-creation</span> between us. I hope that on the road ahead, we can continue to move forward together, allowing Catime and this community to grow continuously."
    };
    
    for (const [key, value] of Object.entries(translations)) {
        const elements = document.querySelectorAll('h1, h2, h3, h4, p, span.question-text, div.feature-content p, div.quote-highlight');
        
        elements.forEach(el => {
            if (el.innerHTML === key) {
                el.innerHTML = value;
            } 
            else if (el.innerHTML && el.innerHTML.includes(key)) {
                el.innerHTML = el.innerHTML.replace(new RegExp(escapeRegExp(key), 'g'), value);
            }
        });
    }
}

function escapeRegExp(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function translateSpecialElements() {
    document.querySelectorAll('.story-begin-title, .name-origin-title, .logo-story-title, .conclusion-story-title').forEach(title => {
        if (title.textContent === '梦开始的地方') title.textContent = 'Where the Dream Began';
        if (title.textContent === '关于名字的由来') title.textContent = 'About the Name';
        if (title.textContent === '关于 Logo 的故事') title.textContent = 'The Story of the Logo';
        if (title.textContent === '结语') title.textContent = 'Conclusion';
    });
    
    const questionItems = document.querySelectorAll('.question-item .question-text');
    questionItems.forEach(item => {
        if (item.innerHTML.includes('Catime 这个名字是怎么来的')) {
            item.innerHTML = item.innerHTML.replace('Catime 这个名字是怎么来的', 'How did the name \'Catime\' come about');
        }
        if (item.innerHTML.includes('为什么要用 C 语言写')) {
            item.innerHTML = item.innerHTML.replace('为什么要用 C 语言写', 'Why was it written in C');
        }
        if (item.innerHTML.includes('为什么图标是猫猫泥')) {
            item.innerHTML = item.innerHTML.replace('为什么图标是猫猫泥', 'Why is the icon a cat');
        }
    });
}


