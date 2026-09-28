# The PascalPatch guide

Everything PascalPatch does and how to use it: setting it up, playing, the training plugins, making your own fighters, and fixing problems.

- [What it is](#what-it-is)
- [Set it up](#set-it-up)
- [Play](#play)
- [In game: the F2 window](#in-game-the-f2-window)
- [Plugins](#plugins)
- [The plugins, one by one](#the-plugins-one-by-one)
- [Practice recipes](#practice-recipes)
- [Every hotkey](#every-hotkey)
- [Character Studio: make your own fighters](#character-studio-make-your-own-fighters)
- [Profiles](#profiles)
- [Logs and troubleshooting](#logs-and-troubleshooting)
- [Offline, safe, legal](#offline-safe-legal)
- [Make your own plugin](#make-your-own-plugin)

## What it is

PascalPatch is a mod loader for [Melee Unlocked](https://github.com/hero88go/melee-unlocked), the native Windows version of Super Smash Bros. Melee 1.02. It works like BakkesMod does for Rocket League: your game is never modified. PascalPatch starts Melee Unlocked and loads plugins into the running game.

It has five parts:

| Part | What you use it for |
|---|---|
| **The PascalPatch app** | Pick a profile and press Play. Install, update and configure plugins. |
| **The F2 window** | In game, press F2 to see every plugin, change its settings, and read its log. |
| **Plugins** | Training tools, HUDs and stats, plus Quick Match and Unlock All. Get them from the app's Browse page or the [plugin site](https://dylan-demolder.github.io/pascalpatch-plugins/). |
| **Character Studio** | Turn your own 3D models into playable fighters, then test them in game in one click. |
| **Profiles** | A profile is one version of your game: which custom fighters and mods go in. |

Everything is offline. PascalPatch never uses Slippi or netplay, and it blocks every network request the game makes.

## Set it up

### What you need

- A Windows 10 or 11 PC (64-bit).
- **Your own Melee disc image**: NTSC 1.02 (GALE01 revision 2), as an `.iso`. PascalPatch never changes it and never includes any game data.
- **Melee Unlocked**: download `MeleeUnlocked-<version>-win64.zip` from its [releases](https://github.com/hero88go/melee-unlocked/releases) and unzip it anywhere. Version 0.7 or newer; 0.8 is the latest.

That's all. PascalPatch brings its own Python, and its in-game runtime comes built.

### Install PascalPatch

1. Download **`PascalPatch-<version>-windows.zip`** from PascalPatch's [latest release](https://github.com/Dylan-Demolder/PascalPatch/releases/latest).
2. Unzip it anywhere, for example next to your Melee Unlocked folder.
3. Open the `PascalPatch-<version>` folder and double-click **`pascalpatch.cmd`**.

Windows may say it protected your PC, because the download is new and not signed by a company. Click **More info**, then **Run anyway**. You only see this once.

The app opens in its own window. A console window opens with it: that is the app's server, and closing it closes the app. Your settings, plugins and logs live in PascalPatch's data folder (`%USERPROFILE%\.local\share\pascalpatch`), not in the unzipped folder.

### Open the app

The **Play** page lists what is left to set up, with a button for each step. On first run:

1. **Settings > melee_port.exe**: click **Browse…** and pick `melee_port.exe` in your Melee Unlocked folder, then **Save**. Leave **Game folder** empty.
2. **Profiles > New profile**: give it a name and **Browse…** to your Melee `.iso`. See [Play](#play).

Melee Unlocked 0.8 has several programs in its folder. PascalPatch works with `melee_port.exe`, the Static Recomp build, and with `melee_port_compat.exe`, the same game built for older processors. `melee_source.exe` is the new Source Port, which is built for Slippi online and cannot load plugins, and `MeleeUnlockedLauncher.exe` is Melee Unlocked's own launcher. If you pick one of those, Settings tells you which file to choose.

The game uses Melee Unlocked's own graphics and controller settings (its `port-settings.ini`), so anything you set in Melee Unlocked's launcher carries over. It does **not** use your Melee Unlocked save: PascalPatch gives the game a memory card of its own, so plugins never touch your real save.

When everything is ready the status list reads: Melee Unlocked found (with its version), PascalPatch runtime ready, offline guard on, and the number of plugins available.

### Update

Download the new release zip and unzip it in place of the old folder. Your settings, profiles' data, plugins and saves are in the data folder, so nothing is lost. Profiles themselves are in the PascalPatch folder's `profiles` folder: copy it across (or unzip over the old folder) to keep them.

Plugins from Browse update from the app: when a new version is out, its card on Browse shows **Update to** the new version.

### From source

To work on PascalPatch itself, clone the repository instead. You then need Python 3.11 or newer, Git, and Visual Studio 2022 Build Tools (with "Desktop development with C++") and CMake to build the runtime:

```bash
git clone https://github.com/Dylan-Demolder/PascalPatch
```

```bash
python tooling/native/build_plugins.py
```

Run the second command inside the `PascalPatch` folder, and again after each `git pull`. `pascalpatch.cmd` then uses your installed Python. `python tooling/release/package.py` builds the release zip.

## Play

1. **Make a profile** (once): **Profiles > New profile**. Give it a name and **Browse…** to your Melee 1.02 `.iso`. A profile with no custom fighters is plain Melee plus your plugins.
2. On the **Play** page, pick the profile and press **Play**.

The very first time, Melee asks whether to create game data on PascalPatch's memory card. Choose **Yes**; it never asks again.

Play prepares the profile (its fighters and plugins), then starts the game. **Activity** shows the progress. **Build** does only the preparing, which is handy for checking a profile after changing its fighters.

Melee Unlocked opens its own **PC settings** panel at launch, over the game. Untick **Open this panel at startup** in it if you don't want it every time, then close it. In game, F1 opens it again.

Every profile unlocks all characters and stages by default (the Unlock All plugin), without touching your memory card.

## In game: the F2 window

Press **F2** at any time for the PascalPatch window:

- a list of every plugin that is running;
- one **tab per plugin**, with its status line, its settings, and its recent log;
- a **Console** tab with everything PascalPatch and the plugins logged this session.

Changes apply on the next frame and are saved for the next launch. They are the same settings you see in the app under **Installed > Settings**.

While the F2 window is open, plugin hotkeys are switched off, so typing in it never triggers anything. Hotkeys also only work while the game window has focus.

The F2 window needs Melee Unlocked's Direct3D 12 renderer, which is the default (F1 > **Graphics backend**). On Direct3D 11, plugins still run but nothing is drawn.

## Plugins

### Install from Browse

1. **Browse** in the app, then **Refresh** to download the plugin list.
2. Read a plugin's **Details** (what it does, its settings, its hotkeys), then **Install**.
3. Press **Play**. Installed plugins load every time you play, whatever the profile.

Every download is checked twice before install: against the plugin site's signed index, and against the SHA-256 hash the index lists. Plugins from Browse are marked **verified**.

### Manage them

On the **Installed** page:

- the **switch** turns a plugin off without uninstalling it;
- **Settings** changes its settings outside the game (the same ones as in F2);
- **Uninstall** removes it. Its saved settings are kept, in case you reinstall it.

Changes apply the next time you press Play.

### Install from a file

**Install from file** takes a plugin ZIP that someone sent you, or one you built yourself. These are marked **local**, because nobody checked them. Only install ZIPs from people you trust: a plugin runs inside the game with the same access as the game itself.

### Built in and local builds

The **Built in** list holds the parts of PascalPatch that profiles use by themselves: Extra Fighters and Move Graft (custom fighters), Unlock All and Quick Match. **Local builds** only appear when you build plugins from source yourself. Play does not load those: install them from Browse or from a file.

## The plugins, one by one

Each plugin's settings are in its F2 tab. The hotkeys below are the defaults, and you can rebind any of them there.

### Quick Match: skip the menus

Starts the game straight in a match, about eight seconds after launch. There is no intro, title screen or select screen.

- In its settings, pick the two fighters, the stage (the six tournament stages), player 2 (a human port, a CPU of level 1 to 9, or nobody) and the rules: **endless** for practice (no timer, no stocks), 4 stocks, or 1 stock.
- **Backspace** restarts the match, with both fighters back at their spawn points.
- To get the normal menus back without uninstalling it, set "When the game starts" to the title screen.

### Training Lab: pause, savestates and a practice dummy

- **F5** pauses and resumes, **F6** advances one frame, **F7** toggles slow motion (half or quarter speed).
- **F9** resets percent. You can also **lock** a player at a percent, give infinite shield, or give endless stocks, to port 1, port 2, everyone, or CPUs only.
- **Savestates:** **D-pad right** or **End** saves the moment, and **D-pad left** or **Delete** puts it back: both fighters, percents, stage and timer. A savestate belongs to the match it was saved in.
- **Drills:** the state can reload on its own after each try, either when the exchange ends or after 3 or 5 seconds. A try counter keeps track.
- **The dummy:** set player 2 to a **human** port, then press **Home**. From then on the dummy plays that port through its controller:
  - it stands, crouches, shields or jumps;
  - it DIs (survival, combo, in, out, random) and SDIs;
  - it techs (in place, toward, away, never, random) and gets up the way you pick;
  - it acts on the first possible frame out of hitstun or shield stun (grab, jump, nair, up-B, shine, spot dodge, roll...);
  - it recovers to the stage and mashes out of grabs.
- **Record and replay:** **PageUp** records your inputs (up to 20 seconds). **PageDown** has the dummy play them back, once or on a loop, mirrored when it faces the other way.

### Tech Trainer: feedback on every technique

A line of feedback after each attempt:

- L-cancels ("L 5 frames early"), short hops or full hops, wavedashes (frame and angle), techs;
- out-of-shield timing, powershields, ledgedashes (intangibility left), SDI, and fastfalls (off by default).

A tally keeps your success rate for the match. **F8** resets it. You can coach one port or every human player, and switch each technique off separately.

### Wavedash Trainer: the full breakdown

After every wavedash:

- a verdict and one tip above your damage meter;
- a panel with a frame strip (jumpsquat, airborne frames, air dodge, landing lag);
- a stick gauge showing where your stick was against the angles that work;
- the angle, and the length compared to your character's best;
- the gap between chained wavedashes, and your last 16 attempts. **F12** resets the count.

### Frame Data: who is plus

A small panel per fighter shows:

- what the fighter is doing and on which frame;
- chips for hitlag, hitstun and **CAN ACT**;
- a shield bar.

After every hit or shielded attack it shows the **frame advantage**. "Jab on shield -10" means the defender can act 10 frames before you: they can punish.

### Hitbox Viewer: what the game actually checks

- Red capsules are hitboxes, purple are grabs, yellow are hurtboxes.
- Green hurtboxes are invincible, blue are intangible.

**Numpad1** shows or hides them. Pair it with Training Lab's pause (F5) and frame advance (F6).

### Info Display: the numbers behind each fighter

A lab-style panel. It shows state and frame, position, velocities, jumps left, shield, intangibility, ledge regrab timer, hitlag and hitstun, and the stick as the game reads it. A green ring flashes on a fighter the frame they can act again. **Insert** hides or shows everything.

### DI Trainer: survive more hits

While you are frozen in hitlag it draws three paths: grey is no DI, yellow is the DI you are holding, and green is the DI that survives best. A red X marks a KO. A small gauge beside you gives the verdict. **F10** shows or hides the paths.

### Combo Counter

Shows "4 HITS 37%" under the timer while a combo runs, then the total when it ends. It keeps each player's best combo of the match. It uses the same rule as Slippi's stats, so its numbers agree with replays.

### Match Stats

When a match ends, a card shows each player's:

- kills, openings, openings per kill and damage per opening;
- neutral wins, damage, L-cancel rate and inputs per minute.

**Hold F4** to see the card mid-match. Each match is also saved as one line in `match-stats.jsonl`, so you can track progress over time.

### Input Display

Draws a GameCube controller in a corner, with sticks, buttons and triggers. Pick the port, corner, size and opacity in F2.

### Unlock All

All 26 characters and every stage from the first boot. It works without touching your memory card and never plays the "new challenger" notices. Every profile has it on by default.

## Practice recipes

**Tech chasing.**

1. Quick Match on Final Destination, endless rules, player 2 human.
2. Training Lab dummy on: tech random, get up random.
3. Put your character in a grab, then press End to save.
4. Set drills to reload "when the exchange is over". Every try now starts from the grab.

**Out-of-shield options.**

1. Dummy stance shield, infinite shield on.
2. Set Training Lab's "when shield stun ends" to grab.
3. Attack its shield. Frame Data shows how minus you were, and whether the grab should have hit you.

**Kill confirms at the right percent.** Lock the dummy at the kill percent (Training Lab), set its DI to survival, and practise the confirm. Turn on DI Trainer to see how much room the DI buys.

**Edge-guarding.** Save a state with the dummy offstage (Training Lab's recovery is on by default), and drill against its recovery. Hitbox Viewer shows when its up-B is intangible.

**Wavedashes and L-cancels.** Tech Trainer for the running tally, and Wavedash Trainer for the full breakdown of each wavedash. Slow motion (F7) helps learn the rhythm first.

**Learning a matchup.** Record the opponent's pressure string yourself (PageUp), have the dummy play it back (PageDown), and practise your answers. Use Frame Data to find the gaps.

## Every hotkey

| Key | What it does |
|---|---|
| F1 | Melee Unlocked's PC settings |
| F2 | PascalPatch's plugin window |
| F11 | Melee Unlocked's compact menu |
| F4 (hold) | Match Stats: the live card |
| F5 / F6 / F7 | Training Lab: pause, frame advance, slow motion |
| F8 | Tech Trainer: reset the tally |
| F9 | Training Lab: reset percent |
| F10 | DI Trainer: paths on or off |
| F12 | Wavedash Trainer: reset the count |
| End / Delete | Training Lab: save state / load state (also D-pad right / left) |
| Home | Training Lab: dummy on or off |
| PageUp / PageDown | Training Lab: record / play back |
| Insert | Info Display: on or off |
| Numpad1 | Hitbox Viewer: on or off |
| Backspace | Quick Match: restart the match |

Rebind any plugin key in its F2 tab: click the key, then press the new one. Esc unbinds it.

## Character Studio: make your own fighters

Character Studio turns your own 3D model into a Melee fighter. The model takes over a base fighter's skeleton and animations, and gets its own moves and stats. It comes with PascalPatch: press **Character Studio** at the top of the app. It reads the base fighters from the disc your profile uses, so make a profile first.

It comes with six example characters: Glacier, Sir Nova, Bolt-9, Umbra, Cinder and Chungus. To play them, open **Roster** in the studio, press **+ All examples**, then **Build & play**. To see how one is made, click it under **Examples** on the start screen: that opens your own copy, and the original stays as it came.


1. **New project**: drop in a `.glb`, an `.obj`, or a `.zip` of a `.gltf` with its textures. Pick a name, the base fighter whose skeleton and moves it uses, and which way the model faces.
2. Work through the modes (keys 1 to 7):
   - **Fit**: drag the skeleton's joints onto your model;
   - **Body parts**: paint which bone each area follows (fixes capes, hair, baggy clothes);
   - **Sculpt**: brushes for shape tweaks;
   - **Animate**: watch the base fighter's real animations on your model;
   - **Moves**: pick each special from any fighter, borrow normal attacks, and retune hitboxes, with frame data and KO percents for every move;
   - **Stats**: attribute sliders, compared with the whole cast;
   - **Build**: a check for problems, then the build.
3. **Test in game** (in Build): builds the fighter and starts the game straight in a match with it. You pick the opponent, whether player 2 is a human, a Training Lab dummy or a CPU, and the stage. It uses Quick Match, so there are no menus.
4. **Roster**: put several characters together, then **Build & play**. They appear as new fighters on the character select screen, which pages to show them, with their own portraits and stock icons. No original fighter is replaced.

Projects autosave. **File > Save as** copies a project to a new folder.

## Profiles

A profile is a JSON file under `PascalPatch/profiles/`, holding the game version, your ISO, the custom fighters, and some options. The app creates and edits them for you:

- **Profiles** lists them. Each has **Play**, **Build**, **Delete**, its fighters and its mods.
- Character Studio's roster build writes a profile with every fighter in it.
- Test in game uses its own `<character>-studio-test` profile, with a Quick Match set up.

The built game for each profile lives in PascalPatch's data folder, never beside your ISO.

## Logs and troubleshooting

**Activity** in the app lists builds and launches from this session, and every log the game wrote. The `.pascalpatch.log` files are PascalPatch's own log: which plugins loaded, and anything they reported. In game, the F2 window's Console tab shows the same lines live.

| Problem | Try |
|---|---|
| The Play page says Melee Unlocked is missing | Settings: **Browse…** to `melee_port.exe` in your Melee Unlocked folder. |
| The Play page says the runtime is not built | The release zip includes it: check the `bin` folder was unzipped too. From source, run `python tooling/native/build_plugins.py` in the PascalPatch folder. |
| The game stops at boot with "OSPanic" in its log | Update PascalPatch: versions before 0.5 did not pass Melee Unlocked's release folders to the game. |
| `pascalpatch.cmd` says it needs Python | Unzip the whole download first: opening it from inside the zip leaves its own Python behind. |
| The game starts, but F2 does nothing | Melee Unlocked must use Direct3D 12 (F1 > Graphics backend). Click the game window so it has focus. |
| A plugin does nothing | Is it switched on (Installed)? Are you in a match? Check its F2 tab for "failed to load" and its log. |
| A plugin says it needs a newer PascalPatch | Download the latest release (from source: `git pull`, then rebuild the runtime). |
| A hotkey does nothing | The F2 window must be closed and the game window focused. Check the key in the plugin's F2 tab for a clash with another plugin. |
| The dummy does not move | Its port must be a **human** player (Quick Match: "Player 2 is: a human player"). |
| A savestate won't load | States only load into the match they were saved in. |
| Melee Unlocked's settings cover the game at every launch | Untick **Open this panel at startup** in that panel. |

To report a problem, open an issue on [PascalPatch](https://github.com/Dylan-Demolder/PascalPatch/issues) with the `.pascalpatch.log` from Activity. For a plugin, include its name and version.

## Offline, safe, legal

- **Offline only.** The runtime refuses every network request the game makes, and PascalPatch hides Slippi's login. Never use PascalPatch for netplay.
- **Your game, untouched.** Melee Unlocked runs unmodified, and your ISO is only read. Builds and saves stay in PascalPatch's own folders.
- **Checked downloads.** Browse installs only what the signed index lists, and only when the file's hash matches.
- **No game data.** PascalPatch and its plugins ship no Nintendo files. You bring your own disc.

## Make your own plugin

PascalPatch is community developed. Anyone can build a plugin, try it in their own game within minutes, and get it listed on Browse for everyone. Start with the [plugin guide](plugins/README.md).
