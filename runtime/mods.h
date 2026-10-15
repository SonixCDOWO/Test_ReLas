// Mods: game files replaced or added from a folder, as Atmosphere's LayeredFS (fan translations come this way), and
// the game's language. runtime/mods.c; the start screen (runtime/ui/launcher_lg.mm) picks both.
//
//   mods/<name>/romfs/...                                  the game's RomFS paths (bin/message/English/...)
//   mods/<name>/atmosphere/contents/<title id>/romfs/...   (or any folder holding a romfs/ up to 4 levels down:
//                                                           a download unpacked as it came)
#pragma once
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODS_MAX 32
// The folders under mods/ that hold a romfs/: names[i] (up to MODS_MAX). Returns how many.
int mods_list(char names[][128], int max);
// The active mod's romfs/ folder (g_settings.mod), or NULL.
const char *mod_romfs_dir(void);
// path: a RomFS path ("bin/message/English/..."). True with the host file when the active mod replaces it.
bool mod_romfs_override(const char *path, char *out, size_t cap);
// The language whose texts a mod replaces (its romfs/bin/message/<language>/), as a language code ("en-US"), or NULL
// (none, or several).
const char *mod_language(const char *name);
// The language the game is told (nn::settings / applet "desired language"): the active mod's, else g_settings.game_lang.
const char *game_language(void);

#ifdef __cplusplus
}
#endif
