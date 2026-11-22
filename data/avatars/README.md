# Flood Tuber Avatar Package – English

This package contains ready-to-use avatar folders for the **Flood Tuber** OBS Studio plugin (based on the enhanced version by Poste2Travail).

## How to use
1. Download and install Flood Tuber (see official repo).
2. Extract this ZIP.
3. Copy the avatar folders into:
   `C:\ProgramData\obs-studio\plugins\flood-tuber\data\avatars\`
   (or use the “Open Avatars Folder” button in the plugin).
4. In OBS, add a **Flood Tuber Avatar** source, select an avatar from the library and click **Load & Apply**.

## Strict file naming
The plugin only looks for exact filenames:
- `idle.png` (required)
- `talk_a.png`, `talk_b.png`, `talk_c.png`
- `blink.png`, `action.png`
- Optional talking+blink variants: `talk_a_blink.png`, etc.

## Creating your own avatars
Use these free tools to generate characters with the needed expressions:
- https://charat.me
- https://picrew.me

Export the different mouth/eye states and rename them exactly as above.

## Superposition tool
Use the companion Sprite Comparator (static site) to overlay your files and check alignment / crop before importing.

## Included example avatars
Flood Tuber Avatar, Gamer Robot, Streamer Boy, Streamer Girl, The Neon Oni, The Outlaw, The Retro TV.

Each folder contains a sample `settings.ini`.
