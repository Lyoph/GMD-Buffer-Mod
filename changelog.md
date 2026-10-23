## v1.1.0
- **Fixed:** the buffer did not help when you jumped with a **Jump P2** keybind in a normal one-player level. The game applies that input to player 1, but the mod was helping the hidden second icon, so clicks never got buffered. Thanks to manty987 for the report and the fix.
- **New Buffer menu:** the pause menu's Buffer button now opens a simple window with **Basic** and **Advanced** tabs. Basic has a Quick setup (Off, Strict, Standard, Relaxed, Very relaxed) and your TPS. Advanced has the experimental switches, shortcuts to Profiles, Sections and all settings, and Reset all.
- **Experimental timing styles for ship, UFO and swing:** Min Hold (default), Min Gap or Grid, the same styles the wave has. Off unless you change them.
- **Copy sections to another level** from the Sections window (also copies the level's linked profile). Recommended for moving from a copy of a level to the main level. Not experimental.
- **Level sections (experimental):** split a level by percent (two decimals). Each section has a start, an end and its own settings that you edit in the Sections window, with a choice for parts no section covers. Press **Add** and choose where it starts: a typed percent, your last death, or your current position. The Add Section Here key (F11) adds one at your current position.
- **Profiles:** save your timing settings under a name and switch between them with the new **Profiles** button on the pause menu.
- **Per-level profiles:** link a profile to a level and it is applied automatically when that level loads. When you leave the level, your own settings are restored (also after a crash, the next time the game starts).
- **Typed Game Speed (TPS):** type any TPS from 1 to 10000 instead of picking from a list. Your TPS resets to 240 once, so set it again to match your game.
- **Hotkeys now use Geode keybinds:** rebind them in the settings with any key or combination. They no longer depend on Windows-only key reading. Your old hotkey choices reset to the defaults (F6 to F10).
- **Dual Mode Support (Experimental):** new setting, off by default. When on, each icon in a dual section is buffered by its own game mode, so mixed modes such as cube + wave work. When off, dual sections use normal game input.

## v1.0.2
- Same mod as v1.0.1, now built automatically by GitHub Actions instead of on a local PC.
- No gameplay or settings changes. Your saved settings carry over.
- Builds are pinned to Geode 5.11.2 and its matching bindings, so every release is reproducible.

## v1.0.1
- Fixed first clicks being dropped after death, restart or checkpoint.
- Fixed dual-portal sections so held-back inputs reach both icons.
- Fixed wave Grid quick taps cancelling out. Taps now hold at least one cell.
- Wave Grid and Min Gap are skipped for about 16 ticks after a spawn, so start-pos clicks go straight through.