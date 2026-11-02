# Echo Client Architecture & File Index

## 1. Overview

Echo is a lightweight, server-authoritative voice and chat client built with C++20 and Qt 6.11 (static MinGW-w64). It avoids excessive class abstractions, favoring procedural, header-only modules where Qt subclassing is not strictly required.

For the complete TCP/UDP API, wire fields, response events, and event diagrams,
see [API_REFERENCE.md](API_REFERENCE.md).

---

## 2. Core Entry & Window Management

- **[main.cpp](../main.cpp)**
  - Application entry point.
  - Initializes ASN.1 definitions (`ChatProtocol.asn`).
  - Reads configuration (`config.ini`) and loads startup theme.
  - Registers ASN.1 response event dispatcher.
  - Configures application identity, branding, and taskbar icon (`logo_small.ico`).

- **[mainwindow.h](../mainwindow.h)** / **[mainwindow.cpp](../mainwindow.cpp)**
  - Frameless main window container with custom draggable titlebar.
  - Instantiates `QSplitter` layout: `side_bar` (left), `main_area` (center), and `chat_area` (right).
  - Handles auto-reconnect timer (3s interval), maintaining previous voice channel.
  - Persists and restores window geometry, maximized state, and splitter sizes via `config.ini`.
  - Implements dynamic theme hot-reloading (`set_ui_style()`).
  - Clears preview image cache on exit (`closeEvent` and destructor).

---

## 3. Data Types (`common/`)

- **[common/types.h](../common/types.h)**
  - Defines core enumerations: `ASN1_EVENT`, `ASN1_RESPONE_EVENT`, `MESSAGE_TYPE`, `VOICE_PACKET_TYPE`.
  - Defines data structs: `server_t`, `room_t`, `voice_user_t`, `chat_message_t`, `user_info_t`, `member_item_t`, `role_t`, `user_role_item_t`.
  - Implements natural sorting helper `natural_sort_less_than` using `QCollator` with numeric mode enabled.

---

## 4. Utility Modules (`utils/`)

- **[utils/config.h](../utils/config.h)** / **[utils/config.cpp](../utils/config.cpp)**
  - Thread-safe configuration manager with `QReadWriteLock`.
  - Manages audio settings, theme selection, saved servers, per-server room passwords, and per-server user volumes (`[servers/serverX/user_volumes]`).
  - Implements non-blocking `write_config_async()` via `QThreadPool` with atomic generation counter to coalesce rapid writes (e.g. slider drags).
  - Keeps synchronous `write_config()` exclusively for application shutdown.

- **[utils/asn1_parser.h](../utils/asn1_parser.h)**
  - Wrapper around `libtasn1`.
  - `ClientNetworkWorker`: Dedicated network thread managing `QTcpSocket` / `QSslSocket`.
  - Implements ASN.1 tree parsing, DER encoding, integer sign-extension handling, and chunk decoding.

- **[utils/theme.h](../utils/theme.h)**
  - Theme engine driving 10 visual styles via `global_values.ini` key-value pairs.
  - Substitutes `${token}` placeholders into `.qss` template stylesheets at runtime.
  - Provides dynamic SVG and PNG colorization (`colored_svg`, `colored_png`).

- **[utils/markdown_helper.h](../utils/markdown_helper.h)**
  - Parses Markdown syntax for chat rendering (bold, italic, underline, blockquotes, code blocks, links).
  - Implements `sanitize_markdown_html` to strip unsafe HTML tags (`<script>`, `<iframe>`, `<style>`, `<font>`) and clamp inline font sizes to prevent UI exploits.

---

## 5. Functional Subsystems (`functions/`)

- **[functions/voice.h](../functions/voice.h)**
  - Low-latency UDP voice pipeline running on a dedicated high-priority worker thread.
  - Audio capture via `QAudioSource` (48kHz mono 16-bit PCM).
  - Audio encoding/decoding via `libopus`.
  - Voice Activity Detection (VAD) with decibel threshold calculation.
  - Microphone testing facility with audio loopback and 12 FPS throttled meter updates.
  - Per-user volume attenuation stored and retrieved per server.

- **[functions/sound.h](../functions/sound.h)**
  - Sound effect subsystem using `QSoundEffect` with PCM WAV audio (`voice_join.wav`, `voice_leave.wav`, `poke.wav`, `message.wav`).
  - Non-overlapping playback for room transitions.
  - 3-instance round-robin pool for stackable incoming message chimes.
  - Configurable master volume and toggle in Settings.

- **[functions/user.h](../functions/user.h)**
  - Handles authentication, PoW challenge solving, admin privilege claiming, and member profile querying.

- **[functions/room.h](../functions/room.h)**
  - Room management client handlers: create room, join room, update room, leave room.

- **[functions/role.h](../functions/role.h)**
  - Role management handlers: create role, update role permissions, delete role, assign user roles.

- **[functions/file.h](../functions/file.h)**
  - File transfer pipeline: chunked uploads and downloads.
  - Enforces avatar MIME and magic byte validation (PNG, JPEG, GIF, BMP, WEBP, AVIF).
  - In-memory temporary preview cache management in `%temp%/echo_preview/`.
  - Multi-platform detached file manager opening (`explorer.exe /select,"<path>"`).

---

## 6. User Interface (`ui/`)

- **[ui/chat_area.h](../ui/chat_area.h)**
  - Full chat room view with auto-expanding input, live preview, formatting toolbar, and file drag-and-drop.
  - In-memory sliding window media architecture: holds up to 20 images in RAM, unloading offscreen assets without touching disk.
  - Built-in emoji picker popup with standard UTF-8 emojis.
  - Anti-spam rate limiter (5 messages / 2 seconds).

- **[ui/server_header.h](../ui/server_header.h)**
  - Top header displaying server name, Disconnect, My Profile, Server Users, and Server Menu (`⋯`).

- **[ui/room_view.h](../ui/room_view.h)**
  - Tree-style list of rooms showing occupants, password locks, and speaking indicators.

- **[ui/voice_panel.h](../ui/voice_panel.h)**
  - Bottom-left status card showing active voice channel, mute/deafen buttons, and disconnect button.

- **[ui/server_list.h](../ui/server_list.h)**
  - Collapsible sidebar widget listing saved servers for quick reconnect.

- **[ui/toast.h](../ui/toast.h)**
  - Non-intrusive floating notification cards anchored above all dialogs.

- **[ui/popup.h](../ui/popup.h)** / **[ui/popup.cpp](../ui/popup.cpp)**
  - Base modal dialog with background blur and dynamic recentering on window resize/move.

- **Dialog Popups:**
  - `ui/connection_popup.h`: Server connection status and errors.
  - `ui/password_popup.h`: Server and room password prompts.
  - `ui/settings_popup.h`: General (Theme, Cache Purge), Voice (Device, VAD, Test), Audio (Playback, SFX), and About (Repo, Updates).
  - `ui/my_info.h`: Current user profile, avatar upload, and uptime.
  - `ui/user_info_popup.h`: Target user profile card displaying assigned roles.
  - `ui/server_users_popup.h`: Full server member roster with poke and profile shortcuts.
  - `ui/user_volume_popup.h`: Slider to adjust individual user playback volume.
  - `ui/roles_popup.h` & `ui/user_roles_popup.h`: Role creation and assignment management.
  - `ui/create_room.h` & `ui/room_settings_popup.h`: Room creation and configuration.
  - `ui/claim_admin.h`: Dialog to input initial server setup key.
