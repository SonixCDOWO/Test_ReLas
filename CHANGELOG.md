# Changelog

## 0.2.2 (2026-10-17)

From the second patch in [issue #1](https://github.com/raulzrrs/letsgo-recomp/issues/1), thanks again to
@porstobotgovna:

- **No more crashes on the communication screens.** The network services (`nifm`, `ssl`, `bsd`, `ldn`) didn't exist:
  the game got an empty session and closed when opening local wireless, Mystery Gift or internet co-op. They now
  answer like a console that is offline. Tested on both games (Eevee and Pikachu 1.0.0): **local trade** searches for
  a player and B goes back; **Mystery Gift** stays on "Connecting..." (that screen has no way out offline: the game
  waits with no timeout and ignores B, as on a console without Nintendo's servers); the **Pokémon GO settings**
  search and B goes back to the menu.
- The other Bluetooth (`btm:u`) commands answer with no data, like a console with nothing paired.

## 0.2.1 (2026-10-17)

From [issue #1](https://github.com/raulzrrs/letsgo-recomp/issues/1), with thanks to @porstobotgovna, who played
*Let's Go, Pikachu!* 1.0.2 on an AMD RX 7800 XT with Windows 10 and sent a patch:

- **Supports *Let's Go, Pikachu!*** (and updated games): the launcher accepts either game by its program id, shows
  which one is set up, keeps Pikachu's saves in `save-pikachu\` and installs translations for the set-up game.
  Tested here with Pikachu 1.0.0: setup (13 to 17 minutes), a new game through the title movie, Professor Oak and the
  name screen, and played (from a copy of an Eevee save) with 60 fps, wild battles and the PT-BR translation. Each
  folder holds one set-up game: to have both, unzip the release twice. **60 fps and wild battles work on Pikachu 1.0.0 too:** their hooks and the generator's instruction patches
  are addresses in the game's executable, now in a table per build (the same functions in Pikachu, found by comparing
  the code instruction by instruction, `tools/addrmap.py`). Other builds (the updates) get neither (greyed out on the
  start screen). The game is told its real version (1.0.0 / 1.0.2) instead of always 1.0.0.
- **Fixes a crash at boot (`c0000005` accessing `0x38`) on some Windows 10 machines**, whose Yu Gothic Regular font
  breaks the game's font engine: Yu Gothic Medium and MS Gothic come first now. `LARECOMP_FONT=<file>` overrides it.
- **Pokémon GO Park settings:** the game waited on a Bluetooth pairing event that was never created and aborted; the
  event exists now, and a wait on an empty handle slot waits on the others.
- **Fixes wild battles with 60 fps on (0.2.0, both games):** the battle counted as a menu and got in-between frames;
  it runs at 30 fps like the game's own battles now.
- Flat dumps (the code files next to the RomFS's `bin/`) are set up too.
- The overlay (close the game?, name entry) speaks English, Portuguese or Russian, from the system language
  (`LARECOMP_LANG=en|pt|ru`).
- Crash reports list the last calls to the host, and the previous run's log is kept as `lasrecomp-prev.log`.
- Tested with the zip downloaded from this release, unzipped twice without development tools on PATH: Eevee 1.0.0
  and Pikachu 1.0.0 set up from `exefs` + `romfs.bin` / `romfs` (13 minutes each), an Eevee save in Viridian City with
  the PT-BR translation and 60 fps in the world, and a new Pikachu game with the translation installed from its `.rar`
  (Professor Oak's lines in Portuguese).

## 0.2.0 (2026-10-17)

Two experimental options on the start screen, both off by default:

- **60 fps (experimental).** The overworld runs at 60 fps natively: the game's own time step is halved there, so
  walking, the camera and the world's animations are smooth at the right speed. Menus and the Pokédex keep the game's
  30 updates per second and are shown at 60 with an in-between frame. Battles and captures stay at 30 fps, as the game
  made them (their timelines count frames; an in-between frame there was tried and left out). Tested by playing:
  world, menus and Pokédex at 60 without speed-ups; battles and captures at normal speed.
- **Wild battles (experimental).** Touching a wild Pokémon starts a classic battle against it (its species, level,
  gender, IVs and level-up moves) and then the capture, like the game does with Snorlax and the legendary birds.
  Played on Route 1. Not done yet: split experience, nature and shiny of the touched Pokémon, regional forms, Pokémon
  with an aura; other areas not tested.

Also:

- **Controller stick dead zone**, adjustable on the start screen ("Keyboard and mouse"): a stick at rest no longer
  moves the Poké Ball aim.
- Known issue, also in 0.1.2: the colored aura of special wild Pokémon sometimes disappears for a single frame
  ([docs/PROGRESSO.md](docs/PROGRESSO.md), item 6).
- Tested with the zip downloaded from this release, in a new folder without development tools on PATH: setup from a
  game folder with `exefs` and `romfs` (13 minutes to recompile on a 20-thread CPU), a new game through the controller
  screen, the title movie, Professor Oak and the character and name screens, and the Brazilian Portuguese fan
  translation installed from its downloaded `.rar` (Professor Oak's lines in Portuguese). With both options on (another
  clean folder): the world at 60 fps and a classic battle started by touching a wild Pidgey.

## 0.1.2 (2026-10-16)

- **Fixes a crash at the title screen on a new game** (no save yet) in 0.1.1: the movie player looked for the
  game's movies in an extracted `romfs` folder, which the release's setup doesn't create (it builds the `romfs.bin`
  image). Movies are now read from inside the image.
- Tested from a clean folder with the downloaded zip: setup, a new game through the controller and language screens
  into the introduction, and the Brazilian Portuguese translation installed from the `.rar`.

## 0.1.1 (2026-10-16)

- **Fixes the game closing right after the controller screen** ("svcBreak") in the 0.1.0 release: its FFmpeg was
  built without the Opus decoder the game's music and sound effects use. The release build now checks every decoder
  the game needs, and a missing one is reported by name instead of crashing.
- The one-time setup's progress bar no longer reaches 100% long before the end (it counted the four game modules as
  two).
- The README states exactly which game files and version are supported, with the sources.

## 0.1.0 (2026-10-15)

First public release.

- Native Windows port of *Pokémon: Let's Go, Eevee!* v1.0.0 by static recompilation (built on
  [LAS Recomp](https://github.com/peiikai/LASRecomp)): the game's four executable modules are translated from ARM64
  to C and compiled to x86-64; no ARM64 interpreter or JIT at run time.
- Release zip with `LetsGoRecomp.exe`: pick your extracted game folder in the launcher and the game code is
  recompiled on your PC (bundled generator and Zig compiler; nothing else to install).
- Stable 30 fps, 720p to 4K internal resolution (with supersampling), SMAA, 16x anisotropic filtering, any window
  size, fullscreen; parallel shader translation with disk caches.
- Keyboard and mouse (rebindable; the mouse aims Poké Balls and is the touch screen), any SDL 3 controller, virtual
  finger with the right stick for Play with Eevee.
- Atmosphere-style LayeredFS mods; one-click install of fan translations from the downloaded .zip / .rar / .7z.
- Launcher in English and Brazilian Portuguese.
