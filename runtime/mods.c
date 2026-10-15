// Mods: LayeredFS-style replacement files and the game's language (runtime/mods.h). The RomFS reader
// (runtime/services/fs.c) asks mod_romfs_override for every file of the image and serves the mod's copy instead, and
// adds the files a mod has that the game hasn't.
#include "mods.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "hle.h"
#include "settings.h"

static bool is_dir(const char *p) {
    struct stat sb;
    return !stat(p, &sb) && S_ISDIR(sb.st_mode);
}

// The romfs/ folder inside dir, up to `depth` levels down (a download unpacked with its own folders around it). Packs
// made for both games (atmosphere/titles/<Pikachu>/ and <Eevee>/) are searched under Let's Go Eevee's title id first.
#define EEVEE_TITLE "0100187003A36000"
static bool find_romfs(const char *dir, int depth, char *out, size_t cap) {
    char p[1024];
    snprintf(p, sizeof p, "%s/romfs", dir);  // (RomFs / romfs: Windows names ignore case)
    if (is_dir(p)) { snprintf(out, cap, "%s", p); return true; }
    if (depth <= 0) return false;
    snprintf(p, sizeof p, "%s/" EEVEE_TITLE, dir);
    if (is_dir(p) && find_romfs(p, depth - 1, out, cap)) return true;
    DIR *d = opendir(dir);
    if (!d) return false;
    bool found = false;
    for (struct dirent *e; !found && (e = readdir(d));) {
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        if (is_dir(p)) found = find_romfs(p, depth - 1, out, cap);
    }
    closedir(d);
    return found;
}

int mods_list(char names[][128], int max) {
    DIR *d = opendir("mods");
    if (!d) return 0;
    int n = 0;
    char p[1024], r[1024];
    for (struct dirent *e; n < max && (e = readdir(d));) {
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "mods/%s", e->d_name);
        if (is_dir(p) && find_romfs(p, 4, r, sizeof r)) snprintf(names[n++], 128, "%s", e->d_name);
    }
    closedir(d);
    return n;
}

static const char *romfs_of(const char *name) {
    static char cached_name[128], dir[1024];
    static bool ok;
    if (!name || !*name) return NULL;
    if (strcmp(name, cached_name)) {
        snprintf(cached_name, sizeof cached_name, "%s", name);
        char p[1024];
        snprintf(p, sizeof p, "mods/%s", name);
        ok = find_romfs(p, 4, dir, sizeof dir);
        if (ok) LOG("[mods] %s: %s\n", name, dir);
        else LOG("[mods] %s: no romfs/ folder found under mods/%s\n", name, name);
    }
    return ok ? dir : NULL;
}

const char *mod_romfs_dir(void) { return romfs_of(g_settings.mod); }

bool mod_romfs_override(const char *path, char *out, size_t cap) {
    const char *root = mod_romfs_dir();
    if (!root) return false;
    struct stat sb;
    snprintf(out, cap, "%s/%s", root, path);
    return !stat(out, &sb) && S_ISREG(sb.st_mode);
}

// bin/message/<folder> -> language code
static const struct { const char *folder, *code; } k_msg_langs[] = {
    {"English", "en-US"}, {"Spanish", "es"},  {"French", "fr"},          {"German", "de"},         {"Italian", "it"},
    {"JPN", "ja"},        {"Korean", "ko"},   {"Simp_Chinese", "zh-CN"}, {"Trad_Chinese", "zh-TW"},
};

const char *mod_language(const char *name) {
    const char *root = romfs_of(name);
    if (!root) return NULL;
    const char *found = NULL;
    char p[1100];
    for (size_t i = 0; i < sizeof k_msg_langs / sizeof k_msg_langs[0]; i++) {
        snprintf(p, sizeof p, "%s/bin/message/%s", root, k_msg_langs[i].folder);
        if (!is_dir(p)) continue;
        if (found) return NULL;  // several languages: the player's choice stands
        found = k_msg_langs[i].code;
    }
    return found;
}

const char *game_language(void) {
    const char *m = mod_language(g_settings.mod);
    if (m) return m;
    return g_settings.game_lang[0] ? g_settings.game_lang : "en-US";
}
