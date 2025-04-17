// AOS 初始化
document.addEventListener('DOMContentLoaded', function() {
    // 初始化 AOS 动画库
    AOS.init({
        duration: 800,
        once: true,
        offset: 50,
    });

    // 初始化滚动进度指示器
    initScrollProgressIndicator();

    // 为图片添加3D倾斜效果
    document.querySelectorAll('.animated-image img').forEach(img => {
        const container = img.closest('.animated-image');
        if (!container) return;
        
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
                    }, 150);
                }
            }
        });
    });
});

// 初始化滚动进度指示器
function initScrollProgressIndicator() {
    const scrollProgressContainer = document.getElementById('scrollProgressContainer');
    if (!scrollProgressContainer) return;

    const scrollProgressCircle = document.querySelector('.scroll-progress-circle-fill');
    const scrollProgressPercentage = document.querySelector('.scroll-progress-percentage');

    if (!scrollProgressCircle || !scrollProgressPercentage) return;

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
