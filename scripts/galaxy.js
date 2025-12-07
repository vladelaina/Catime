/* 
 * The Kinetic Noir (Liquid Motion Engine + Nebula System)
 * Combines Mouse Parallax + Autonomous Breathing + Particle Storm
 */

document.addEventListener('DOMContentLoaded', () => {
    initLiquidMotion();
});

function initLiquidMotion() {
    const section = document.querySelector('.luminaries-section');
    if (!section) return; // Exit early if section missing

    const container = section.querySelector('.narrative-container');
    const core3k = section.querySelector('.core-2k');
    const starsText = section.querySelector('.script-title');
    const ghostText = section.querySelector('.ghost-text');
    const pills = section.querySelectorAll('.stat-pill');
    const cta = section.querySelector('.narrative-cta');
    const redDeco = section.querySelector('.red-line-deco');
    
    // Scoped Selectors to avoid conflicts (e.g., .phonetic exists elsewhere)
    const badgeTop = section.querySelector('.badge-top');
    const phonetic = section.querySelector('.phonetic');
    const statsRow = section.querySelector('.stats-row');
    
    if (!core3k || !starsText) return;

    // --- DARK MATTER PHYSICS ENGINE: Create Stardust ---
    const PARTICLE_COUNT = 40; // More density
    const particles = [];
    
    for (let i = 0; i < PARTICLE_COUNT; i++) {
        const mote = document.createElement('div');
        mote.className = 'star-mote';
        
        // Physics Init
        const x = (Math.random() - 0.5) * 120; // Wide spread
        const y = (Math.random() - 0.5) * 120; 
        const size = Math.random() * 3 + 1; 
        const opacity = Math.random() * 0.4 + 0.05; // Subtle
        const blur = Math.random() > 0.5 ? '1px' : '0px'; // Depth of Field
        
        mote.style.cssText = `
            position: absolute;
            top: 50%; left: 50%;
            width: ${size}px; height: ${size}px;
            background: #000;
            border-radius: 50%;
            opacity: ${opacity};
            filter: blur(${blur}); 
            pointer-events: none;
            z-index: 4;
            will-change: transform;
        `;
        
        container.appendChild(mote);
        
        particles.push({
            el: mote,
            baseX: x, // Origin
            baseY: y, 
            currX: 0, // Current Physics Position (px offset)
            currY: 0,
            vx: 0, // Velocity
            vy: 0,
            friction: 0.94, // Air Resistance
            spring: 0.05, // Return Strength
            parallaxFactor: Math.random() * 0.5 + 0.1 // Parallax Depth
        });
    }

    // --- TEMPORAL ARTIFACTS: Geometric Markers ---
    // Add subtle engineering marks to the background grid
    const MARKER_COUNT = 6;
    const markers = ['+', '○', '△', '⟁', '×', '::'];
    
    for (let i = 0; i < MARKER_COUNT; i++) {
        const marker = document.createElement('div');
        marker.className = 'geo-marker';
        marker.textContent = markers[Math.floor(Math.random() * markers.length)];
        
        // Snap to grid-ish positions (assuming 100px grid, roughly)
        const gridX = Math.floor(Math.random() * 10) * 10 + 5; // %
        const gridY = Math.floor(Math.random() * 10) * 10 + 5; // %
        
        marker.style.cssText = `
            position: absolute;
            left: ${gridX}%; top: ${gridY}%;
            font-family: 'Courier New', monospace;
            font-size: 10px;
            color: rgba(0,0,0,0.1); /* Very subtle */
            pointer-events: none;
            z-index: 0;
            transform: translate(-50%, -50%);
        `;
        container.appendChild(marker);
    }

    // --- AMBIENT LIGHT ---
    const ambientLight = document.createElement('div');
    ambientLight.className = 'ambient-light';
    ambientLight.style.cssText = `
        position: absolute;
        top: 0; left: 0; width: 100%; height: 100%;
        background: radial-gradient(circle 800px at 50% 50%, rgba(0,0,0,0.03) 0%, transparent 100%);
        pointer-events: none;
        z-index: 0;
        transition: opacity 0.5s ease;
    `;
    section.insertBefore(ambientLight, section.firstChild);


    // State
    let mouse = { x: 0, y: 0 }; // Normalized (-1 to 1)
    let rawMouse = { x: 0, y: 0 }; // Raw px relative to section
    let current = { x: 0, y: 0 }; 
    
    const LERP_FACTOR = 0.05; 

    // --- HUD SYSTEM ---
    const hudCoords = document.getElementById('mouse-coords');

    // ... existing code ...

    // Event Listener
    section.addEventListener('mousemove', (e) => {
        const rect = section.getBoundingClientRect();
        rawMouse.x = e.clientX - rect.left;
        rawMouse.y = e.clientY - rect.top;
        
        mouse.x = (rawMouse.x - rect.width/2) / (rect.width/2);
        mouse.y = (rawMouse.y - rect.height/2) / (rect.height/2);

        // Update HUD
        if (hudCoords) {
            const displayX = rawMouse.x.toFixed(2).padStart(7, '0');
            const displayY = rawMouse.y.toFixed(2).padStart(7, '0');
            hudCoords.textContent = `${displayX}, ${displayY}`;
        }
    });

    section.addEventListener('mouseleave', () => {
        mouse.x = 0;
        mouse.y = 0;
        // Reset raw mouse to center to stop repelling
        const rect = section.getBoundingClientRect();
        rawMouse.x = rect.width / 2;
        rawMouse.y = rect.height / 2;
    });

    // The Animation Loop
    function animate() {
        // 1. Interpolate Mouse Values
        current.x += (mouse.x - current.x) * LERP_FACTOR;
        current.y += (mouse.y - current.y) * LERP_FACTOR;

        // 2. Life Force
        const time = Date.now() * 0.001; 

        // --- Core 3K ---
        const coreBreathY = Math.sin(time * 0.8) * 15;
        const coreRotate = Math.sin(time * 0.5) * 1; 
        core3k.style.transform = `
            translate(calc(-50% + ${current.x * -25}px), calc(-50% + ${current.y * -25 + coreBreathY}px)) 
            rotate(${coreRotate}deg)
        `;

        // --- Stars Text ---
        const starBreathY = Math.sin(time * 1.2 + 1) * 10;
        starsText.style.transform = `
            translate(calc(-50% + ${current.x * 40}px), calc(-50% + ${current.y * 40 + starBreathY}px)) 
            rotate(calc(-5deg + ${current.x * 5}deg))
        `;

        // --- MICRO-TYPOGRAPHY RESONANCE ---
        // 1. Badge Top: Floats above STARS
        if(badgeTop) {
            const badgeFloatY = Math.sin(time * 1.0 + 0.5) * 8; 
            badgeTop.style.transform = `translate3d(calc(-50% + ${current.x * 20}px), ${current.y * 20 + badgeFloatY}px, 0)`;
        }

        // 2. Phonetic: Floats near STARS
        if(phonetic) {
            const phoneFloatY = Math.sin(time * 1.1 + 2) * 6; 
            phonetic.style.transform = `translate3d(${current.x * 30}px, ${current.y * 30 + phoneFloatY}px, 0)`;
        }

        // --- Ghost Text ---
        if(ghostText) {
            const ghostBreathY = Math.sin(time * 0.5) * 20;
            ghostText.style.transform = `
                translate3d(calc(-50% + ${current.x * -10}px), calc(-50% + ${current.y * -10 + ghostBreathY}px), 0)
            `;
        }

        // --- STATS & DECO ---
        const debrisBreathY = Math.sin(time * 1.5) * 5;
        
        pills.forEach((pill, index) => {
            const delay = index * 0.8; 
            const pillY = Math.sin(time * 2.0 + delay) * 10; 
            const pillX = current.x * (30 + index * 15); 
            const pillMouseY = current.y * (30 + index * 15);
            pill.style.transform = `translate3d(${pillX}px, ${pillMouseY + pillY}px, 0)`;
        });

        if (redDeco) {
             const redY = Math.sin(time * 1.5 + 0.2) * 8;
             redDeco.style.transform = `translate3d(calc(-50% + ${current.x * 20}px), ${current.y * 20 + redY}px, 0)`;
        }

        if (cta) {
            cta.style.transform = `translate(calc(-50% + ${current.x * 15}px), ${current.y * 15 + debrisBreathY}px)`;
        }

        // --- DARK MATTER PHYSICS: Particles ---
        const rect = section.getBoundingClientRect();
        const centerX = rect.width / 2;
        const centerY = rect.height / 2;

        particles.forEach(p => {
            // 1. Convert vw/vh base to px
            const targetX = (p.baseX / 100) * rect.width;
            const targetY = (p.baseY / 100) * rect.height;

            // 2. Parallax Influence (Base Drift)
            const parallaxX = current.x * p.parallaxFactor * 50; 
            const parallaxY = current.y * p.parallaxFactor * 50;

            // 3. Repulsion Logic (Interaction)
            // Particle Position relative to center + Center offset
            // We calculate distance from Mouse to Particle
            // Particle world pos approx = Center + Target + Parallax
            const pWorldX = centerX + targetX + parallaxX;
            const pWorldY = centerY + targetY + parallaxY;
            
            const dx = rawMouse.x - pWorldX;
            const dy = rawMouse.y - pWorldY;
            const distance = Math.sqrt(dx * dx + dy * dy);
            const repelRadius = 200;

            if (distance < repelRadius) {
                const force = (repelRadius - distance) / repelRadius;
                const angle = Math.atan2(dy, dx);
                // Push away
                p.vx -= Math.cos(angle) * force * 1.5; 
                p.vy -= Math.sin(angle) * force * 1.5;
            }

            // 4. Spring Mechanics (Return to orbit)
            // Hooke's Law: F = -k * x
            // We want p.currX/Y to settle at 0 (relative to base+parallax)
            p.vx += (0 - p.currX) * p.spring;
            p.vy += (0 - p.currY) * p.spring;

            // 5. Apply Velocity & Friction
            p.vx *= p.friction;
            p.vy *= p.friction;
            p.currX += p.vx;
            p.currY += p.vy;

            // 6. Render
            // Base + Parallax + Physics Offset
            p.el.style.transform = `translate(calc(-50% + ${targetX + parallaxX + p.currX}px), calc(-50% + ${targetY + parallaxY + p.currY}px))`;
        });

        // --- Ambient Light Follow ---
        ambientLight.style.background = `radial-gradient(circle 800px at ${rawMouse.x}px ${rawMouse.y}px, rgba(0,0,0,0.04) 0%, transparent 100%)`;

        requestAnimationFrame(animate);
    }

    animate();
}
