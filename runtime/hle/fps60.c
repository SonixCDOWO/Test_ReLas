// 60 fps (experimental): the world and the menus at 60, battles and captures as the game made them (30).
//
// The game keeps a table of frame durations G (main+0xf7eeb8: 16.67 ms, 33.33 ms, 50 ms, ...; read through the pointer
// at main+0x15fb438). The world's update step comes from G[1] through the accessor f_7000039600; menus, battles and
// captures count their own frames, so at 60 fps they run twice as fast (docs/PESQUISA-60FPS.md). A community "60 FPS"
// patch changes G[1] for every reader; here:
//  - the animation systems take "elapsed time" as a fixed 33.33 ms per update and divide it by G[1] to get frames: the
//    time the accessor hands out is that elapsed time, so with 16.67 ms there they advance half a frame per update at
//    60 fps - right, by time (menus, the world's animations);
//  - battles and captures also count integer frames of their own: they keep the game's 33.33 ms and 30 fps (the entry
//    points of both scenes, mapped in docs/MODO-BATALHA.md, switch to it until the world's update runs again).
// Scenes move from the 30 fps side to the 60 fps side one at a time, once their frame counters are fixed.
//
// LARECOMP_FPS60=1 turns it on (settings: fps = 60). LARECOMP_G1_READERS="<addr>[,<addr>...]" (research): G[1] is
// 16.67 ms only while those functions run, to find which readers a speed comes from.
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../hle.h"
#include "../settings.h"

void rt_register_fnhook(uint64_t addr, int (*fn)(Cpu *c, uint64_t addr));  // runtime/fnhook.c
void rt_call_original(Cpu *c, uint64_t addr);
uint64_t renderer_present_count(void);

#define G1_ADDR 0x7000f7eec0ull  // G[1]: 33333334 ns
#define G1_ORIGINAL 33333334u
#define G1_60HZ 16666667u
#define FIELD_UPDATE 0x70002f8bb0ull  // the world's update, every frame while it runs
#define G_ACCESSOR 0x7000039600ull    // G[index]: the world's step comes through here

static bool g_on;
static bool g_world = true;             // 60 fps now (not inside a battle or a capture)
static bool g_special;                  // a battle or a capture started and the world hasn't updated since
#define ENCOUNTER_SCENE 0x700028ef70ull  // creates the capture scene (x0 = the encounter object)
#define BATTLE_START 0x700028bb60ull     // copies the battle's configuration

static int scene_hook(Cpu *c, uint64_t addr) {
    g_special = true;
    (void)c, (void)addr;
    return 0;
}

static int field_hook(Cpu *c, uint64_t addr) {
    g_special = false;
    (void)c, (void)addr;
    return 0;  // (the game's function runs as usual)
}

// G[1] at 60 Hz for the duration of f: the world's step; any other scene keeps the game's value.
static int accessor_hook(Cpu *c, uint64_t addr) {
    volatile uint32_t *g1 = (volatile uint32_t *)(uintptr_t)G1_ADDR;
    if (!g_world) return 0;
    *g1 = G1_60HZ;
    rt_call_original(c, addr);
    *g1 = G1_ORIGINAL;
    return 1;
}

static int g1_reader_hook(Cpu *c, uint64_t addr) {
    volatile uint32_t *g1 = (volatile uint32_t *)(uintptr_t)G1_ADDR;
    *g1 = G1_60HZ;
    rt_call_original(c, addr);
    *g1 = G1_ORIGINAL;
    return 1;
}

// Research (LARECOMP_FPS60_LOG=1): the frame-rate setting object the animation systems index G with (main+0x15fab08
// holds a pointer to a pointer to it; +0x88 is the index), once a second.
static void log_index(void) {
    static uint64_t last;
    uint64_t f = renderer_present_count();
    if (f - last < 60) return;
    last = f;
    const uint64_t *slot = (const uint64_t *)(uintptr_t)(0x7000000000ull + 0x15fab08);
    const uint64_t *obj = (const uint64_t *)(uintptr_t)*slot;
    if (!obj || (uint64_t)(uintptr_t)obj < 0x7000000000ull) return;
    const uint64_t *real = (const uint64_t *)(uintptr_t)*obj;
    if (!real || (uint64_t)(uintptr_t)real < 0x7000000000ull) return;
    LOG("[fps60] present %llu: frame-rate index %d (G[idx] = %u)\n", (unsigned long long)f, *(const int32_t *)((const uint8_t *)real + 0x88),
        *(const uint32_t *)(uintptr_t)(0x7000f7eeb8ull + 8ull * (uint32_t)*(const int32_t *)((const uint8_t *)real + 0x88)));
}

// Research (LARECOMP_ELAPSED_LOG=1): the elapsed time the animation functions f_7000663660 / f_7000664cd0 divide by
// G[index]: the object their parameter holds, asked as they do (its first virtual method, argument 1), field +8.
static int elapsed_hook(Cpu *c, uint64_t addr) {
    static int n;
    uint64_t saved[32], pc_arg = c->x[1];
    memcpy(saved, c->x, sizeof saved);
    uint64_t obj = pc_arg ? *(uint64_t *)(uintptr_t)pc_arg : 0;
    if (obj >= 0x7000000000ull && n < 40) {
        uint64_t vt = *(uint64_t *)(uintptr_t)obj, fn = vt >= 0x7000000000ull ? *(uint64_t *)(uintptr_t)vt : 0;
        if (fn >= 0x7000000000ull) {
            c->x[0] = obj;
            c->x[1] = 1;
            guest_call(c, fn);
            uint64_t r = c->x[0];
            if (r >= 0x7000000000ull)
                LOG("[elapsed] f_%llx present %llu: delta %lld ns\n", (unsigned long long)addr, (unsigned long long)renderer_present_count(),
                    (long long)*(const int64_t *)(uintptr_t)(r + 8));
            n++;
        }
    }
    memcpy(c->x, saved, sizeof saved);
    return 0;
}

bool fps60_enabled(void) { return g_on; }
bool fps60_world(void) { return g_world; }

// At each present (before its pacing): was this a world frame?
void fps60_present(void) {
    if (getenv("LARECOMP_FPS60_LOG")) log_index();
    if (!g_on) return;
    bool w = !g_special;
    if (w != g_world) LOG("[fps60] %s from present %llu\n", w ? "60 fps" : "battle / capture: 30 fps", (unsigned long long)renderer_present_count());
    g_world = w;
}

__attribute__((constructor)) static void fps60_init(void) {
    const char *e = getenv("LARECOMP_FPS60");
    g_on = e && atoi(e);
    if (g_on) {
        rt_register_fnhook(FIELD_UPDATE, field_hook);
        rt_register_fnhook(ENCOUNTER_SCENE, scene_hook);
        rt_register_fnhook(BATTLE_START, scene_hook);
        rt_register_fnhook(G_ACCESSOR, accessor_hook);
    }
    if (getenv("LARECOMP_ELAPSED_LOG")) {
        rt_register_fnhook(0x7000663660ull, elapsed_hook);
        rt_register_fnhook(0x7000664cd0ull, elapsed_hook);
    }
    e = getenv("LARECOMP_G1_READERS");
    char buf[512];
    if (!e) return;
    snprintf(buf, sizeof buf, "%s", e);
    for (char *t = strtok(buf, ","); t; t = strtok(NULL, ",")) rt_register_fnhook(strtoull(t, NULL, 16), g1_reader_hook);
}
