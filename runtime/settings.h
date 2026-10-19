// User settings (larecomp.ini in the data folder), set on the start screen (runtime/ui/launcher_lg.mm).
#pragma once
#include <stdbool.h>

enum { RES_MATCH = 0, /* 1..8 = 1x..8x */ RES_DYNAMIC = 9, RES_Q75 = 10, RES_Q25 = 11, RES_Q50 = 12 };
// (11 was 67% of the display: larecomp.ini "67%" now loads as RES_Q75)
enum { UPSCALE_BILINEAR = 0, UPSCALE_FSR1 = 2 /* AMD FSR 1 */ };
#define UPSCALE_SHARP UPSCALE_FSR1  // the non-bilinear upscaler (the default)
enum { AA_OFF = 0, AA_SMAA = 1 };
enum { VREAD_AUTO = 0, VREAD_COPY = 1, VREAD_DIRECT = 2 };
// the game's (each of its taps a hard in / out), each tap bilinear (2x2 PCF), each tap a 3x3 tent (4 bilinear lookups)
enum { SHADOW_GAME = 0, SHADOW_BILINEAR = 1, SHADOW_SMOOTH = 2 };

typedef struct {
    bool fullscreen;
    int fps;  // 30 (the game's own) or 60 (interpolated: runtime/nvn/vulkan/exec.c)
    int res_mode;        // RES_MATCH, 1..4 (N x the Switch's 1080p), RES_DYNAMIC, RES_Q75/50/25 (share of the display)
    int res_height;      // the launcher's render height in lines (720 / 1080 / 1440 / 2160); 0: res_mode decides
    int upscaler;        // UPSCALE_*: how a lower internal resolution reaches the window
    int antialias;       // AA_*: anti-aliasing of the game image before it is scaled to the window
    float volume;        // master volume, 1 = 100%
    float brightness;    // colour of the game image (F12 menu, runtime/ui/menu.mm; post pass in runtime/nvn/vulkan/exec.c):
    float contrast;      // 1 = unchanged for all three; brightness / contrast 0.5..1.5, saturation 0..2
    float saturation;
    bool ssao;            // screen-space ambient occlusion on the game's HDR scene (F12 menu; runtime/nvn/vulkan/exec.c)
    float ssao_intensity; // 0..3, 1 = the default strength
    float ssao_radius;    // in the game's world units (~cm): 10..400
    bool ssao_debug;      // (not saved) the scene shows the occlusion alone
    int shadow_filter;    // SHADOW_*: how the shadow map is filtered (F12 menu; textures.c vk_sampler, translate.c shpcf)
    float mouse_sens;    // mouse aim (the virtual gyro, runtime/hle/input.c): degrees per pixel
    float stick_deadzone;  // a controller's sticks: the share of their travel read as rest (runtime/hle/input.c)
    bool reverb;         // room reverb effect
    bool movie_audio;
    bool show_fps;
    bool dpad_move;      // the d-pad (and arrow keys) move the player like the stick
    bool render_thread;  // the backend runs on its own thread (runtime/nvn/vulkan/renderthread.c); default on
    int vertex_read;     // VREAD_*: where the GPU reads vertex and index data (runtime/nvn/vulkan/exec.c)
    bool wild_battle;    // touching a wild Pokémon starts a battle, then the capture (runtime/hle/wildbattle.c)
    char game_lang[8];   // the game's language code ("en-US", "es"...: runtime/mods.c game_language)
    char mod[128];       // active mod / fan translation (mods/<name>/), empty = none (runtime/mods.h)
} Settings;

extern Settings g_settings;

void settings_load(void);
void settings_save(void);
void settings_clamp_color(void);  // brightness / contrast / saturation / SSAO into their ranges
