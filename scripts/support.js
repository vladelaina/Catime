// 支持项目的3D交互效果
document.addEventListener('DOMContentLoaded', function() {
    // AOS (滚动时动画) 库初始化
    AOS.init({
        duration: 800, // 动画持续时间
        once: true, // 动画是否只发生一次 - 在向下滚动时
        offset: 50, // 从原始触发点偏移的距离 (以像素为单位)
    });

    // 初始化滚动进度指示器
    initScrollProgressIndicator();

    // 为支持项目卡添加3D交互效果
    const supportMethods = document.querySelectorAll('.support-method');
    const isMobile = window.innerWidth <= 768; // 检测是否是移动设备
    
    supportMethods.forEach(card => {
        let rect = card.getBoundingClientRect();
        let centerX = (rect.left + rect.right) / 2;
        let centerY = (rect.top + rect.bottom) / 2;
        
        // 鼠标移动事件处理
        card.addEventListener('mousemove', function(e) {
            if (isMobile) return; // 在移动设备上不执行这些特效
            
            rect = card.getBoundingClientRect();
            centerX = (rect.left + rect.right) / 2;
            centerY = (rect.top + rect.bottom) / 2;
            
            // 计算鼠标与卡片中心的相对距离（-1到1的范围）
            const relativeX = (e.clientX - centerX) / (rect.width / 2);
            const relativeY = (e.clientY - centerY) / (rect.height / 2);
            
            // 应用旋转效果，最大±10度
            card.style.transform = `translateY(-20px) scale(1.03) rotateX(${-relativeY * 10}deg) rotateY(${relativeX * 10}deg)`;
            
            // 获取QR码和标签元素
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                // QR码沿Z轴移动更多，创造深度感
                qrCode.style.transform = `translateZ(40px) scale(1.08) rotate(${relativeX * 2}deg)`;
            }
            
            if (label) {
                // 标签轻微偏移跟随鼠标
                label.style.transform = `translateZ(25px) translateY(-5px) translateX(${relativeX * 5}px) scale(1.05)`;
            }
        });
        
        // 鼠标离开时重置效果
        card.addEventListener('mouseleave', function() {
            if (isMobile) return; // 在移动设备上不执行这些特效
            
            card.style.transform = '';
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                qrCode.style.transform = 'translateZ(20px)';
            }
            
            if (label) {
                label.style.transform = 'translateZ(10px)';
            }
            
            // 使用setTimeout添加过渡回原位的动画
            setTimeout(() => {
                card.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                qrCode.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                label.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
            }, 50);
        });
        
        // 鼠标进入时移除过渡效果，使移动更流畅
        card.addEventListener('mouseenter', function() {
            if (isMobile) return; // 在移动设备上不执行这些特效
            
            card.style.transition = 'none';
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                qrCode.style.transition = 'none';
            }
            
            if (label) {
                label.style.transition = 'none';
            }
        });
    });

    // 为其他支持卡片添加悬停效果
    const supportCards = document.querySelectorAll('.support-card');
    supportCards.forEach(card => {
        card.addEventListener('mouseenter', function() {
            const icon = this.querySelector('.support-icon i');
            if (icon) {
                icon.style.transition = 'transform 0.3s ease';
                icon.style.transform = 'scale(1.2)';
            }
        });
        
        card.addEventListener('mouseleave', function() {
            const icon = this.querySelector('.support-icon i');
            if (icon) {
                icon.style.transform = 'scale(1)';
            }
        });
    });

    // 添加心形动画交互
    const heartAnimation = document.querySelector('.heart-animation');
    if (heartAnimation) {
        heartAnimation.addEventListener('click', function() {
            const heart = this.querySelector('i');
            heart.style.animation = 'none';
            setTimeout(() => {
                heart.style.animation = 'heartbeat 1.5s infinite';
                this.classList.add('clicked');
                setTimeout(() => {
                    this.classList.remove('clicked');
                }, 1000);
            }, 10);
        });
    }

    // 初始化咖啡粒子效果
    initCoffeeParticles();
});

// 初始化咖啡粒子效果
function initCoffeeParticles() {
    const coffeeParticlesContainer = document.querySelector('.coffee-particles');
    if (!coffeeParticlesContainer) return;

    const coffeeParticles = coffeeParticlesContainer.querySelectorAll('.coffee-particle');
    
    // 为每个咖啡粒子分配随机的动画延迟和位置
    coffeeParticles.forEach(particle => {
        const delay = Math.random() * 5; // 0-5秒的随机延迟
        const duration = 15 + Math.random() * 10; // 15-25秒的随机持续时间
        const left = Math.random() * 100; // 0-100%的随机水平位置

        particle.style.animationDelay = `${delay}s`;
        particle.style.animationDuration = `${duration}s`;
        particle.style.left = `${left}%`;
    });
}

// 初始化滚动进度指示器
function initScrollProgressIndicator() {
    const scrollProgressContainer = document.getElementById('scrollProgressContainer');
    const scrollProgressCircle = document.querySelector('.scroll-progress-circle-fill');
    const scrollProgressPercentage = document.querySelector('.scroll-progress-percentage');

    if (!scrollProgressContainer || !scrollProgressCircle || !scrollProgressPercentage) return;

    // 窗口滚动时更新进度
    window.addEventListener('scroll', function() {
        updateScrollProgress();
    });

    // 点击滚动进度指示器返回顶部
    scrollProgressContainer.addEventListener('click', function() {
        // 添加点击效果
        this.classList.add('clicked');
        
        // 平滑滚动到顶部
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
        
        // 移除点击效果
        setTimeout(() => {
            this.classList.remove('clicked');
        }, 500);
    });

    // 初始化滚动进度
    updateScrollProgress();

    // 更新滚动进度函数
    function updateScrollProgress() {
        const scrollTop = window.scrollY;
        const scrollHeight = document.documentElement.scrollHeight - window.innerHeight;
        const scrollPercentage = (scrollTop / scrollHeight) * 100;
        
        // 更新圆形进度条
        const perimeter = Math.PI * 2 * 45; // 2πr，r=45
        const strokeDashoffset = perimeter * (1 - scrollPercentage / 100);
        scrollProgressCircle.style.strokeDashoffset = strokeDashoffset;
        
        // 更新百分比文本
        scrollProgressPercentage.textContent = `${Math.round(scrollPercentage)}%`;
        
        // 根据滚动位置切换容器可见性
        if (scrollTop > 300) {
            scrollProgressContainer.style.opacity = '1';
            scrollProgressContainer.style.transform = 'scale(1)';
            scrollProgressContainer.style.pointerEvents = 'auto';
        } else {
            scrollProgressContainer.style.opacity = '0';
            scrollProgressContainer.style.transform = 'scale(0.8)';
            scrollProgressContainer.style.pointerEvents = 'none';
        }
    }
} 