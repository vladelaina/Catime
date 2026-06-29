initAOSOnce();

document.addEventListener('DOMContentLoaded', function() {
    initScenarioImageInteractions();
    enhanceThanksListItems();
    initContributorInteractions();
});

function initScenarioImageInteractions() {
    CatimeUI.initTiltBreatheImages({
        imageSelector: '.scenario-img',
        containerSelector: '.scenario-visual',
        maxRotateX: 12,
        maxRotateY: 18,
        scaleAmplitude: 0.02,
    });
}

function initContributorInteractions() {
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
                    <img src="${imgSrc}" width="100px;" alt="${imgAlt}" loading="lazy" decoding="async">
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

                for (let j = 0; j < cellsToAdd; j++) {
                    const cell = cellsCopy[i + j];
                    cell.style.display = 'block';
                    cell.style.width = `${100 / cellsPerRow}%`;
                    cell.style.boxSizing = 'border-box';

                    newRow.appendChild(cell);
                }
                
                contributorsTbody.appendChild(newRow);
            }
        }
    }

    adjustContributorsLayout();

    window.addEventListener('resize', adjustContributorsLayout);
}

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
