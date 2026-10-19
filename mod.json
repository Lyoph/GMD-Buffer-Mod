{
    "geode": "5.11.2",
    "gd": {
        "win": "2.2081"
    },
    "id": "lyophi.buffermod",
    "name": "Input Buffer",
    "version": "v1.0.1",
    "developer": "Lyophi",
    "description": "Predictive input buffering for Geometry Dash. Early clicks are remembered and fired when you land or touch an orb, and ship, UFO, swing and wave get smoother, more consistent timing.",
    "settings": {
        "guide": {
            "type": "title",
            "name": "How to read the timing numbers",
            "description": "Timing windows are measured in **ticks**. A tick is one tiny step of the game's physics. There are 240 ticks per second, so:\n\n**1 tick** = about 4 milliseconds\n**2 ticks** = about 8 ms\n**4 ticks** = about 17 ms (1 frame at 60 FPS)\n**8 ticks** = about 33 ms (2 frames)\n**16 ticks** = about 67 ms (4 frames)\n**32 ticks** = about 133 ms (8 frames)\n\nOn every window setting:\n**-1** = use the Main Buffer Window\n**0** = off, raw input with no help\n**1 or more** = your own value\n\nSmaller windows are stricter and feel more natural. Larger windows are more forgiving but can fire clicks earlier or later than you meant."
        },
        "stage": {
            "type": "string",
            "name": "Main Buffer Window",
            "description": "The default window for every mode that is set to -1 below.\n\n**2 ticks** = about half a frame (very strict)\n**4 ticks** = about 1 frame (a good starting point)\n**8 ticks** = about 2 frames\n**16 ticks** = about 4 frames (very forgiving)\n**Off** = no default window\n\nA bigger window is more forgiving, but a click meant for later might fire sooner than you wanted.",
            "default": "4 ticks",
            "one-of": ["2 ticks", "4 ticks", "8 ticks", "16 ticks", "Off"]
        },
        "sec-ground": {
            "type": "title",
            "name": "Landing and orb modes",
            "description": "Cube, ball, robot and spider.\n\nThese windows set how long before landing or touching an orb a click is still remembered. If you tap a little early and let go, the click fires the moment you touch the ground or orb."
        },
        "cube-ticks": {
            "type": "int",
            "name": "Cube Window (ticks)",
            "description": "How early you can click before landing or touching an orb in cube and still have it count.\n\n**-1** = same as Main Buffer Window\n**0** = off\n**1 or more** = your own value\n\nExamples: 4 = about 1 frame early, 8 = about 2 frames early, 16 = about 4 frames early.",
            "default": -1,
            "min": -1,
            "max": 64,
            "control": { "input": true, "arrows": true, "slider": false }
        },
        "ball-ticks": {
            "type": "int",
            "name": "Ball Window (ticks)",
            "description": "How early you can click before touching a surface or orb in ball and still have it count.\n\n**-1** = same as Main Buffer Window\n**0** = off\n**1 or more** = your own value\n\nExamples: 4 = about 1 frame early, 8 = about 2 frames early, 16 = about 4 frames early.",
            "default": -1,
            "min": -1,
            "max": 64,
            "control": { "input": true, "arrows": true, "slider": false }
        },
        "robot-ticks": {
            "type": "int",
            "name": "Robot Window (ticks)",
            "description": "How early you can click before landing or touching an orb in robot and still have it count.\n\n**-1** = same as Main Buffer Window\n**0** = off\n**1 or more** = your own value\n\nExamples: 4 = about 1 frame early, 8 = about 2 frames early, 16 = about 4 frames early.",
            "default": -1,
            "min": -1,
            "max": 64,
            "control": { "input": true, "arrows": true, "slider": false }
        },
        "spider-ticks": {
            "type": "int",
            "name": "Spider Window (ticks)",
            "description": "How early you can click before touching a surface or orb in spider and still have it count.\n\n**-1** = same as Main Buffer Window\n**0** = off\n**1 or more** = your own value\n\nExamples: 4 = about 1 frame early, 8 = about 2 frames early, 16 = about 4 frames early.",
            "default": -1,
            "min": -1,
            "max": 64,
            "control": { "input": true, "arrows": true, "slider": false }
        },
        "sec-fly": {
            "type": "title",
            "name": "Flying modes",
            "description": "Ship, UFO, swing and wave.\n\nThere is no ground to wait for here, so these windows work differently. In ship, UFO and swing, a very quick tap is made to last at least this long, so fast taps come out the same length every time. In wave, the window is used by the Wave Mode you pick."
        },
        "ship-ticks": {
            "type": "int",
            "name": "Ship Window (ticks)",
            "description": "The shortest a tap can be in ship. A quicker tap is stretched to this length.\n\n**-1** = same as Main Buffer Window\n**0** = off, raw input\n**1 or more** = your own value\n\nExamples: 2 = taps last at least about half a frame, 4 = at least about 1 frame.\n\nBigger values can make quick taps feel slower or heavier.",
            "default": -1,
            "min": -1,
            "max": 64,
            "control": { "input": true, "arrows": true, "slider": false }
        },
        "ufo-ticks": {
            "type": "int",
            "name": "UFO Window (ticks)",
            "description": "The shortest a tap can be in UFO. A quicker tap is stretched to this length.\n\n**-1** = same as Main Buffer Window\n**0** = off, raw input\n**1 or more** = your own value\n\nExamples: 2 = taps last at least about half a frame, 4 = at least about 1 frame.\n\nBigger values can make quick taps feel slower or heavier.",
            "default": -1,
            "min": -1,
            "max": 64,
            "control": { "input": true, "arrows": true, "slider": false }
        },
        "swing-ticks": {
            "type": "int",
            "name": "Swing Window (ticks)",
            "description": "The shortest a tap can be in swing. A quicker tap is stretched to this length.\n\n**-1** = same as Main Buffer Window\n**0** = off, raw input\n**1 or more** = your own value\n\nExamples: 2 = taps last at least about half a frame, 4 = at least about 1 frame.\n\nBigger values can make quick taps feel slower or heavier.",
            "default": -1,
            "min": -1,
            "max": 64,
            "control": { "input": true, "arrows": true, "slider": false }
        },
        "wave-ticks": {
            "type": "int",
            "name": "Wave Window (ticks)",
            "description": "How strong the wave timing help is. The meaning depends on the Wave Mode below:\n\n**Min Hold**: the shortest a tap can be\n**Min Gap**: the shortest wait between letting go and pressing again\n**Grid**: the size of the timing grid (every click and release waits for the next grid line)\n\n**-1** = same as Main Buffer Window, but never more than 8 ticks\n**0** = off, raw input\n**1 or more** = your own value (not capped)\n\n2 or 4 is a good place to start. Large values make wave more consistent but much less responsive, and can feel very laggy.",
            "default": -1,
            "min": -1,
            "max": 64,
            "control": { "input": true, "arrows": true, "slider": false }
        },
        "wave-mode": {
            "type": "string",
            "name": "Wave Mode",
            "description": "Picks how the wave timing help works. The Wave Window above sets how strong it is.\n\n**Min Hold**: a very short tap is stretched to at least the window length. Good if your quick taps come out too short or uneven.\nWatch out: quick taps feel slightly longer than you pressed, and fast spam can feel sticky.\n\n**Min Gap**: after you let go, the next press has to wait a minimum time. Good for fast zigzags that bunch up.\nWatch out: a press that comes too soon is delayed, which can feel like a late or skipped click.\n\n**Grid**: every press and release waits for the next beat of a fixed timing grid, so everything lines up evenly. Gives the most consistent result.\nWatch out: adds a small delay to every click and release, up to the window size. It can feel laggy with large windows.\n\nSwitching modes in the middle of a wave section can cause one odd click, so change it while paused.",
            "default": "Grid",
            "one-of": ["Min Hold", "Min Gap", "Grid"]
        },
        "sec-keys": {
            "type": "title",
            "name": "Hotkeys",
            "description": "Keys that work while you are in a level. Pick **None** to disable one. If two actions share a key, both will trigger."
        },
        "key-stage": {
            "type": "string",
            "name": "Key: Cycle Main Window",
            "description": "Steps the Main Buffer Window through 2, 4, 8, 16 ticks and Off.",
            "default": "F6",
            "one-of": ["None","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","Insert","Delete","Home","End","Page Up","Page Down"]
        },
        "key-tps-up": {
            "type": "string",
            "name": "Key: TPS Up",
            "description": "Moves the Game Speed (TPS) setting up one preset. Remember it must match your real TPS.",
            "default": "F7",
            "one-of": ["None","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","Insert","Delete","Home","End","Page Up","Page Down"]
        },
        "key-tps-down": {
            "type": "string",
            "name": "Key: TPS Down",
            "description": "Moves the Game Speed (TPS) setting down one preset.",
            "default": "F8",
            "one-of": ["None","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","Insert","Delete","Home","End","Page Up","Page Down"]
        },
        "key-debug": {
            "type": "string",
            "name": "Key: Debug Logging",
            "description": "Turns debug logging on or off.",
            "default": "F9",
            "one-of": ["None","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","Insert","Delete","Home","End","Page Up","Page Down"]
        },
        "key-wave": {
            "type": "string",
            "name": "Key: Cycle Wave Mode",
            "description": "Switches between Min Hold, Min Gap and Grid. Best used while paused.",
            "default": "F10",
            "one-of": ["None","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","Insert","Delete","Home","End","Page Up","Page Down"]
        },
        "sec-general": {
            "type": "title",
            "name": "General",
            "description": "Settings that apply to everything."
        },
        "tps": {
            "type": "string",
            "name": "Game Speed (TPS)",
            "description": "Must match the TPS you set in Mega Hack, or 240 if you have not changed it.\n\nThe mod uses this to turn ticks into real time. If it is wrong, every window will be the wrong size: too short if you set it lower than your real TPS, too long if you set it higher.\n\nThis does not change your game's speed. It only tells the mod what speed you are running.",
            "default": "240",
            "one-of": ["60", "120", "240", "360", "480", "720", "960", "1000"]
        },
        "debug": {
            "type": "bool",
            "name": "Debug Logging",
            "description": "Prints detailed messages to the console about what the buffer is doing. Only useful for troubleshooting. Leave it off for normal play.",
            "default": false
        }
    }
}