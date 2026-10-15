# Contributing

Bug reports, test results from other PCs and code are all welcome. Issues and pull requests can be written in
English or Portuguese.

## The rule that never changes: nothing from Nintendo

Never post any of these in the repository, issues or pull requests:

- copies of the game in any format (`.xci`, `.nsp`, `.nca`...), keys (`prod.keys`, `title.keys`)
- extracted files (`extracted/`), generated code (`generated/`), the Switch's system fonts (`.bfttf`)
- any game asset: textures, models, music, text, romfs files, not even "just one file to test"
- saves (`save/`) and mods containing game files (`mods/`)

Screenshots and short videos of the game in bug reports are welcome.

## Reporting problems

Open an issue with:

- your PC: CPU, GPU, driver version, and the window size or resolution you chose
- what you did, what happened, and what you expected
- a screenshot or a short video, if it's visual
- if the game closed or froze: the `lasrecomp.log` file next to `LetsGoRecomp.exe`
- if you were using a translation or mod, which one

## Code

- Build and test as in [docs/COMPILAR-WINDOWS.md](docs/COMPILAR-WINDOWS.md).
- Every fix gets a line in [docs/HISTORICO.md](docs/HISTORICO.md) (symptom, diagnosis, cause, fix), and
  [docs/PROGRESSO.md](docs/PROGRESSO.md) tracks the current state.
- New debugging variables and tools go in [docs/DEPURACAO.md](docs/DEPURACAO.md).
- The runtime came from LAS Recomp, made for Zelda. Before blaming the game, check whether one of the runtime's
  assumptions holds for Let's Go (formats, controller styles, return codes, reported resolution).
- To change the game's behavior without rebuilding the game code library, use the function hooks
  (`runtime/fnhook.c`).
