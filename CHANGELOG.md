# Changelog

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
