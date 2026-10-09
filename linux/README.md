# Catime — Linux port (Ubuntu 24.04)

A native **GTK3 + Cairo** port of Catime (the lightweight, transparent
countdown & Pomodoro timer) for Ubuntu 24.04 LTS. It is written in **pure C**,
just like the original Windows app, and reuses the project's portable assets
(`libs/miniaudio`, `libs/stb`, `libs/miniz`) and the `resource/languages/*.ini`
translation files.

This lives alongside the Windows build and does **not** modify it. The Windows
sources remain unchanged; everything for Linux is under this `linux/` directory.

## What is implemented (core scope)

- **Transparent, borderless, always-on-top countdown window** with custom
  Cairo/Pango text rendering (solid color or multi-stop gradient).
- **Edit mode**: drag to move, mouse-wheel to scale, `Ctrl`+wheel for opacity,
  arrow keys to nudge, right-click / `Esc` to exit. Position & scale persist.
  Click-through when not in edit mode.
- **Timer modes**: Show Current Time (clock), Count Up (stopwatch), Countdown.
  Drift-free timing (monotonic absolute deadlines), 12/24-hour, show-seconds,
  show-milliseconds, and the three time formats (Default / `09:59` / `00:09:59`).
- **Pomodoro**: configurable interval sequence + loop count, per-session
  snapshot, completion notifications.
- **System tray** (Ayatana AppIndicator) with a full context menu: timer
  control, time display, Pomodoro, count up/down, quick presets, format, topmost,
  edit mode, language, about, exit.
- **Desktop notifications** (libnotify) on countdown/Pomodoro completion.
- **Alarm audio** (miniaudio): plays a configured sound file or a generated beep.
- **CLI** compatible with the Windows app: `catime 25`, `catime 2h3m`,
  `catime 17 20t`, and commands `s u p r pr q1.. v e h` / `p<number>`.
  Commands are forwarded to the running instance (single-instance via a Unix
  socket) — no display server required to send commands.
- **Global hotkeys** (X11 `XGrabKey`), parsed from `[Hotkeys]` in the config.
  Gracefully disabled on a pure Wayland session (where global grabs aren't
  available).
- **Config**: `~/.config/catime/config.ini` using the same section/key names and
  value formats as the Windows build (so the two interoperate).
- **i18n**: loads the original `resource/languages/*.ini` (10 languages) at
  runtime, with English fallback and automatic locale detection.

Not in this core port (present on Windows, out of scope here): tray icon
animations & system monitor, the full owner-drawn color/font/style picker
dialogs, plugins, the update checker, and the advanced text effects.

## Requirements (Ubuntu 24.04)

```bash
sudo apt install build-essential cmake pkg-config \
    libgtk-3-dev libcairo2-dev libpango1.0-dev \
    libayatana-appindicator3-dev libnotify-dev libx11-dev
```

## Build

```bash
cd linux
./build.sh                 # Release build -> ./build/catime
# or:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The build automatically locates the shared `resource/` and `libs/` trees one
level up, so language files are found at runtime in development without
installing.

## Run

```bash
./build/catime                       # run with the configured startup mode
./build/catime 25                    # 25-minute countdown
./build/catime p                     # start Pomodoro
./build/catime v                     # show/hide window
./build/catime e                     # toggle edit mode
./build/catime pr                    # pause/resume
./build/catime --help                # full CLI reference
./build/catime --version
```

### System install

```bash
sudo cmake --install build           # -> /usr/local/bin/catime, share/catime, ...
sudo gtk-update-icon-cache /usr/local/share/icons/hicolor || true
```

## Configuration

`~/.config/catime/config.ini`. Key sections: `[General]`, `[Display]`, `[Timer]`,
`[Pomodoro]`, `[Notification]`, `[Hotkeys]`, `[Colors]`. Edit it by hand — most
display/color/format/hotkey changes are picked up the next time the app starts.

Example hotkey entries (in `[Hotkeys]`):

```ini
PAUSE_RESUME=Ctrl+Alt+P
RESTART_TIMER=Ctrl+Alt+R
TOGGLE_VISIBILITY=Ctrl+Alt+V
POMODORO=Ctrl+Alt+M
SHOW_TIME=Ctrl+Alt+T
```

## Notes on desktop sessions

- **X11**: full functionality, including global hotkeys.
- **Wayland**: window, tray, notifications, audio, and CLI all work. Global
  hotkeys are unavailable (X11-only) and are disabled automatically.
  Always-on-top is a hint that some Wayland compositors ignore.

## File layout

```
linux/
  CMakeLists.txt        build definition (separate from the Windows CMake)
  build.sh              convenience build script
  data/
    catime.desktop      application launcher entry
    icons/catime.png    application icon
  src/                  the native Linux frontend (pure C)
```
