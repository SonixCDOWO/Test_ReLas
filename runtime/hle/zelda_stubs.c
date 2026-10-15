// Neutral versions of the Link's Awakening features removed from this fork (randomizer, QoL, warp, save editor,
// widescreen patches, overlays...). The shared runtime still calls them; here they do nothing, so the game runs
// as it does on the console. Remove each call site and its stub as that part of the runtime is cleaned up.
#include <stdbool.h>
#include <stdint.h>

#include "../cpu.h"

typedef struct { float x, y; } ImVec2C;  // (ImGui's ImVec2: two floats, passed by value)

// ---- state the input / launcher code shares with the removed features
int g_start_screen;  // the start screen is up: no game loaded
char g_window_shot_request[256];
int32_t g_stick_lx, g_stick_ly;
uint64_t g_stick_freeze_until;
int32_t g_test_lx, g_test_ly;
uint64_t g_test_stick_until, g_test_buttons, g_test_buttons_until;

int game_in_play(void) { return !g_start_screen; }
const char *game_level_name(void) { return ""; }

// ---- per-frame hooks (game main thread / presenter)
void chamber_game_tick(Cpu *c, uint64_t frame) { (void)c; (void)frame; }
void digs_game_tick(Cpu *c, uint64_t frame) { (void)c; (void)frame; }
void give_item_tick(Cpu *c, uint64_t frame) { (void)c; (void)frame; }
void pets_game_tick(Cpu *c) { (void)c; }
void warp_game_tick(Cpu *c) { (void)c; }
void qol_tick(void) {}
void warp_frame(uint64_t frame) { (void)frame; }
void save_editor_frame(uint64_t frame) { (void)frame; }

// ---- input
void input_note_rstick(int32_t ry) { (void)ry; }
uint64_t pets_menu_input(uint64_t b, int32_t st[4]) { (void)st; return b; }

// ---- save / romfs file hooks
void flags_ext_save_read(const char *path, uint64_t addr, int64_t off, int64_t size) { (void)path; (void)addr; (void)off; (void)size; }
void flags_ext_save_written(const char *path, uint64_t addr, int64_t off, int64_t size) { (void)path; (void)addr; (void)off; (void)size; }
void save_editor_file_read(const char *path, uint64_t addr, int64_t off, int64_t size) { (void)path; (void)addr; (void)off; (void)size; }
void romfs_file_read_hook(const char *path) { (void)path; }

// ---- renderer hooks: widescreen off (the game's own 16:9), no tints
bool widescreen_allowed(void) { return false; }
void widescreen_set_factor(float f) { (void)f; }
void widescreen_frame_end(uint64_t frame) { (void)frame; }
void widescreen_backdrop_seen(uint64_t hash) { (void)hash; }
void widescreen_scene_ubo(const float *u, uint64_t size) { (void)u; (void)size; }
float widescreen_effect_stretch(uint64_t fs_hash) { (void)fs_hash; return 1.0f; }
void widescreen_present_crop(uint64_t frame, double shape, float crop[4], double *vx, double *vy, double *vw, double *vh) {
    (void)frame; (void)shape; (void)vx; (void)vy; (void)vw; (void)vh;
    crop[0] = crop[1] = 0, crop[2] = crop[3] = 1;  // the whole image, viewport as given
}
bool pot_glow_active(void) { return false; }
bool pot_level_active(void) { return false; }
bool pot_instance_gold(const float *m) { (void)m; return false; }
bool pot_tint(const float *m, float tint[8], uint64_t mesh) { (void)m; (void)tint; (void)mesh; return false; }
void pot_gold_tint(float tint[8]) { (void)tint; }
void escape_bottle_tint(int kind, const float *ui, uint32_t ui_size, float tint[8]) { (void)kind; (void)ui; (void)ui_size; (void)tint; }
bool pets_hide_item_cursor(void) { return false; }

// ---- in-game menu tabs and overlays (runtime/ui/menu.mm)
void save_editor_draw(float scale) { (void)scale; }
void save_editor_on_open(void) {}
void rando_tab_draw(float scale) { (void)scale; }
bool warp_tab_draw(float scale) { (void)scale; return false; }
void cheats_draw(float scale) { (void)scale; }
void difficulty_draw(float scale) { (void)scale; }
bool inventory_overlay_wanted(void) { return false; }
bool tracker_overlay_wanted(void) { return false; }
bool chaos_overlay_wanted(void) { return false; }
void inventory_overlay_draw(ImVec2C display, void *font) { (void)display; (void)font; }
void tracker_overlay_draw(ImVec2C display, void *font) { (void)display; (void)font; }
void chaos_overlay_draw(ImVec2C display, void *font) { (void)display; (void)font; }
