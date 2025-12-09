# THE CATIME STANDARD // DESIGN MANIFESTO
> "We are not building a timer. We are sculpting time itself."

## 0. THE CORE PHILOSOPHY (核心哲学)

**Status: Top-Tier / Apple-Level**
We do not accept "good enough." Every pixel must justify its existence. If an element does not contribute to the narrative of "Precision" or "Kawaii," it must be removed.

**The Golden Rule: Complex Minimalism (复杂的极简主义)**
-   **Surface**: Clean, white, airy, approachable.
-   **Depth**: When the user looks closer, they see infinite detail (Micro-typography, noise textures, HUD coordinates, ghost text).
-   **Result**: A design that feels simple at a glance, but expensive upon inspection.

---

## 1. VISUAL LANGUAGE (视觉语言)

### 1.1 The Palette (色板)
-   **Absolute Purity**: Pure White (`#FFFFFF`) vs. Vantablack (`#000000`). No greys unless they are ghosts.
-   **The Pulse**: International Orange / Neon Red (e.g., `#ff3b30`). Used sparingly. Only for "Heartbeat," "Live Status," or "Critical Focus."
-   **Glass**: Frosted materials must be high-quality. `backdrop-filter: blur(10px)` is the minimum standard.

### 1.2 Typography as Architecture (字体即建筑)
-   **Editorial Layout**: Treat the web page like a high-end fashion magazine (Vogue/Kinfolk).
-   **Monospace Data**: Use monospaced fonts (Courier New/Consolas) for technical data (Versions, Coords). It adds "Instrumental Reality."
-   **The "Ghost" Layer**: Giant, outlined, transparent text behind the content creates depth.
-   **Micro-Labels**: Every major element should have a tiny "Technical Label" (e.g., `[ REF. GITHUB ]`, `EST. 2025`).

---

## 2. THE KINETIC DOCTRINE (动效法则)

**"No Dead Pixels" (拒绝死像素)**
Nothing on the screen should ever be completely static. Even when the user is doing nothing, the interface must breathe.

### 2.1 The Liquid Motion Engine
-   **Parallax**: Elements must float at different speeds based on mouse movement.
-   **Sympathetic Resonance (共振)**: Small elements (badges, labels) must "follow" larger elements but with a *Phase Shift* (Delay). They should feel like they are connected by invisible rubber bands.
-   **Autonomous Breathing**: Use Sine Waves (`Math.sin(time)`) to make elements gently rise and fall.

### 2.2 Physics > Animations
-   Do not use linear animations. Use **Spring Physics** and **Lerp (Linear Interpolation)**.
-   Movement should feel like it has *weight* and *inertia*.
-   **GPU Enforcement**: All moving elements MUST use `translate3d(x,y,0)` or `will-change: transform`. We do not tolerate frame drops.

---

## 3. THE ART OF TRANSITION (过渡的艺术)

**"Dissolve the Horizon" (消融地平线)**
A hard line between sections is a failure of imagination.

-   **The Mist**: Use gradient masks (`mask-image`) to make content fade into existence.
-   **The Bleed**: Allow sections to physically overlap (Negative Margins).
-   **The Void**: Do not fear empty space. White space is not "empty"; it is "air."
-   **Subtraction**: If a connector (like a capsule or line) looks cluttered, remove it. The best connection is the user's flow of consciousness.

---

## 4. QUALITY CONTROL (质检标准)

Before committing any code, ask these questions:

1.  **Is it Alive?** (Does it breathe?)
2.  **Is it Deep?** (Is there a layer behind it? A layer in front of it?)
3.  **Is it Necessary?** (If you delete it, does the design look cleaner? If yes, delete it.)
4.  **Does it Frame 60fps?** (If it stutters, optimize it.)

