// 支持项目的3D交互效果
document.addEventListener('DOMContentLoaded', function() {
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
});

// AOS (滚动时动画) 库初始化
AOS.init({
    duration: 800, // 动画持续时间
    once: true, // 动画是否只发生一次 - 在向下滚动时
    offset: 50, // 从原始触发点偏移的距离 (以像素为单位)
});

// 为展示图片添加3D倾斜效果
document.addEventListener('DOMContentLoaded', function() {
    const heroImage = document.querySelector('.hero-screenshot');
    const heroContainer = document.querySelector('.hero-visual');
    const scenarioImages = document.querySelectorAll('.scenario-img');
    const scenarioContainers = document.querySelectorAll('.scenario-visual');
    
    // 处理英雄图片
    if (heroImage && heroContainer) {
        // 最大倾斜角度
        const maxRotateX = 15; 
        const maxRotateY = 20;
        
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
                // 检测是否是移动设备
                const isMobile = window.innerWidth <= 480;
                
                // 根据设备类型使用不同的缩放比例
                let scale;
                if (isMobile) {
                    // 移动设备上使用更小的缩放效果
                    scale = 0.85 + Math.sin(phase) * 0.02;
                } else {
                    // 正弦波动的呼吸效果
                    scale = 1.55 + Math.sin(phase) * 0.03;
                }
                
                // 应用变换，结合当前的旋转角度
                heroImage.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                
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
        
        // 注意：不在一开始就启动呼吸效果
        
        heroContainer.addEventListener('mousemove', function(e) {
            // 获取鼠标在元素内的相对位置（0-1）
            const rect = heroContainer.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width;
            const y = (e.clientY - rect.top) / rect.height;
            
            // 计算倾斜角度（转换为-maxRotate到maxRotate的范围）
            currentRotateY = (x - 0.5) * 2 * maxRotateY;
            currentRotateX = (y - 0.5) * -2 * maxRotateX; // 负号让鼠标在顶部时，图片向上倾斜
        });
        
        // 鼠标离开时恢复原始状态
        heroContainer.addEventListener('mouseleave', function() {
            stopBreatheEffect(); // 停止呼吸效果
            currentRotateX = 0;
            currentRotateY = 0;
            
            // 检测是否是移动设备
            const isMobile = window.innerWidth <= 480;
            
            // 根据设备类型设置不同的初始缩放
            if (isMobile) {
                heroImage.style.transform = 'scale(0.85) perspective(1000px)';
            } else {
                // 直接设置回初始状态
                heroImage.style.transform = 'scale(1.5) perspective(1000px)';
            }
        });
        
        // 鼠标进入时准备变换并启动呼吸效果
        heroContainer.addEventListener('mouseenter', function() {
            heroImage.style.transition = 'transform 0.2s ease-out';
            startBreatheEffect(); // 启动呼吸效果
        });
        
        // 添加点击效果：按下和回弹
        heroImage.addEventListener('mousedown', function() {
            // 暂时停止呼吸效果
            stopBreatheEffect();
            
            // 检测是否是移动设备（屏幕宽度小于等于480px）
            const isMobile = window.innerWidth <= 480;
            
            // 按下效果 - 缩小并轻微下沉，使变化更加柔和
            // 在移动设备上使用更小的缩放比例
            if (isMobile) {
                heroImage.style.transform = `scale(0.8) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
            } else {
                heroImage.style.transform = `scale(1.48) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
            }
        });
        
        // 鼠标释放或离开时回弹
        document.addEventListener('mouseup', function(event) {
            if (heroContainer.matches(':hover')) {
                // 如果鼠标仍在图片上，回弹到悬停状态
                if (event.target === heroImage || heroImage.contains(event.target)) {
                    // 添加更快速的回弹效果
                    heroImage.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                    
                    // 重新启动呼吸效果之前，先执行回弹动画，进一步缩短延迟时间
                    setTimeout(() => {
                        // 这里不设置transform，让呼吸效果接管
                        startBreatheEffect();
                    }, 100);
                    
                    // 恢复正常过渡，延迟时间与动画时长匹配
                    setTimeout(() => {
                        heroImage.style.transition = 'transform 0.2s ease-out';
                    }, 150);
                }
            } else {
                // 检测是否是移动设备
                const isMobile = window.innerWidth <= 480;
                
                // 如果鼠标不在图片上，保持停止呼吸效果的状态
                heroImage.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                
                // 在移动设备上使用更小的缩放比例
                if (isMobile) {
                    heroImage.style.transform = 'scale(0.85) perspective(1000px)';
                } else {
                    heroImage.style.transform = 'scale(1.5) perspective(1000px)';
                }
                
                // 恢复正常过渡，延迟时间与动画时长匹配
                setTimeout(() => {
                    heroImage.style.transition = 'transform 0.2s ease-out';
                }, 150);
            }
        });
    }
    
    // 为场景图片添加相同的3D倾斜效果
    if (scenarioImages.length > 0 && scenarioContainers.length > 0) {
        // 为每个场景图片设置动画效果
        scenarioContainers.forEach((container, index) => {
            const image = scenarioImages[index];
            
            // 最大倾斜角度
            const maxRotateX = 12;
            const maxRotateY = 18;
            
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
                    // 添加缩放效果，类似于hero图片的呼吸效果
                    const scale = 1.02 + Math.sin(phase) * 0.02;
                    
                    // 应用变换，结合当前的旋转角度和缩放效果
                    image.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                    
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
                image.style.transform = 'scale(1) perspective(1000px)';
            });
            
            // 鼠标进入时准备变换并启动呼吸效果
            container.addEventListener('mouseenter', function() {
                image.style.transition = 'transform 0.2s ease-out';
                startBreatheEffect(); // 启动呼吸效果
            });
            
            // 添加点击效果：按下和回弹
            image.addEventListener('mousedown', function() {
                // 暂时停止呼吸效果
                stopBreatheEffect();
                // 按下效果 - 只添加轻微下沉，不缩放
                image.style.transform = `scale(0.98) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
            });
            
            // 单独为每个场景图片添加鼠标释放事件
            image.addEventListener('mouseup', function() {
                // 添加更快速的回弹效果
                image.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                
                // 重新启动呼吸效果之前，先执行回弹动画
                setTimeout(() => {
                    startBreatheEffect();
                }, 100);
                
                // 恢复正常过渡，延迟时间与动画时长匹配
                setTimeout(() => {
                    image.style.transition = 'transform 0.2s ease-out';
                }, 150);
            });
        });
    }
});

// Star History Chart
document.addEventListener('DOMContentLoaded', function() {
    // 检查是否存在图表容器
    const chartContainer = document.getElementById('star-history-chart');
    if (!chartContainer) return;
    
    // 初始化 ECharts 实例
    const chart = echarts.init(chartContainer);
    
    // 配置加载状态
    chart.showLoading({
        text: '加载 GitHub 星星数据中...',
        color: '#7aa2f7',
        textColor: '#414868',
        maskColor: 'rgba(255, 255, 255, 0.8)',
    });
    
    // 首先显示模拟数据，提供即时反馈
    renderFallbackChart(chart, false);
    
    // 为GitHub星星图表添加3D交互效果
    const githubChartContainer = document.querySelector('.github-chart-container');
    if (githubChartContainer) {
        let isMouseOver = false;
        let mouseX = 0, mouseY = 0;
        let rafId = null;
        
        // 鼠标移动事件
        githubChartContainer.addEventListener('mousemove', function(e) {
            if (!isMouseOver) return;
            
            // 获取鼠标在容器内的相对位置（-0.5到0.5的范围）
            const rect = githubChartContainer.getBoundingClientRect();
            mouseX = ((e.clientX - rect.left) / rect.width) - 0.5;
            mouseY = ((e.clientY - rect.top) / rect.height) - 0.5;
            
            // 如果没有动画帧请求，则添加一个
            if (!rafId) {
                rafId = requestAnimationFrame(updateTransform);
            }
            
            // 随机创建跟随鼠标的彗星粒子
            if (Math.random() > 0.92) { // 控制生成频率
                createCometParticle(e.clientX - rect.left, e.clientY - rect.top);
            }
        });
        
        // 更新3D变换
        function updateTransform() {
            // 计算旋转角度（最大±10度）
            const rotateY = mouseX * 5;
            const rotateX = -mouseY * 5;
            
            // 应用变换
            githubChartContainer.style.transform = 
                `translateY(-10px) scale(1.02) perspective(1000px) rotateX(${rotateX}deg) rotateY(${rotateY}deg)`;
            
            // 添加阴影方向跟随
            const shadowX = mouseX * 20;
            const shadowY = mouseY * 20;
            githubChartContainer.style.boxShadow = 
                `${-shadowX}px ${-shadowY}px 30px rgba(122, 162, 247, 0.3), 0 10px 20px rgba(0,0,0,0.1)`;
            
            // 重置动画帧请求ID
            rafId = null;
            
            // 如果鼠标仍在容器上，则请求下一帧
            if (isMouseOver) {
                rafId = requestAnimationFrame(updateTransform);
            }
        }
        
        // 鼠标进入事件
        githubChartContainer.addEventListener('mouseenter', function() {
            isMouseOver = true;
            githubChartContainer.style.transition = 'transform 0.3s ease, box-shadow 0.3s ease';
            rafId = requestAnimationFrame(updateTransform);
            
            // 当图表容器被悬停时，更新ECharts图表大小以确保正确渲染
            chart.resize();
            
            // 创建星星粒子
            createStarParticles();
        });
        
        // 创建星星粒子效果
        function createStarParticles() {
            const starParticlesContainer = githubChartContainer.querySelector('.star-particles');
            if (!starParticlesContainer || starParticlesContainer.children.length > 0) return;
            
            // 增加粒子数量到30个
            for (let i = 0; i < 30; i++) {
                const particle = document.createElement('div');
                particle.className = 'star-particle';
                
                // 随机位置 - 更广泛的分布
                const x = Math.random() * 100;
                const y = Math.random() * 100;
                particle.style.left = `${x}%`;
                particle.style.bottom = `${y}%`;
                
                // 随机大小 - 增加大小范围
                const size = Math.random() * 4 + 1;
                particle.style.width = `${size}px`;
                particle.style.height = `${size}px`;
                
                // 随机不透明度
                const baseOpacity = Math.random() * 0.5 + 0.5;
                particle.style.opacity = baseOpacity;
                
                // 随机延迟 - 更宽的延迟范围
                const delay = Math.random() * 5;
                particle.style.animationDelay = `${delay}s`;
                
                // 随机持续时间 - 更多变化
                const duration = Math.random() * 3 + 2;
                particle.style.animationDuration = `${duration}s`;
                
                // 添加随机旋转
                if (Math.random() > 0.7) {
                    particle.style.transform = `rotate(${Math.random() * 360}deg)`;
                }
                
                // 随机星星形状 - 有10%概率创建特殊形状
                if (Math.random() > 0.9) {
                    // 创建星形粒子
                    particle.style.clipPath = 'polygon(50% 0%, 61% 35%, 98% 35%, 68% 57%, 79% 91%, 50% 70%, 21% 91%, 32% 57%, 2% 35%, 39% 35%)';
                    particle.style.width = `${size * 3}px`;
                    particle.style.height = `${size * 3}px`;
                }
                
                // 添加到容器
                starParticlesContainer.appendChild(particle);
            }
        }
        
        // 鼠标离开事件
        githubChartContainer.addEventListener('mouseleave', function() {
            isMouseOver = false;
            
            // 重置变换和阴影
            githubChartContainer.style.transition = 'transform 0.5s ease, box-shadow 0.5s ease';
            githubChartContainer.style.transform = '';
            githubChartContainer.style.boxShadow = '';
            
            // 取消动画帧请求
            if (rafId) {
                cancelAnimationFrame(rafId);
                rafId = null;
            }
        });
        
        // 点击效果
        githubChartContainer.addEventListener('mousedown', function() {
            // 按下效果 - 缩小并降低高度
            githubChartContainer.style.transition = 'transform 0.1s ease';
            githubChartContainer.style.transform = 
                `translateY(-5px) scale(1.01) perspective(1000px) rotateX(${-mouseY * 3}deg) rotateY(${mouseX * 3}deg)`;
        });
        
        // 释放效果
        githubChartContainer.addEventListener('mouseup', function() {
            // 回弹效果
            githubChartContainer.style.transition = 'transform 0.2s cubic-bezier(0.34, 1.56, 0.64, 1)';
            if (isMouseOver) {
                updateTransform();
            }
        });
    }
    
    // 然后在后台获取真实数据
    fetch('https://api.star-history.com/svg?repos=vladelaina/Catime&type=Date')
        .then(response => response.text())
        .then(svgData => {
            // 解析 SVG 数据以提取星星历史
            const parser = new DOMParser();
            const svgDoc = parser.parseFromString(svgData, 'image/svg+xml');
            
            try {
                // 提取日期和星星数
                const paths = svgDoc.querySelectorAll('path[d]');
                const dataPath = Array.from(paths).find(path => 
                    path.getAttribute('d').includes('M') && 
                    path.getAttribute('d').includes('L') && 
                    !path.getAttribute('d').includes('H')
                );
                
                if (!dataPath) throw new Error('数据路径未找到');
                
                const data = dataPath.getAttribute('d')
                    .split('M')[1]
                    .split('L')
                    .map(point => {
                        const [x, y] = point.trim().split(',').map(parseFloat);
                        return [x, y];
                    });
                
                // 获取 X 轴和 Y 轴范围
                const xRange = svgDoc.querySelector('.x').textContent;
                const yRange = svgDoc.querySelector('.y').textContent;
                
                // 解析日期范围
                const dateMatch = xRange.match(/(\d{4}-\d{2})/g);
                if (!dateMatch || dateMatch.length < 2) throw new Error('日期范围未找到');
                
                const startDate = new Date(dateMatch[0]);
                const endDate = new Date(dateMatch[dateMatch.length - 1]);
                const totalDays = (endDate - startDate) / (1000 * 60 * 60 * 24);
                
                // 解析星星数范围
                const starsMatch = yRange.match(/\d+/g);
                if (!starsMatch || starsMatch.length < 2) throw new Error('星星数范围未找到');
                
                const maxStars = parseInt(starsMatch[starsMatch.length - 1]);
                
                // 转换数据为 ECharts 格式
                const chartData = data.map(([x, y]) => {
                    // 将 x 从 SVG 坐标转换为日期
                    const dateOffset = x / svgDoc.querySelector('svg').getAttribute('width') * totalDays;
                    const date = new Date(startDate);
                    date.setDate(date.getDate() + dateOffset);
                    
                    // 格式化日期为 YYYY-MM-DD
                    const formattedDate = date.toISOString().split('T')[0];
                    
                    // 将 y 从 SVG 坐标转换为星星数
                    const height = svgDoc.querySelector('svg').getAttribute('height');
                    const stars = Math.round(maxStars * (1 - y / height));
                    
                    return [formattedDate, stars];
                });
                
                // 配置图表选项
                const option = {
                    title: {
                        text: 'Catime GitHub 星星增长历史',
                        left: 'center',
                        top: 10,
                        textStyle: {
                            color: '#414868'
                        }
                    },
                    tooltip: {
                        trigger: 'axis',
                        formatter: function(params) {
                            const date = new Date(params[0].value[0]);
                            const formattedDate = date.toISOString().split('T')[0];
                            return `${formattedDate}: ${params[0].value[1]} 星星`;
                        }
                    },
                    grid: {
                        left: '5%',
                        right: '5%',
                        bottom: '15%',
                        top: '15%',
                        containLabel: true
                    },
                    xAxis: {
                        type: 'time',
                        name: '',
                        nameLocation: 'middle',
                        nameGap: 35,
                        axisLabel: {
                            formatter: '{yyyy}-{MM}-{dd}',
                            rotate: 30,
                            interval: function(index, value) {
                                // 每周只显示一个标签
                                const date = new Date(value);
                                return date.getDay() === 1; // 每周一显示
                            },
                            margin: 10
                        },
                        splitLine: {
                            show: false
                        }
                    },
                    yAxis: {
                        type: 'value',
                        name: '星星数',
                        nameRotate: 90,
                        nameLocation: 'middle',
                        nameGap: 50,
                        max: 1200,
                        min: 0,
                        interval: 200,
                        splitNumber: 6
                    },
                    series: [{
                        name: 'GitHub 星星',
                        type: 'line',
                        smooth: true,
                        symbol: 'circle',
                        symbolSize: 8,
                        sampling: 'average',
                        itemStyle: {
                            color: '#7aa2f7'
                        },
                        areaStyle: {
                            color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                                { offset: 0, color: 'rgba(122, 162, 247, 0.5)' },
                                { offset: 1, color: 'rgba(122, 162, 247, 0.1)' }
                            ])
                        },
                        data: chartData.map(item => {
                            return {
                                name: item[0],
                                value: [item[0], item[1]]
                            };
                        })
                    }]
                };
                
                // 应用配置并隐藏加载状态
                chart.hideLoading();
                chart.setOption(option);
                
            } catch (error) {
                console.error('解析 Star History SVG 错误:', error);
            }
        })
        .catch(error => {
            console.error('获取 Star History 数据错误:', error);
        });
        
    // 备用图表
    function renderFallbackChart(chart, showFallbackTitle = true) {
        chart.hideLoading();
        
        // 模拟数据 - 使用用户提供的准确日期
        const dateData = [
            ['2025-02-02', 0],
            ['2025-03-08', 120],
            ['2025-03-16', 390],
            ['2025-03-23', 570],
            ['2025-03-25', 750],
            ['2025-03-29', 930],
            ['2025-04-01', 1110],
            ['2025-04-07', 1330]
        ];
        
        // 备用图表配置
        const option = {
            title: {
                text: showFallbackTitle ? 'Catime GitHub 星星增长历史 (估计数据)' : 'Catime GitHub 星星增长历史',
                left: 'center',
                textStyle: {
                    color: '#414868'
                }
            },
            tooltip: {
                trigger: 'axis',
                formatter: function(params) {
                    const date = new Date(params[0].value[0]);
                    const formattedDate = date.toISOString().split('T')[0];
                    return `${formattedDate}: ${params[0].value[1]} 星星`;
                }
            },
            grid: {
                left: '5%',
                right: '5%',
                bottom: '15%',
                top: '15%',
                containLabel: true
            },
            xAxis: {
                type: 'time',
                name: '',
                nameLocation: 'middle',
                nameGap: 35,
                axisLabel: {
                    formatter: '{yyyy}-{MM}-{dd}',
                    rotate: 30,
                    interval: function(index, value) {
                        // 每周只显示一个标签
                        const date = new Date(value);
                        return date.getDay() === 1; // 每周一显示
                    },
                    margin: 10
                },
                splitLine: {
                    show: false
                }
            },
            yAxis: {
                type: 'value',
                name: '星星数',
                nameRotate: 90,
                nameLocation: 'middle',
                nameGap: 50,
                splitNumber: 6
            },
            series: [{
                name: 'GitHub 星星',
                type: 'line',
                smooth: true,
                symbol: 'circle',
                symbolSize: 8,
                itemStyle: {
                    color: '#7aa2f7'
                },
                areaStyle: {
                    color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                        { offset: 0, color: 'rgba(122, 162, 247, 0.5)' },
                        { offset: 1, color: 'rgba(122, 162, 247, 0.1)' }
                    ])
                },
                data: dateData.map(item => {
                    return {
                        name: item[0],
                        value: [item[0], item[1]]
                    };
                })
            }]
        };
        
        chart.setOption(option);
    }
    
    // 响应窗口调整大小
    window.addEventListener('resize', function() {
        chart.resize();
    });
});

// 创建彗星效果
function createCometParticle(x, y) {
    const starParticlesContainer = githubChartContainer.querySelector('.star-particles');
    if (!starParticlesContainer) return;
    
    const comet = document.createElement('div');
    comet.className = 'star-particle comet';
    
    // 设置彗星位置为鼠标位置
    comet.style.left = `${x}px`;
    comet.style.top = `${y}px`;
    
    // 随机方向角度
    const angle = Math.random() * 360;
    const distance = 50 + Math.random() * 100; // 移动距离
    
    // 添加到容器
    starParticlesContainer.appendChild(comet);
    
    // 在下一帧应用变换
    requestAnimationFrame(() => {
        // 根据角度计算新位置
        const rad = angle * Math.PI / 180;
        const moveX = Math.cos(rad) * distance;
        const moveY = Math.sin(rad) * distance;
        
        // 设置移动和缩放效果
        comet.style.transform = `translate(${moveX}px, ${moveY}px) scale(0.1)`;
        comet.style.opacity = '0';
    });
    
    // 移除彗星元素
    setTimeout(() => {
        if (comet.parentNode === starParticlesContainer) {
            starParticlesContainer.removeChild(comet);
        }
    }, 1500);
}

// 滚动进度指示器脚本
document.addEventListener('DOMContentLoaded', function() {
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
});

// 添加贡献者动效
document.addEventListener('DOMContentLoaded', function() {
    // 为所有贡献者头像添加动效容器
    const contributorCells = document.querySelectorAll('.contributors-grid td');
    
    contributorCells.forEach(cell => {
        // 检查是否已经有贡献者容器
        const link = cell.querySelector('a');
        if (!link) return;
        
        // 如果没有现代结构，就添加它
        const img = link.querySelector('img');
        if (img && !link.querySelector('.contributor-avatar-container')) {
            // 保存原始HTML以获取图片和姓名
            const imgSrc = img.getAttribute('src');
            const imgAlt = img.getAttribute('alt');
            const nameElement = link.querySelector('sub');
            const name = nameElement ? nameElement.innerHTML : '';
            
            // 创建新结构
            const newHTML = `
                <div class="contributor-avatar-container">
                    <div class="contributor-avatar-glow"></div>
                    <img src="${imgSrc}" width="100px;" alt="${imgAlt}"/>
                </div>
                <div class="contributor-particles">
                    <span class="contributor-particle" style="--tx: -${Math.random() * 30 + 10}; --ty: -${Math.random() * 25 + 5}; background-color: ${Math.random() > 0.5 ? '#7aa2f7' : '#f77daa'};"></span>
                    <span class="contributor-particle" style="--tx: ${Math.random() * 30 + 10}; --ty: -${Math.random() * 25 + 5}; background-color: ${Math.random() > 0.5 ? '#7aa2f7' : '#f77daa'};"></span>
                    <span class="contributor-particle" style="--tx: ${Math.random() * 25 + 5}; --ty: ${Math.random() * 25 + 5}; background-color: ${Math.random() > 0.5 ? '#7aa2f7' : '#f77daa'};"></span>
                    <span class="contributor-particle" style="--tx: -${Math.random() * 25 + 5}; --ty: ${Math.random() * 25 + 5}; background-color: ${Math.random() > 0.5 ? '#7aa2f7' : '#f77daa'};"></span>
                </div>
                ${nameElement ? '<sub>' + name + '</sub>' : ''}
            `;
            
            // 替换链接内容
            if (nameElement) nameElement.remove();
            img.remove();
            link.innerHTML = newHTML;
        }
    });
    
    // 添加鼠标移动交互效果
    contributorCells.forEach(cell => {
        cell.addEventListener('mousemove', function(e) {
            const avatar = cell.querySelector('img');
            if (!avatar) return;
            
            const rect = cell.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width - 0.5; // -0.5 to 0.5
            const y = (e.clientY - rect.top) / rect.height - 0.5; // -0.5 to 0.5
            
            // 应用3D倾斜效果
            avatar.style.transform = `scale(1.1) rotateY(${x * 20}deg) rotateX(${-y * 20}deg)`;
            
            // 添加发光效果的动态位置
            const glow = cell.querySelector('.contributor-avatar-glow');
            if (glow) {
                glow.style.background = ''; // 不设置背景，保持透明
            }
        });
        
        // 鼠标离开时重置效果
        cell.addEventListener('mouseleave', function() {
            const avatar = cell.querySelector('img');
            if (avatar) {
                avatar.style.transform = '';
            }
            
            const glow = cell.querySelector('.contributor-avatar-glow');
            if (glow) {
                glow.style.background = '';
            }
        });
    });

    // 处理移动端贡献者列表布局
    function adjustContributorsLayout() {
        if (window.innerWidth <= 768) {
            const contributorsTable = document.querySelector('.contributors-grid table');
            const contributorsTbody = document.querySelector('.contributors-grid tbody');
            const allCells = Array.from(document.querySelectorAll('.contributors-grid td'));
            
            // 如果没有表格，直接返回
            if (!contributorsTable || !contributorsTbody || allCells.length === 0) return;
            
            // 保存原始单元格数组
            const cellsCopy = allCells.map(cell => cell.cloneNode(true));
            
            // 删除所有现有行
            while (contributorsTbody.firstChild) {
                contributorsTbody.removeChild(contributorsTbody.firstChild);
            }
            
            // 根据屏幕宽度决定每行显示的单元格数量
            const cellsPerRow = window.innerWidth <= 480 ? 2 : 3;
            
            // 创建新的行并平均分配单元格
            for (let i = 0; i < cellsCopy.length; i += cellsPerRow) {
                const newRow = document.createElement('tr');
                newRow.style.display = 'flex';
                newRow.style.justifyContent = 'center';
                newRow.style.width = '100%';
                newRow.style.marginBottom = '10px';
                
                // 计算当前行要添加的单元格数量（不超过总数）
                const cellsToAdd = Math.min(cellsPerRow, cellsCopy.length - i);
                
                // 如果是最后一行且单元格数量不足一整行，调整样式使其居中
                const isLastRow = i + cellsPerRow >= cellsCopy.length;
                const isCellsNotFull = cellsToAdd < cellsPerRow;
                
                if (isLastRow && isCellsNotFull) {
                    // 添加左边距，使最后一行居中显示
                    newRow.style.justifyContent = 'center';
                    
                    // 如果只有一个单元格在最后一行
                    if (cellsToAdd === 1) {
                        // 特殊处理最后一行只有一个单元格的情况
                        const emptyCell = document.createElement('td');
                        emptyCell.style.visibility = 'hidden';
                        emptyCell.style.flex = '0 0 0';
                        
                        if (window.innerWidth <= 480) {
                            // 为最后一行的单个贡献者应用合适的样式
                            newRow.style.justifyContent = 'center';
                        }
                    }
                }
                
                // 添加当前行的单元格
                for (let j = 0; j < cellsToAdd; j++) {
                    const cell = cellsCopy[i + j];
                    
                    // 设置单元格样式
                    if (window.innerWidth <= 480) {
                        cell.style.flex = '0 0 calc(50% - 10px)';
                        cell.style.maxWidth = 'calc(50% - 10px)';
                        cell.style.margin = '0 5px';
                    } else {
                        cell.style.flex = '0 0 calc(33.33% - 16px)';
                        cell.style.maxWidth = 'calc(33.33% - 16px)';
                        cell.style.margin = '0 8px';
                    }
                    
                    newRow.appendChild(cell);
                }
                
                contributorsTbody.appendChild(newRow);
            }
        }
    }
    
    // 初始调整
    setTimeout(adjustContributorsLayout, 100);
    
    // 窗口大小改变时重新调整
    window.addEventListener('resize', adjustContributorsLayout);
});