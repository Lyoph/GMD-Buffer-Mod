# Input Buffer

A Geode mod for Geometry Dash 2.2081 (Windows) that makes tight timings easier.

If you click slightly too early, the mod remembers the click and fires it the moment you land or touch an orb. In ship, UFO, swing and wave it makes very quick taps come out consistent.

> **Note:** this is an input assist. Use it for practice and fun, not for leaderboard or submitted runs.

## What it does

| Mode | Behavior |
|---|---|
| Cube, Ball, Robot, Spider | Tap a little before landing or touching an orb, let go, and the click fires when you touch the surface or ring. Clicks you are still holding are left to the normal game behavior. |
| Ship, UFO, Swing | A very short tap is stretched to a minimum length, so fast taps are all the same length. |
| Wave | Three styles: **Min Hold**, **Min Gap** and **Grid**. See below. |
| 2 Player | Both icons are buffered with the same rules, each by its own mode. |
| Dual portal (experimental) | Off by default. When turned on, each icon in a dual section is buffered by its own mode, so mixed modes such as cube + wave work. When off, dual sections use normal game input. |

Pads fire on touch in the normal game, so they need no buffering.

### Wave modes

- **Min Hold:** a very short tap is stretched to at least the window length. Quick taps feel slightly longer than you pressed.
- **Min Gap:** after you let go, the next press must wait a minimum time. A press that comes too soon is delayed, which can feel like a late click.
- **Grid:** every press and release waits for the next beat of a fixed timing grid. Most consistent, but adds a small delay to every input.

Switching wave mode in the middle of a wave section can cause one odd click, so change it while paused.

## Install

1. You need Geometry Dash 2.2081 on Windows with the [Geode](https://geode-sdk.org) loader installed.
2. Download `lyophi.buffermod.geode` from the [Releases](../../releases) page.
3. Put it in the `geode/mods` folder inside your Geometry Dash folder, then restart the game.

The mod appears as **Input Buffer** in the Geode mod list.

## Settings

Open them from the Geode mod list, or press the **Buffer** button on the pause menu. The **Profiles** button next to it saves and loads sets of settings.

- **Main Buffer Window:** the default window (2, 4, 8 or 16 ticks, or Off).
- **Per-mode windows:** every mode has its own window in ticks. `-1` uses the main window, `0` turns that mode off, and `1` or more is your own value. Wave is capped at 8 ticks when set to `-1`.
- **Wave Mode:** Min Hold, Min Gap or Grid.
- **Game Speed (TPS):** type the TPS you actually run (for example the value you set in Mega Hack), any whole number from 1 to 10000. The mod uses it to turn ticks into real time. It does not change your game speed.
- **Dual Mode Support (Experimental):** off by default. See above.
- **Hotkeys:** Geode keybind settings. Click a box and press any key or combination, or clear it to turn that hotkey off.
- **Debug Logging:** prints detailed events to the console.

### Profiles

Press **Profiles** on the pause menu to save your current timing settings under a name, load or delete saved ones, and link a profile to the level you are playing. A linked profile is applied automatically when that level loads, so you can keep different settings for different levels.

A profile stores the Main Buffer Window, every per-mode window, the Wave Mode, the Game Speed (TPS) and the Dual Mode setting. It does not store hotkeys or debug logging. Loading a profile changes your current settings. When a level applies its linked profile, your own settings from before are put back when you leave the level (and after a crash, the next time the game starts).

### About ticks

A tick is 1/240 of a second (about 4 ms):

| Ticks | About |
|---|---|
| 2 | 8 ms, half a frame at 60 FPS |
| 4 | 17 ms, 1 frame at 60 FPS |
| 8 | 33 ms, 2 frames |
| 16 | 67 ms, 4 frames |

Smaller windows feel more natural. Larger windows are more forgiving but can fire clicks earlier or later than you meant.

### Default hotkeys

| Key | Action |
|---|---|
| F6 | Cycle the main window |
| F7 / F8 | TPS up / down |
| F9 | Toggle debug logging |
| F10 | Cycle wave mode |

Hotkeys only act while you are inside a level, and you can rebind them in the settings.

## Known limitations

- The mod is built for Windows. Hotkeys use Geode's keybind system, but other platforms are untested.
- The TPS setting is typed by hand and must be kept in sync with your real TPS.
- Dual portal support is experimental and off by default. In levels with mixed modes on the two icons, expect some rough edges.
- Delayed inputs are injected by the mod, so effects that depend on the original input event may not see them at the exact moment.
- Other input mods (for example Click Between Frames) can interact with this one. If something feels off, test with the other mod disabled.

## Building from source

You need Geode CLI and SDK, CMake, and Visual Studio 2022 with the **Desktop development with C++** workload.

```
geode sdk install-binaries
geode build
```

The packaged `.geode` file is produced in the `build` folder. Releases are built by GitHub Actions from the pinned SDK and bindings version in `.github/workflows`.

If a local build fails inside `GeodeBindings`, the bindings repo's `main` branch may be ahead of your SDK version. Pin the bindings to the commit used in the workflow file (`bindings-ref`), point `GEODE_BINDINGS_REPO_PATH` at that checkout, and build with `geode build -- -DSKIP_BUILDING_CODEGEN=OFF`.

## License

MIT, see [LICENSE](LICENSE).

### Level sections (experimental)

Turn on **Level Sections (Experimental)** in the mod settings, then open **Pause > Buffer > Advanced > Sections** inside a level. Each section has its own **start** and **end** percent (two decimals) and its own copy of the settings, which you edit right in the window on three pages: Window (main window, TPS, dual), Modes (ticks per mode) and Styles (wave, ship, UFO, swing). You can also copy a saved profile into a section. Parts of the level that no section covers follow **Empty sections**: the level's linked profile, your normal settings, or buffer off. If sections overlap, the one that starts last wins.

Ways to set a percent: type it, press **Add** (starts at the percent where you last died), or press the **Add Section Here** key (F11 by default) while playing or in practice mode. Sections do not work in platformer levels. Your own settings are restored when you leave the level.

### Simple and advanced settings

The **Buffer** button on the pause menu opens a small window. **Basic** has the Quick setup and your TPS; most people only need this. **Advanced** has the experimental switches and links to Profiles, Sections and the full settings page.

**Experimental timing styles:** ship, UFO and swing can use Min Hold, Min Gap or Grid like the wave. Grid delays clicks to a fixed beat and can make some timings impossible to hit, so leave it off unless you are testing.

Some lists may not accept input-assist mods. Check your list's rules before submitting a completion.

**Copying sections:** the Sections window can copy a level's sections (and its linked profile) to another level ID. This is recommended for moving your setup from a copy of the level to the main level. It is not experimental and works even if the Level Sections setting is off.
