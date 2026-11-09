## 🚀 Release [v1.3.0] — Hotkeys, Linux & macOS Support, Audio & UTF-8 Fixes

> [!NOTE]
> **✅ AUTOMATIC LEGACY AVATAR MIGRATION (UPDATING FROM v1.1.0 OR OLDER):**
> The Windows Installer (`FloodTuber-Installer-1.3.0.exe`) now automatically backs up and migrates any existing custom avatars from the legacy `C:\Program Files\obs-studio\data\obs-plugins\flood-tuber\avatars\` directory to `C:\ProgramData\obs-studio\plugins\flood-tuber\data\avatars\` and keeps a safety copy inside `backup_legacy_avatars\`.

### ✨ What's New in v1.3.0
* **[OBS Hotkeys & 10 Avatar Slots]** Added native OBS Hotkeys (`Flood Tuber: next avatar`, `Flood Tuber: previous avatar`, and `Avatar slot 1–10`) along with a dedicated **Avatar Slots (1–10)** group in the properties UI — fully Stream Deck friendly! (#15)
* **[Linux & macOS Support]** Official cross-platform support and packages for **Linux (Ubuntu 24.04 `.deb` & `.tar.xz`)** and **macOS Universal (`.pkg` & `.tar.xz` for Intel & Apple Silicon)** (#7, #14, #15).
* **[Audio Capture Reliability Fix]** Fixed an issue where microphones that initialize late on OBS startup would fail to attach. The plugin now uses weak source references and automatically retries attaching (#15).
* **[Keep Audio Settings on Switch]** Added a *"Keep current audio settings when switching"* checkbox so switching avatars in the library or via hotkeys preserves your microphone, threshold, and release delay (#15).
* **[Refresh Avatar List Button]** Added a button to refresh the avatar dropdowns without reopening the properties window (#15).
* **[UTF-8 Special Character Path Fix]** Fixed file loading, saving, and folder opening on Windows when folder/avatar names contain non-ASCII characters (`ö, ä, ü, ş, ğ, ç`, Japanese, Chinese, etc.).
* **[Oscillating Motion & Smooth Easing]** Added a new **Oscillating** (figure-eight sine drift) talking motion effect (#15) and smooth exponential easing/interpolation when transitioning between Talking and Idle states (#5).
* **[Optional Fixed Reference Height]** Added an optional *"Normalize Avatar Size (Fixed Height)"* toggle (off by default to preserve 100% compatibility with existing scenes) so switching between different-resolution avatars via hotkeys keeps a consistent display height (#15).
* **[Automatic Migration & Backup]** The Windows Installer now automatically detects legacy `Program Files` installations, migrates custom avatars to `ProgramData`, and keeps an untouched backup in `backup_legacy_avatars`.
* **[New Languages]** Added **Japanese (`ja-JP`)** and **Simplified Chinese (`zh-CN`)** localizations, and updated English, Turkish, and French translations (#15).

---

## ❤️ Special Thanks & Community Contributors
Huge thanks to the community members who contributed code and feedback for this update:
* **@Poste2Travail (Mardios Lambda)** — For contributing OBS Hotkeys & Avatar Slots, the audio startup auto-reconnect fix, Oscillating motion, reference height normalization, and Japanese & Chinese localizations (#15).
* **@firstof9** — For contributing cross-platform Linux & macOS build support, portable URL/folder opening (`posix_spawnp`), and Flatpak/Linux documentation (#14).
* **@ToRRent1812** & **@jojo-chaechae** — For initiating and testing Linux & macOS support (#7, #11).
* **@Suerion** — For the detailed report on legacy avatar installer migration.

---

## 📦 Downloads (Windows, macOS & Linux)
Choose the package for your operating system:

* 🪟 **Windows (`x64` — OBS Studio 33.0.0+):**
  * **`FloodTuber-Installer-1.3.0.exe`** *(Recommended)* — Automatically installs to `C:\ProgramData\obs-studio\plugins\flood-tuber`.
  * **`FloodTuber-Portable-v1.3.0.zip`** — Extract the `flood-tuber` folder into `C:\ProgramData\obs-studio\plugins\`.
* 🍏 **macOS (`Universal` — Intel & Apple Silicon):**
  * **`flood-tuber-1.3.0-macos-universal.pkg`** *(Installer)* or **`.tar.xz`** archive.
* 🐧 **Linux (`Ubuntu 24.04 x86_64`):**
  * **`flood-tuber-1.3.0-x86_64-linux-gnu.deb`** *(Debian/Ubuntu package)* or **`.tar.xz`** archive.

---

## ⚠️ Windows SmartScreen Warning (Important)
Since this project is open-source and independently developed, it does not currently hold a code signing certificate. Because of this, Windows might display a **"Windows protected your PC"** warning during installation.

1. Click on `More Info` on the warning screen.
2. Click the `Run Anyway` button that appears at the bottom right.

---

### 🔒 SHA-256 Checksums
