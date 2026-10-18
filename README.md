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

Open them from the Geode mod list, or press the **Buffer** button on the pause menu.

- **Main Buffer Window:** the default window (2, 4, 8 or 16 ticks, or Off).
- **Per-mode windows:** every mode has its own window in ticks. `-1` uses the main window, `0` turns that mode off, and `1` or more is your own value. Wave is capped at 8 ticks when set to `-1`.
- **Wave Mode:** Min Hold, Min Gap or Grid.
- **Game Speed (TPS):** **must match the TPS you actually run** (for example the value you set in Mega Hack). The mod uses it to turn ticks into real time. It does not change your game speed.
- **Hotkeys:** each one can be rebound to F1 to F12, Insert, Delete, Home, End, Page Up, Page Down, or None.
- **Debug Logging:** prints detailed events to the console.

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

Hotkeys are Windows-only and are read even when the game window is not focused.

## Known limitations

- Windows only.
- The TPS setting is entered by hand and must be kept in sync with your real TPS.
- Delayed inputs are injected by the mod, so effects that depend on the original input event may not see them at the exact moment.
- Other input mods (for example Click Between Frames) can interact with this one. If something feels off, test with the other mod disabled.

## Building from source

You need Geode CLI and SDK, CMake, and Visual Studio 2022 with the **Desktop development with C++** workload.

```
geode sdk install-binaries
geode build
```

The packaged `.geode` file is produced in the `build` folder.

## License

MIT, see [LICENSE](LICENSE).
