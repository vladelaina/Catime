document.addEventListener('DOMContentLoaded', function() {
    const supportMethods = document.querySelectorAll('.support-method');
    const isMobile = window.innerWidth <= 768; 
    
    supportMethods.forEach(card => {
        let rect = card.getBoundingClientRect();
        let centerX = (rect.left + rect.right) / 2;
        let centerY = (rect.top + rect.bottom) / 2;
        
        card.addEventListener('mousemove', function(e) {
            if (isMobile) return; 
            
            rect = card.getBoundingClientRect();
            centerX = (rect.left + rect.right) / 2;
            centerY = (rect.top + rect.bottom) / 2;
            
            const relativeX = (e.clientX - centerX) / (rect.width / 2);
            const relativeY = (e.clientY - centerY) / (rect.height / 2);
            
            card.style.transform = `translateY(-20px) scale(1.03) rotateX(${-relativeY * 10}deg) rotateY(${relativeX * 10}deg)`;
            
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                qrCode.style.transform = `translateZ(40px) scale(1.08) rotate(${relativeX * 2}deg)`;
            }
            
            if (label) {
                label.style.transform = `translateZ(25px) translateY(-5px) translateX(${relativeX * 5}px) scale(1.05)`;
            }
        });
        
        card.addEventListener('mouseleave', function() {
            if (isMobile) return; 
            
            card.style.transform = '';
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                qrCode.style.transform = 'translateZ(20px)';
            }
            
            if (label) {
                label.style.transform = 'translateZ(10px)';
            }
            
            setTimeout(() => {
                card.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                qrCode.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                label.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
            }, 50);
        });
        
        card.addEventListener('mouseenter', function() {
            if (isMobile) return; 
            
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

AOS.init({
    duration: 800,
    once: true,
    offset: 50,
});

document.addEventListener('DOMContentLoaded', function() {
    const heroImage = document.querySelector('.hero-screenshot');
    const heroContainer = document.querySelector('.hero-visual');
    const scenarioImages = document.querySelectorAll('.scenario-img');
    const scenarioContainers = document.querySelectorAll('.scenario-visual');
    
    if (heroImage && heroContainer) {
        const maxRotateX = 15; 
        const maxRotateY = 20;
        
        let breatheTimer = null;
        
        let currentRotateX = 0;
        let currentRotateY = 0;
        
        function startBreatheEffect() {
            if (breatheTimer) return; 
            
            let phase = 0;
            breatheTimer = setInterval(() => {
                const isMobile = window.innerWidth <= 480;
                
                let scale;
                if (isMobile) {
                    scale = 0.85 + Math.sin(phase) * 0.02;
                } else {
                    scale = 1.55 + Math.sin(phase) * 0.03;
                }
                
                heroImage.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                
                phase += 0.05;
            }, 30);
        }

        function stopBreatheEffect() {
            if (breatheTimer) {
                clearInterval(breatheTimer);
                breatheTimer = null;
            }
        }
        
        heroContainer.addEventListener('mousemove', function(e) {
            const rect = heroContainer.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width;
            const y = (e.clientY - rect.top) / rect.height;
            
            currentRotateY = (x - 0.5) * 2 * maxRotateY;
            currentRotateX = (y - 0.5) * -2 * maxRotateX;
        });
        
        heroContainer.addEventListener('mouseleave', function() {
            stopBreatheEffect(); 
            currentRotateX = 0;
            currentRotateY = 0;
            
            const isMobile = window.innerWidth <= 480;
            
            if (isMobile) {
                heroImage.style.transform = 'scale(0.85) perspective(1000px)';
            } else {
                heroImage.style.transform = 'scale(1.5) perspective(1000px)';
            }
        });
        
        heroContainer.addEventListener('mouseenter', function() {
            heroImage.style.transition = 'transform 0.2s ease-out';
            startBreatheEffect();
        });
        
        heroImage.addEventListener('mousedown', function() {
            stopBreatheEffect();
            
            const isMobile = window.innerWidth <= 480;
            
            if (isMobile) {
                heroImage.style.transform = `scale(0.8) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
            } else {
                heroImage.style.transform = `scale(1.48) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
            }
        });
        
        document.addEventListener('mouseup', function(event) {
            if (heroContainer.matches(':hover')) {
                if (event.target === heroImage || heroImage.contains(event.target)) {
                    heroImage.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                    
                    setTimeout(() => {
                        startBreatheEffect();
                    }, 100);
                    
                    setTimeout(() => {
                        heroImage.style.transition = 'transform 0.2s ease-out';
                    }, 150);
                }
            } else {
                const isMobile = window.innerWidth <= 480;
                
                heroImage.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                
                if (isMobile) {
                    heroImage.style.transform = 'scale(0.85) perspective(1000px)';
                } else {
                    heroImage.style.transform = 'scale(1.5) perspective(1000px)';
                }
                
                setTimeout(() => {
                    heroImage.style.transition = 'transform 0.2s ease-out';
                }, 150);
            }
        });
    }
    
    if (scenarioImages.length > 0 && scenarioContainers.length > 0) {
        scenarioContainers.forEach((container, index) => {
            const image = scenarioImages[index];
            
            const maxRotateX = 12;
            const maxRotateY = 18;
            
            let breatheTimer = null;
            
            let currentRotateX = 0;
            let currentRotateY = 0;
            
            function startBreatheEffect() {
                if (breatheTimer) return; 
                
                let phase = 0;
                breatheTimer = setInterval(() => {
                    const scale = 1.02 + Math.sin(phase) * 0.02;
                    
                    image.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                            
                    phase += 0.05;
                }, 30);
            }

            function stopBreatheEffect() {
                if (breatheTimer) {
                    clearInterval(breatheTimer);
                    breatheTimer = null;
                }
            }
                    
            container.addEventListener('mousemove', function(e) {
                const rect = container.getBoundingClientRect();
                const x = (e.clientX - rect.left) / rect.width;
                const y = (e.clientY - rect.top) / rect.height;
                        
                currentRotateY = (x - 0.5) * 2 * maxRotateY;
                currentRotateX = (y - 0.5) * -2 * maxRotateX;
            });
                    
            container.addEventListener('mouseleave', function() {
                stopBreatheEffect();
                currentRotateX = 0;
                currentRotateY = 0;
                image.style.transform = 'scale(1) perspective(1000px)';
            });
                    
            container.addEventListener('mouseenter', function() {
                image.style.transition = 'transform 0.2s ease-out';
                startBreatheEffect();
            });
    const chartContainer = document.getElementById('star-history-chart');
    if (!chartContainer) return;
    
    const chart = echarts.init(chartContainer);
    
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    
    chart.showLoading({
        text: currentLang === 'en' ? 'Loading GitHub stars data...' : '加载 GitHub 星星数据中...',
        color: '#7aa2f7',
        textColor: '#414868',
        maskColor: 'rgba(255, 255, 255, 0.8)',
    });
    
    renderFallbackChart(chart, false, currentLang);
    
    const githubChartContainer = document.querySelector('.github-chart-container');
    if (githubChartContainer) {
        let isMouseOver = false;
        let mouseX = 0, mouseY = 0;
        let rafId = null;
        
        githubChartContainer.addEventListener('mousemove', function(e) {
            if (!isMouseOver) return;
            
            const rect = githubChartContainer.getBoundingClientRect();
            mouseX = ((e.clientX - rect.left) / rect.width) - 0.5;
            mouseY = ((e.clientY - rect.top) / rect.height) - 0.5;
            
            if (!rafId) {
                rafId = requestAnimationFrame(updateTransform);
            }
            
            if (Math.random() > 0.92) { 
                createCometParticle(e.clientX - rect.left, e.clientY - rect.top);
            }
        });
        
        function updateTransform() {
            const rotateY = mouseX * 5;
            const rotateX = -mouseY * 5;
            
            githubChartContainer.style.transform = 
                `translateY(-10px) scale(1.02) perspective(1000px) rotateX(${rotateX}deg) rotateY(${rotateY}deg)`;
            
            const shadowX = mouseX * 20;
            const shadowY = mouseY * 20;
            githubChartContainer.style.boxShadow = 
                `${-shadowX}px ${-shadowY}px 30px rgba(122, 162, 247, 0.3), 0 10px 20px rgba(0,0,0,0.1)`;
            
            rafId = null;
            
            if (isMouseOver) {
                rafId = requestAnimationFrame(updateTransform);
            }
        }
        
        githubChartContainer.addEventListener('mouseenter', function() {
            isMouseOver = true;
            githubChartContainer.style.transition = 'transform 0.3s ease, box-shadow 0.3s ease';
            rafId = requestAnimationFrame(updateTransform);
            
            chart.resize();
            
            createStarParticles();
        });
        
        function createStarParticles() {
            const starParticlesContainer = githubChartContainer.querySelector('.star-particles');
            if (!starParticlesContainer || starParticlesContainer.children.length > 0) return;
            
            for (let i = 0; i < 30; i++) {
                const particle = document.createElement('div');
                particle.className = 'star-particle';
                
                const x = Math.random() * 100;
                const y = Math.random() * 100;
                particle.style.left = `${x}%`;
                particle.style.bottom = `${y}%`;
                
                const size = Math.random() * 4 + 1;
                particle.style.width = `${size}px`;
                particle.style.height = `${size}px`;
                
                const baseOpacity = Math.random() * 0.5 + 0.5;
                particle.style.opacity = baseOpacity;
                
                const delay = Math.random() * 5;
                particle.style.animationDelay = `${delay}s`;
                
                const duration = Math.random() * 3 + 2;
                particle.style.animationDuration = `${duration}s`;
                
                if (Math.random() > 0.7) {
                    particle.style.transform = `rotate(${Math.random() * 360}deg)`;
                }
                
                if (Math.random() > 0.9) {
                    particle.style.clipPath = 'polygon(50% 0%, 61% 35%, 98% 35%, 68% 57%, 79% 91%, 50% 70%, 21% 91%, 32% 57%, 2% 35%, 39% 35%)';
                    particle.style.width = `${size * 3}px`;
                    particle.style.height = `${size * 3}px`;
                }
                
                starParticlesContainer.appendChild(particle);
            }
        }
        
        githubChartContainer.addEventListener('mouseleave', function() {
            isMouseOver = false;
            
            githubChartContainer.style.transition = 'transform 0.5s ease, box-shadow 0.5s ease';
            githubChartContainer.style.transform = '';
            githubChartContainer.style.boxShadow = '';
            
            if (rafId) {
                cancelAnimationFrame(rafId);
                rafId = null;
            }
        });
        
        githubChartContainer.addEventListener('mousedown', function() {
            githubChartContainer.style.transition = 'transform 0.1s ease';
            githubChartContainer.style.transform = 
                `translateY(-5px) scale(1.01) perspective(1000px) rotateX(${-mouseY * 3}deg) rotateY(${mouseX * 3}deg)`;
        });
        
        githubChartContainer.addEventListener('mouseup', function() {
            githubChartContainer.style.transition = 'transform 0.2s cubic-bezier(0.34, 1.56, 0.64, 1)';
            if (isMouseOver) {
                updateTransform();
            }
        });
    }
    
    function renderFallbackChart(chart, showFallbackTitle = true, lang = 'zh') {
        chart.hideLoading();
        
        const dateData = [
            ['2025-02-02', 0],
            ['2025-03-09', 210],
            ['2025-03-15', 330],
            ['2025-03-20', 480],
            ['2025-03-23', 600],
            ['2025-03-26', 750],
            ['2025-03-28', 870],
            ['2025-04-01', 1020],
            ['2025-04-02', 1140],
            ['2025-04-08', 1290],
            ['2025-04-16', 1410],
            ['2025-04-28', 1560],
            ['2025-05-02', 1680],
            ['2025-05-09', 1830],
            ['2025-05-15', 1950],
            ['2025-05-17', 2006] 
        ];
        
        const isEnglish = lang === 'en';
        const chartTitle = isEnglish 
            ? (showFallbackTitle ? 'Catime GitHub Star Growth History (Estimated Data)' : 'Catime GitHub Star Growth History')
            : (showFallbackTitle ? 'Catime GitHub 星星增长历史 (估计数据)' : 'Catime GitHub 星星增长历史');
        
        const starLabel = isEnglish ? 'Stars' : '星星数';
        const seriesName = isEnglish ? 'GitHub Stars' : 'GitHub 星星';
        
        const option = {
            title: {
                text: chartTitle,
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
                    return isEnglish 
                        ? `${formattedDate}: ${params[0].value[1]} stars` 
                        : `${formattedDate}: ${params[0].value[1]} 星星`;
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
                        const date = new Date(value);
                        return date.getDay() === 1; 
                    },
                    margin: 10
                },
                splitLine: {
                    show: false
                }
            },
            yAxis: {
                type: 'value',
                name: starLabel,
                nameRotate: 90,
                nameLocation: 'middle',
                nameGap: 50,
                splitNumber: 6
            },
            series: [{
                name: seriesName,
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
    
    window.addEventListener('resize', function() {
        chart.resize();
    });
});

function createCometParticle(x, y) {
    const starParticlesContainer = document.querySelector('.github-chart-container .star-particles');
    if (!starParticlesContainer) return;
    
    const comet = document.createElement('div');
    comet.className = 'star-particle comet';
    
    comet.style.left = `${x}px`;
    comet.style.top = `${y}px`;
    
    const angle = Math.random() * 360;
    const distance = 50 + Math.random() * 100; 
    
    starParticlesContainer.appendChild(comet);
    
    requestAnimationFrame(() => {
        const rad = angle * Math.PI / 180;
        const moveX = Math.cos(rad) * distance;
        const moveY = Math.sin(rad) * distance;
        
        comet.style.transform = `translate(${moveX}px, ${moveY}px) scale(0.1)`;
        comet.style.opacity = '0';
    });
    
    setTimeout(() => {
        if (comet.parentNode === starParticlesContainer) {
            starParticlesContainer.removeChild(comet);
        }
    }, 1500);
}

document.addEventListener('DOMContentLoaded', function() {
    enhanceThanksListItems();
    
    const contributorCells = document.querySelectorAll('.contributors-grid td');
    
    contributorCells.forEach(cell => {
        const link = cell.querySelector('a');
        if (!link) return;
        
        const img = link.querySelector('img');
        if (img && !link.querySelector('.contributor-avatar-container')) {
            const imgSrc = img.getAttribute('src');
            const imgAlt = img.getAttribute('alt');
            const nameElement = link.querySelector('sub');
            const name = nameElement ? nameElement.innerHTML : '';
            
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
            
            if (nameElement) nameElement.remove();
            img.remove();
            link.innerHTML = newHTML;
        }
    });
    
    contributorCells.forEach(cell => {
        cell.addEventListener('mousemove', function(e) {
            const avatar = cell.querySelector('img');
            if (!avatar) return;
            
            const rect = cell.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width - 0.5; 
            const y = (e.clientY - rect.top) / rect.height - 0.5; 
            
            avatar.style.transform = `scale(1.1) rotateY(${x * 20}deg) rotateX(${-y * 20}deg)`;
            
            const glow = cell.querySelector('.contributor-avatar-glow');
            if (glow) {
                glow.style.background = ''; 
            }
        });
        
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

    function adjustContributorsLayout() {
        if (window.innerWidth <= 768) {
            const contributorsTable = document.querySelector('.contributors-grid table');
            const contributorsTbody = document.querySelector('.contributors-grid tbody');
            const allCells = Array.from(document.querySelectorAll('.contributors-grid td'));
            
            if (!contributorsTable || !contributorsTbody || allCells.length === 0) return;
            
            const cellsCopy = allCells.map(cell => cell.cloneNode(true));
            
            while (contributorsTbody.firstChild) {
                contributorsTbody.removeChild(contributorsTbody.firstChild);
            }
            
            const cellsPerRow = window.innerWidth <= 480 ? 2 : 3;
            
            for (let i = 0; i < cellsCopy.length; i += cellsPerRow) {
                const newRow = document.createElement('tr');
                newRow.style.display = 'flex';
                newRow.style.justifyContent = 'center';
                newRow.style.width = '100%';
                newRow.style.marginBottom = '10px';
                
                const cellsToAdd = Math.min(cellsPerRow, cellsCopy.length - i);
                
                const isLastRow = i + cellsPerRow >= cellsCopy.length;
                if (isLastRow && cellsToAdd < cellsPerRow) {
                    
                }
                
                for (let j = 0; j < cellsToAdd; j++) {
                    const cell = cellsCopy[i + j];
                    cell.style.display = 'block';
                    cell.style.width = `${100 / cellsPerRow}%`;
                    cell.style.boxSizing = 'border-box';
                    
                    const link = cell.querySelector('a');
                    if (link) {
                        link.addEventListener('mousemove', function(e) {
                            
                        });
                    }
                    
                    newRow.appendChild(cell);
                }
                
                contributorsTbody.appendChild(newRow);
            }
        }
    }
    
    adjustContributorsLayout();
    
    window.addEventListener('resize', adjustContributorsLayout);
});

function enhanceThanksListItems() {
    const thanksItems = document.querySelectorAll('.thanks-item');
    
    thanksItems.forEach(item => {
        const link = item.querySelector('a');
        const avatar = item.querySelector('.thanks-avatar');
        const nameSpan = item.querySelector('a span');
        
        if (!link || !avatar) return;
        
        avatar.style.transition = 'all 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
        
        link.addEventListener('mouseenter', function() {
            if (!item.querySelector('.thanks-item-shine')) {
                const shine = document.createElement('div');
                shine.className = 'thanks-item-shine';
                shine.style.position = 'absolute';
                shine.style.top = '0';
                shine.style.left = '0';
                shine.style.width = '100%';
                shine.style.height = '100%';
                shine.style.background = 'linear-gradient(135deg, transparent, rgba(255,255,255,0.4), transparent)';
                shine.style.transform = 'translateX(-100%) skewX(-15deg)';
                shine.style.pointerEvents = 'none';
                
                link.appendChild(shine);
                
                setTimeout(() => {
                    shine.style.transition = 'transform 0.8s ease-out';
                    shine.style.transform = 'translateX(100%) skewX(-15deg)';
                    
                    shine.addEventListener('transitionend', function() {
                        if (shine.parentNode === link) {
                            link.removeChild(shine);
                        }
                    });
                }, 50);
            }
            
            for (let i = 0; i < 3; i++) {
                const particle = document.createElement('span');
                particle.className = 'thanks-particle';
                
                const size = 4 + Math.random() * 6;
                particle.style.position = 'absolute';
                particle.style.width = `${size}px`;
                particle.style.height = `${size}px`;
                particle.style.borderRadius = '50%';
                particle.style.backgroundColor = Math.random() > 0.5 ? 
                    'rgba(122, 162, 247, 0.6)' : 'rgba(247, 125, 170, 0.6)';
                particle.style.left = `${20 + Math.random() * 60}%`;
                particle.style.bottom = '0';
                particle.style.zIndex = '0';
                particle.style.pointerEvents = 'none';
                
                particle.style.animation = `float-up ${1.5 + Math.random()}s ease-out forwards`;
                link.appendChild(particle);
                
                setTimeout(() => {
                    if (particle.parentNode === link) {
                        link.removeChild(particle);
                    }
                }, 2000);
            }
            
            if (nameSpan && Math.random() > 0.7) {
                const originalText = nameSpan.textContent;
                const chars = originalText.split('');
                nameSpan.textContent = '';
                
                chars.forEach((char, index) => {
                    setTimeout(() => {
                        nameSpan.textContent += char;
                    }, 30 * index);
                });
            }
            
            const randomRotate = Math.random() > 0.5 ? '5deg' : '-5deg';
            avatar.style.transform = `scale(1.15) rotate(${randomRotate})`;
        });
        
        link.addEventListener('mouseleave', function() {
            link.style.background = '';
        });
    });
    
    if (!document.getElementById('thanks-list-animations')) {
        const style = document.createElement('style');
        style.id = 'thanks-list-animations';
        style.textContent = `
            @keyframes float-up {
                0% { transform: translateY(0) scale(1); opacity: 1; }
                100% { transform: translateY(-100px) scale(0); opacity: 0; }
            }
        `;
        document.head.appendChild(style);
    }
}