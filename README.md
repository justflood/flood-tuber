[🇬🇧 English](README.md) | [🇹🇷 Türkçe](README.tr.md)

# Flood Tuber - OBS Plugin

[![Patreon](https://img.shields.io/badge/Patreon-Support_Me-F96854?style=for-the-badge&logo=patreon&logoColor=white)](https://www.patreon.com/justflood) [![Discord](https://img.shields.io/badge/Discord-Join_the_Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/wm5qPpYe9A)

**Flood Tuber** is a lightweight OBS Studio plugin that brings your streams to life with reactive PNGTuber avatars. It detects your microphone's audio levels to animate your avatar with talking, blinking, and random action states, adding dynamism to your content without complex setups.

## Features

*   **🎙️ Audio Reactive:** Automatically switches between Idle and Talking states based on your microphone input.
*   **🖼️ 3-Frame Talking Animation:** Supports `Talk A`, `Talk B`, and `Talk C` frames for smoother and more natural talking animations.
*   **📚 Avatar Library:** Built-in library system to easily load and switch between different avatar presets (images + settings) with a single click.
*   **👀 Auto-Blink:** Adds life to your avatar with configurable automatic blinking intervals.
*   **✨ Random Actions:** Plays random action animations (like an emote or special pose) at set intervals.
*   **🌊 Motion Effects:**
    *   **Shake:** Avatar vibrates/shakes when talking.
    *   **Bounce:** Avatar bounces up and down when talking.
    *   **None:** Static talking image.
*   **🎞️ Advanced Animations:** Full support for **APNG**, **WebP**, and **GIF** formats.
    *   **Smart Detection:** Automatically detects animations even if file extensions are incorrect (e.g. APNG saved as `.png`).
*   **⚡ Talking Animation Speed:** Control how fast your avatar's mouth moves (frames/sec) with a dedicated slider.
*   **↔️ Mirror Mode:** Flip your avatar horizontally with a single checkbox.
*   **🛠️ Custom Settings per Avatar:** Each avatar in the library can have its own `settings.ini` to define specific sensitivities, speeds, and timings.

## Installation

You can download the latest version of Flood Tuber from:

*   **[Official OBS Resources Page](https://obsproject.com/forum/resources/flood-tuber-native-pngtuber-plugin.2336/)** 
*   **[GitHub Releases](https://github.com/justflood/flood-tuber/releases/latest)** (Recommended)

> **📌 OBS Studio Version Compatibility:**
> *   **OBS Studio 33.0.0 and newer:** Download **v1.2.0+** (latest release).
> *   **OBS Studio 32.x and older:** Please use **[v1.1.0](https://github.com/justflood/flood-tuber/releases/tag/v1.1.0)**.

I offer two installation methods:

### Option 1: Installer (Recommended)
1.  Download `FloodTuber-Installer-1.2.0.exe`.
2.  Run the installer.
    > **⚠️ Note regarding "Windows protected your PC":**
    > Since this is an open-source project, I do not have an expensive code signing certificate. When you run the installer, Windows SmartScreen may show a warning saying "Unknown Publisher".
    > This is completely normal for open-source software. To proceed:
    > *   Click **"More Info"**
    > *   Click **"Run Anyway"**
3.  Follow the setup wizard instructions. The installer automatically installs the plugin to the standard OBS plugin directory (`C:\ProgramData\obs-studio\plugins\flood-tuber`) and cleans up any legacy files from `C:\Program Files\obs-studio`.

### Option 2: Portable (Manual - Windows)
1.  Download `FloodTuber-Portable-v1.2.0.zip`.
2.  Extract the `flood-tuber` folder from the archive directly into `C:\ProgramData\obs-studio\plugins\` (so that `C:\ProgramData\obs-studio\plugins\flood-tuber\flood-tuber.dll` and `C:\ProgramData\obs-studio\plugins\flood-tuber\data` exist).
3.  *(If upgrading manually from v1.1.0 or older)* Delete the old `C:\Program Files\obs-studio\obs-plugins\64bit\flood-tuber.dll` and `C:\Program Files\obs-studio\data\obs-plugins\flood-tuber` folder so OBS does not load the outdated version.
4.  Restart OBS Studio.

### Option 3: Linux (Build & Install)
1. Install dependencies (Ubuntu/Debian example):
   ```bash
   sudo apt install build-essential cmake libobs-dev libwebp-dev
   ```
2. Build the plugin:
   ```bash
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   ```
3. Install to your OBS plugin directory:
   * **Native / System OBS:**
     ```bash
     mkdir -p ~/.config/obs-studio/plugins/flood-tuber/bin/64bit
     mkdir -p ~/.config/obs-studio/plugins/flood-tuber/data
     cp build/flood-tuber.so ~/.config/obs-studio/plugins/flood-tuber/bin/64bit/
     cp -r data/* ~/.config/obs-studio/plugins/flood-tuber/data/
     ```
   * **Flatpak OBS:**
     ```bash
     mkdir -p ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/flood-tuber/bin/64bit
     mkdir -p ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/flood-tuber/data
     cp build/flood-tuber.so ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/flood-tuber/bin/64bit/
     cp -r data/* ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/flood-tuber/data/
     ```
4. Restart OBS Studio.

## Usage

1.  Add a new **"Flood Tuber Avatar"** source to your scene.
2.  **Select Audio Source:** Choose your microphone from the properties list.
3.  **Load an Avatar:**
    *   Under "Avatar Library", select a preset (e.g., "Flood Tuber Avatar") and click **"Load & Apply Avatar"**.
    *   Or manually select your own images for Idle, Talk, Blink, etc.
4.  **Customize:**
    *   Adjust **Threshold** to match your voice level.
    *   Enable **Mirror** to flip the avatar if needed.
    *   Choose a **Motion Type** (Shake/Bounce) and adjust stickiness.

## Creating Your Own Avatar

> **⚠️ IMPORTANT: STRICT FILE NAMING**
> The plugin does NOT save custom file names to the settings. It strictly looks for exact filenames in the folder. **You MUST name your image files exactly as shown below**, otherwise the "Load & Apply" button will not work, and the plugin will "forget" your files when switching avatars.

1.  Click **"Open Avatars Folder"** in the properties (located at `C:\ProgramData\obs-studio\plugins\flood-tuber\data\avatars`).
2.  Create a new folder with your avatar's name (e.g., `My Cool Avatar`).
3.  Place your images inside and rename them exactly like this:
    *   `idle.png`
    *   `talk_a.png`, `talk_b.png`, `talk_c.png`
    *   `blink.png`
    *   `action.png`
    *   *(Optional)* `talk_a_blink.png` etc. for blinking while talking.
4.  *(Optional)* Copy a `settings.ini` from another avatar to fine-tune default settings for this character.
5.  In OBS, select your new folder from the list and click Load!

## 💬 Community & Support

Need help setting up your avatars, want to share your custom creations, or have a brilliant idea for a new feature? 

Join the official **Flood Tuber** Discord server! It's the best place to get direct support, report bugs, and hang out with other users.

<a href="https://discord.gg/wm5qPpYe9A">
  <img src="https://img.shields.io/badge/Join%20Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white&scale=2" alt="Join Flood Tuber Discord">
</a>

## License

This project is licensed under the GNU General Public License v2.0 (GPLv2).
