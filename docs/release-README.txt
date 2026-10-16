Let's Go Recomp
===============

A native Windows port of Pokemon: Let's Go, Eevee! (Nintendo Switch, v1.0.0) by static recompilation.
Project page, full documentation and source code: https://github.com/raulzrrs/letsgo-recomp

NO GAME FILES ARE INCLUDED. You need your own legally obtained copy of Pokemon: Let's Go, Eevee! version 1.0.0,
extracted (the exefs and romfs folders).

Requirements
------------
- Windows 10 or 11, 64-bit
- A CPU with AVX2 (Intel 4th gen / AMD Ryzen or newer)
- A GPU with a Vulkan 1.3 driver (NVIDIA GTX 900 / AMD RX 400 / Intel Arc or newer, up-to-date drivers)
- About 8 GB of free disk space (the game files are copied here, plus the game code built from them)

How to play
-----------
1. Unzip this folder anywhere you can write to (e.g. D:\Games\LetsGoRecomp). Not inside Program Files.
2. Open LetsGoRecomp.exe.
3. Click "Select game folder..." and pick the folder with your extracted game (the one that holds exefs and romfs,
   or any folder above it).
4. Wait while the game files are copied and the game code is recompiled for your PC. This happens only once and
   takes a while (about 16 minutes on a 20-thread CPU, longer with fewer threads).
5. Click Play.

Everything stays in this folder: settings (larecomp.ini), saves (save\), the game files (extracted\), translations
(mods\) and the log (lasrecomp.log). To move or back up the game, move or copy the whole folder.

Translations
------------
Click "Install a translation..." on the start screen and pick the translation you downloaded (.zip, .rar or .7z, as
it came; packs made for Atmosphere / LayeredFS work). Then choose it under "Translation".

Portugues (Brasil): para jogar em portugues, baixe a traducao PT-BR de fas do grupo Pokemon Switch Brasil, clique em
"Instalar traducao..." e escolha o arquivo baixado (.rar ou .zip, do jeito que veio). Depois selecione-a em "Traducao".
O programa abre em portugues quando o Windows esta em portugues.

Controls
--------
Keyboard: WASD walk, Space/Enter = A, Q/Backspace = B, Tab = X (menu), R = Y, Z/C = L/R, = and - = Plus/Minus,
arrows = D-pad, Esc = close. All keys can be changed on the start screen.
Mouse: aims Poke Balls (like the Switch's motion controls), left button throws; outside of aiming, the left button is
a finger on the touch screen (Play with Eevee). Right button = B.
Controllers: any controller SDL recognizes (Xbox, PlayStation, Switch Pro...), handheld-mode layout. The right stick
plus ZR is a finger on the touch screen.

Troubleshooting
---------------
- "Not Pokemon: Let's Go, Eevee! version 1.0.0": updates are not supported; use the base game files.
- Packed game files (.xci / .nsp) must be extracted first.
- The game closes or doesn't start: see lasrecomp.log in this folder and report it on the project page.

Legal
-----
Fan project, not affiliated with Nintendo, The Pokemon Company, Game Freak or Creatures. MIT licensed (LICENSE.txt),
based on LAS Recomp by Peiikai (LICENSE-LASRecomp.txt, https://github.com/peiikai/LASRecomp).
Bundled: SDL3 (zlib), FFmpeg (LGPL 2.1, built with only the decoders the game needs; source: https://ffmpeg.org),
shaderc (Apache 2.0), libiconv (LGPL), Zig (MIT), the Baloo 2 font (SIL OFL 1.1, Baloo2-OFL.txt), and the
GCC / MinGW-w64 runtime libraries (GPL with the runtime library exception).
