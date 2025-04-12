// AOS 初始化
document.addEventListener('DOMContentLoaded', function() {
    // 初始化 AOS 动画库
    AOS.init({
        duration: 800,
        once: true,
        offset: 50,
    });

    // 滚动进度指示器功能
    const progressContainer = document.getElementById('scrollProgressContainer');
    const progressCircle = document.querySelector('.scroll-progress-circle-fill');
    const progressPercentage = document.querySelector('.scroll-progress-percentage');
    const progressIcon = document.querySelector('.scroll-progress-icon');
    const circleLength = progressCircle.getTotalLength ? 
                         progressCircle.getTotalLength() : 283; // 回退值
    
    // 初始设置
    progressCircle.style.strokeDasharray = circleLength;
    progressCircle.style.strokeDashoffset = circleLength;
    
    // 计算滚动进度
    function updateProgress() {
        // 获取文档高度和滚动位置
        const scrollTop = window.pageYOffset || document.documentElement.scrollTop;
        const docHeight = document.documentElement.scrollHeight - document.documentElement.clientHeight;
        const scrollPercentage = scrollTop / docHeight;
        
        // 更新环形进度条
        const offset = circleLength - (scrollPercentage * circleLength);
        progressCircle.style.strokeDashoffset = offset;
        
        // 更新百分比文本
        const percentValue = Math.min(Math.round(scrollPercentage * 100), 100);
        progressPercentage.textContent = `${percentValue}%`;
        
        // 改变颜色和图标效果，根据进度动态变化
        updateAppearanceByProgress(percentValue);
    }
    
    // 根据进度动态更改外观
    function updateAppearanceByProgress(percentValue) {
        // 设置渐变色比例
        if (percentValue > 80) {
            progressIcon.style.color = 'var(--accent-color)';
            progressIcon.style.textShadow = '0 2px 8px rgba(247, 125, 170, 0.5)';
        } else if (percentValue > 40) {
            progressIcon.style.color = '#9aa5ce';
            progressIcon.style.textShadow = '0 2px 5px rgba(154, 165, 206, 0.4)';
        } else {
            progressIcon.style.color = 'var(--primary-color)';
            progressIcon.style.textShadow = '0 2px 5px rgba(122, 162, 247, 0.3)';
        }
        
        // 调整容器大小和背景，根据滚动位置微调
        const sizeAdjust = 1 + (percentValue * 0.0015); // 最大放大到1.15倍
        progressContainer.style.transform = `scale(${sizeAdjust})`;
        
        // 增加随机微动效果
        if (Math.random() > 0.95) {
            addMicroMovement();
        }
    }
    
    // 添加微小的随机动画，让图标更生动
    function addMicroMovement() {
        const stars = document.querySelectorAll('.star');
        const randomStar = stars[Math.floor(Math.random() * stars.length)];
        randomStar.style.opacity = '0.7';
        
        setTimeout(() => {
            randomStar.style.opacity = '0';
        }, 300);
    }
    
    // 点击回到顶部，添加动画效果
    progressContainer.addEventListener('click', function() {
        // 添加点击效果类
        this.classList.add('clicked');
        
        // 创建粒子爆发效果
        createParticleBurst();
        
        // 平滑滚动到顶部
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
        
        // 播放声音效果（如果浏览器支持）
        playClickSound();
        
        // 移除点击效果类
        setTimeout(() => {
            this.classList.remove('clicked');
        }, 500);
    });
    
    // 播放点击声音 (轻微的"叮"声)
    function playClickSound() {
        try {
            const audioContext = new (window.AudioContext || window.webkitAudioContext)();
            const oscillator = audioContext.createOscillator();
            const gainNode = audioContext.createGain();
            
            oscillator.type = 'sine';
            oscillator.frequency.setValueAtTime(800, audioContext.currentTime);
            oscillator.frequency.exponentialRampToValueAtTime(500, audioContext.currentTime + 0.2);
            
            gainNode.gain.setValueAtTime(0.1, audioContext.currentTime);
            gainNode.gain.exponentialRampToValueAtTime(0.001, audioContext.currentTime + 0.3);
            
            oscillator.connect(gainNode);
            gainNode.connect(audioContext.destination);
            
            oscillator.start();
            oscillator.stop(audioContext.currentTime + 0.3);
        } catch (e) {
            // 浏览器不支持 Web Audio API，静默失败
            console.log('Audio API不受支持');
        }
    }
    
    // 创建粒子爆发效果
    function createParticleBurst() {
        const particles = document.querySelectorAll('.particle');
        
        particles.forEach(particle => {
            // 重置动画
            particle.style.animation = 'none';
            particle.offsetHeight; // 触发重排
            particle.style.animation = null;
            
            // 随机位置和颜色
            const hue = Math.floor(Math.random() * 30) + 330; // 粉红色范围
            const saturation = Math.floor(Math.random() * 30) + 70;
            const lightness = Math.floor(Math.random() * 20) + 70;
            
            particle.style.background = `hsl(${hue}, ${saturation}%, ${lightness}%)`;
            particle.style.boxShadow = `0 0 10px hsl(${hue}, ${saturation}%, ${lightness}%)`;
        });
    }
    
    // 悬停时显示完整的工具提示
    progressContainer.addEventListener('mouseenter', function() {
        document.querySelector('.scroll-progress-tooltip').textContent = "返回顶部";
    });
    
    // 监听滚动事件，使用防抖处理
    let scrollTimeout;
    window.addEventListener('scroll', function() {
        clearTimeout(scrollTimeout);
        scrollTimeout = setTimeout(updateProgress, 10);
    });
    
    // 初始化进度
    updateProgress();
    
    // 定期添加微小的动画，使元素更生动
    setInterval(() => {
        if (!progressContainer.matches(':hover')) {
            if (Math.random() > 0.7) {
                addMicroMovement();
            }
        }
    }, 3000);
    
    // 为指南页面的图片添加3D倾斜效果
    document.querySelectorAll('.animated-image img').forEach(img => {
        const container = img.closest('.animated-image');
        
        // 最大倾斜角度
        const maxRotateX = 10;
        const maxRotateY = 15;
        
        // 呼吸效果的定时器
        let breatheTimer = null;
        
        // 当前旋转角度
        let currentRotateX = 0;
        let currentRotateY = 0;
        
        // 设置呼吸效果
        function startBreatheEffect() {
            if (breatheTimer) return; // 避免重复启动
            
            let phase = 0;
            breatheTimer = setInterval(() => {
                // 添加缩放效果，轻微的呼吸效果
                const scale = 1.02 + Math.sin(phase) * 0.015;
                
                // 应用变换，结合当前的旋转角度和缩放效果
                img.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                
                phase += 0.05;
            }, 30);
        }

        // 停止呼吸效果
        function stopBreatheEffect() {
            if (breatheTimer) {
                clearInterval(breatheTimer);
                breatheTimer = null;
            }
        }
        
        container.addEventListener('mousemove', function(e) {
            // 获取鼠标在元素内的相对位置（0-1）
            const rect = container.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width;
            const y = (e.clientY - rect.top) / rect.height;
            
            // 计算倾斜角度（转换为-maxRotate到maxRotate的范围）
            currentRotateY = (x - 0.5) * 2 * maxRotateY;
            currentRotateX = (y - 0.5) * -2 * maxRotateX; // 负号让鼠标在顶部时，图片向上倾斜
        });
        
        // 鼠标离开时恢复原始状态
        container.addEventListener('mouseleave', function() {
            stopBreatheEffect(); // 停止呼吸效果
            currentRotateX = 0;
            currentRotateY = 0;
            // 直接设置回初始状态
            img.style.transform = 'scale(1) perspective(1000px)';
        });
        
        // 鼠标进入时准备变换并启动呼吸效果
        container.addEventListener('mouseenter', function() {
            img.style.transition = 'transform 0.2s ease-out';
            startBreatheEffect(); // 启动呼吸效果
        });
        
        // 添加点击效果：按下和回弹
        img.addEventListener('mousedown', function() {
            // 暂时停止呼吸效果
            stopBreatheEffect();
            // 按下效果 - 只添加轻微下沉，不缩放
            img.style.transform = `scale(0.98) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
        });
        
        // 鼠标释放时回弹
        document.addEventListener('mouseup', function(event) {
            if (container.matches(':hover')) {
                // 如果鼠标仍在图片上，回弹到悬停状态
                if (event.target === img || img.contains(event.target)) {
                    // 添加更快速的回弹效果
                    img.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                    
                    // 重新启动呼吸效果之前，先执行回弹动画
                    setTimeout(() => {
                        startBreatheEffect();
                    }, 100);
                    
                    // 恢复正常过渡，延迟时间与动画时长匹配
                    setTimeout(() => {
                        img.style.transition = 'transform 0.2s ease-out';
                    }, 150);
                }
            } else {
                // 如果鼠标不在图片上，保持停止呼吸效果的状态
                img.style.transform = 'scale(1) perspective(1000px)';
            }
        });
    });
    
    // 为左右键小图片添加3D倾斜效果
    document.querySelectorAll('.small-image img').forEach(img => {
        const container = img.closest('.small-image');
        
        // 最大倾斜角度 - 对于小图片稍微减少倾斜角度
        const maxRotateX = 8;
        const maxRotateY = 12;
        
        // 呼吸效果的定时器
        let breatheTimer = null;
        
        // 当前旋转角度
        let currentRotateX = 0;
        let currentRotateY = 0;
        
        // 设置呼吸效果
        function startBreatheEffect() {
            if (breatheTimer) return; // 避免重复启动
            
            let phase = 0;
            breatheTimer = setInterval(() => {
                // 添加缩放效果，轻微的呼吸效果，小图片波动更小
                const scale = 1.01 + Math.sin(phase) * 0.01;
                
                // 应用变换，结合当前的旋转角度和缩放效果
                img.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                
                phase += 0.05;
            }, 30);
        }

        // 停止呼吸效果
        function stopBreatheEffect() {
            if (breatheTimer) {
                clearInterval(breatheTimer);
                breatheTimer = null;
            }
        }
        
        container.addEventListener('mousemove', function(e) {
            // 获取鼠标在元素内的相对位置（0-1）
            const rect = container.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width;
            const y = (e.clientY - rect.top) / rect.height;
            
            // 计算倾斜角度（转换为-maxRotate到maxRotate的范围）
            currentRotateY = (x - 0.5) * 2 * maxRotateY;
            currentRotateX = (y - 0.5) * -2 * maxRotateX; // 负号让鼠标在顶部时，图片向上倾斜
        });
        
        // 鼠标离开时恢复原始状态
        container.addEventListener('mouseleave', function() {
            stopBreatheEffect(); // 停止呼吸效果
            currentRotateX = 0;
            currentRotateY = 0;
            // 直接设置回初始状态
            img.style.transform = 'scale(1) perspective(1000px)';
        });
        
        // 鼠标进入时准备变换并启动呼吸效果
        container.addEventListener('mouseenter', function() {
            img.style.transition = 'transform 0.2s ease-out';
            startBreatheEffect(); // 启动呼吸效果
        });
        
        // 添加点击效果：按下和回弹
        img.addEventListener('mousedown', function() {
            // 暂时停止呼吸效果
            stopBreatheEffect();
            // 按下效果 - 只添加轻微下沉，不缩放
            img.style.transform = `scale(0.98) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
        });
        
        // 鼠标释放时回弹
        document.addEventListener('mouseup', function(event) {
            if (container.matches(':hover')) {
                // 如果鼠标仍在图片上，回弹到悬停状态
                if (event.target === img || img.contains(event.target)) {
                    // 添加更快速的回弹效果
                    img.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                    
                    // 重新启动呼吸效果之前，先执行回弹动画
                    setTimeout(() => {
                        startBreatheEffect();
                    }, 100);
                    
                    // 恢复正常过渡，延迟时间与动画时长匹配
                    setTimeout(() => {
                        img.style.transition = 'transform 0.2s ease-out';
                    }, 150);
                }
            } else {
                // 如果鼠标不在图片上，保持停止呼吸效果的状态
                img.style.transform = 'scale(1) perspective(1000px)';
            }
        });
    });
    
    // 为动漫图标添加特殊效果
    document.querySelectorAll('.anime-icon').forEach(icon => {
        icon.addEventListener('click', function() {
            // 创建临时粒子效果
            const particles = 12;
            const colors = icon.textContent === '86' ? 
                ['#7aa2f7', '#bb9af7', '#c0caf5'] : 
                ['#f7768e', '#ff9e64', '#e0af68'];
            
            for (let i = 0; i < particles; i++) {
                const particle = document.createElement('span');
                particle.className = 'anime-particle';
                const size = Math.floor(Math.random() * 8) + 4;
                particle.style.width = `${size}px`;
                particle.style.height = `${size}px`;
                particle.style.background = colors[Math.floor(Math.random() * colors.length)];
                
                // 随机定位和动画
                const angle = Math.random() * Math.PI * 2;
                const distance = Math.random() * 60 + 20;
                const duration = Math.random() * 0.6 + 0.4;
                const delay = Math.random() * 0.2;
                
                particle.style.position = 'absolute';
                particle.style.left = '50%';
                particle.style.top = '50%';
                particle.style.borderRadius = '50%';
                particle.style.transform = 'translate(-50%, -50%)';
                particle.style.pointerEvents = 'none';
                particle.style.zIndex = '10';
                particle.style.opacity = '0.8';
                
                icon.appendChild(particle);
                
                // 应用动画
                setTimeout(() => {
                    particle.style.transition = `all ${duration}s ease-out`;
                    particle.style.transform = `translate(
                        calc(-50% + ${Math.cos(angle) * distance}px), 
                        calc(-50% + ${Math.sin(angle) * distance}px)
                    )`;
                    particle.style.opacity = '0';
                }, delay * 1000);
                
                // 清除粒子
                setTimeout(() => {
                    if (icon.contains(particle)) {
                        icon.removeChild(particle);
                    }
                }, (duration + delay) * 1000);
            }
            
            // 添加弹跳效果
            this.style.transform = 'translateY(-3px) rotate(3deg) scale(1.1)';
            setTimeout(() => {
                this.style.transform = 'translateY(-3px) rotate(3deg)';
            }, 150);
        });
    });
    
    // 为头像添加点击魔法效果
    const authorAvatar = document.querySelector('.author-avatar img');
    if (authorAvatar) {
        authorAvatar.addEventListener('click', function() {
            const avatar = this;
            const originalTransform = avatar.style.transform;
            
            // 创建闪光效果
            const flash = document.createElement('div');
            flash.className = 'avatar-flash';
            flash.style.position = 'absolute';
            flash.style.top = '0';
            flash.style.left = '0';
            flash.style.width = '100%';
            flash.style.height = '100%';
            flash.style.borderRadius = '50%';
            flash.style.background = 'radial-gradient(circle, rgba(255,255,255,0.9) 0%, rgba(255,255,255,0) 70%)';
            flash.style.opacity = '0';
            flash.style.transition = 'all 0.4s ease-out';
            flash.style.zIndex = '5';
            
            avatar.parentNode.appendChild(flash);
            
            // 触发闪光动画
            setTimeout(() => {
                flash.style.opacity = '1';
                avatar.style.transform = `${originalTransform || ''} scale(1.15)`;
                avatar.style.borderColor = 'var(--accent-color)';
            }, 50);
            
            // 清除闪光效果
            setTimeout(() => {
                flash.style.opacity = '0';
                avatar.style.transform = originalTransform || '';
                avatar.style.borderColor = 'var(--primary-color)';
                
                setTimeout(() => {
                    if (avatar.parentNode.contains(flash)) {
                        avatar.parentNode.removeChild(flash);
                    }
                }, 400);
            }, 700);
        });
    }
});
