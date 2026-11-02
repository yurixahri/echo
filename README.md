<p align="center">
  <img src="assets/icons/logo.png" alt="Echo Logo" width="160" />
</p>

<h1 align="center">Echo Client</h1>

<p align="center">
  A lightweight, server-authoritative voice call & chat application built with C++20 and static Qt 6.
</p>

<img width="1458" height="988" alt="image" src="https://github.com/user-attachments/assets/79e616c5-a264-424e-b590-03370157ef14" />


---

## Why?

I can't wait Teamspeak 6 to update feature like centralize message, stable server (it need to be reinstalled every 2 months to renew the licence). I do love it and use it for a few years now, but for me and my friends that use several communation tools, we only want something simple, lightweight, work out of the box. Back in the old day when Garena still have their own voice chat called AloAlo (the old one), it was simple to use for such simple task. Because of it, I vibed this app (also with brain and sanity) to replicate what I want to have, what are good from Garena and Teamspeak, into this.

---

## Quick Review
- **Doesn't required register account to login**
- **Low-Latency Voice:** High-performance UDP pipeline with Opus codec (48kHz mono, configurable 16–128 kbps bitrate).
- **Decoupled Architecture:** Talk in one channel while chatting in another without interruption.
- **Privacy & Sliding Window Media:** Chat images are rendered entirely in RAM (up to 20 images buffered) and never written to disk cache. Hard deletion permanently purges message rows and files.
- **Pure QSS Theming & Hot-Reload:** 10+ built-in themes driven by `global_values.ini`. Change themes instantly at runtime without restarting.
- **Granular Permissions & Roles:** Power-level hierarchy for channel join and talk powers, enforced in real time.
- **Rich Chat:** Live Markdown preview, emoji picker, code blocks, and chunked attachment uploads (up to 10MB).

---

## Building from Source

### Prerequisites

- **Compiler:** MinGW-w64 with C++20 support (GCC 13+ recommended).
- **Qt Version:** Qt 6.11 static version from MinGW configured with static linking (`-static`).
- **Dependencies:**
  - `libtasn1` (ASN.1 DER encoding/decoding. I watched a stream from Tsoding messing around with this, so I tested it out, work so good with TCP than JSON so i use it)
  - `libopus` (Audio compression)
  - `zlib` (Not use, for now)

### Build Instructions
##### Windows
To build this project, please install msys2 and get the lasted [mingw-w64-x86_64-qt6-base](https://packages.msys2.org/packages/mingw-w64-x86_64-qt6-base) and [mingw-w64-x86_64-qt6-static](https://packages.msys2.org/packages/mingw-w64-x86_64-qt6-static), this is a static build project. You can use Qtcreator from mingw for gui, or just use qmake and build:

```bash
# 1. Clone repository
git clone https://github.com/yurixahri/echo.git
cd echo

# 2. Run qmake with your static Qt installation
qmake echo.pro -spec win32-g++ "CONFIG+=release"

# 3. Compile
mingw32-make -j$(nproc)
```

The compiled standalone executable will be placed in `release/Echo.exe` with bundled assets and styles.
Only support Windows for now.

---

## How to Use

1. **Launch Echo:** Run `Echo.exe`.
2. **Connect to a Server:** Enter your server IP/domain and port (default `5555`). Toggle SSL if your server runs encrypted.
3. **Voice & Audio Settings:** Open Client Settings (gear icon) to configure:
   - Input/output devices, volume, and Voice Activity Detection (VAD) threshold.
   - Built-in microphone test with live decibel loopback.
   - Sound effects toggle and volume slider.
4. **Chat & Channels:** Click the phone icon to enter voice; click the chat bubble to open channel messaging.
5. **Config**: All your settings, including your account - which is your uuid per server will be saved in config.ini at program directory. Please keep it safe, or if you want to use on another machine, copy it to the new machine and run the program.
6. **Theme customization**: I write external qss files (Qt css but fewer features) and expose it to end users, you can just clone a theme in styles/ an customize your own style, colors.

---

## Todo list (if i'm not lazy)

- streaming
- more strictly connection, handshake...

---
## Credits

- **Icons:** Provided by [SVG Repo](https://www.svgrepo.com) under open licenses.
- **Sound Effects:** Provided by [KDE Store (Fresh UI Sound Theme)](https://store.kde.org/p/2328498).

---

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
