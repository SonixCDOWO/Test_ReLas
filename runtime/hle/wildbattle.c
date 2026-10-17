// Wild battles (experimental, docs/MODO-BATALHA.md): touching a wild Pokémon starts a battle (the game's "static
// encounter" process: Snorlax, the legendary birds, Mewtwo) instead of the Pokémon GO-style capture.
//
// How the game does a static battle (docs/MODO-BATALHA.md, stage 3c): a script command builds a process of class A
// (factory f_700017f0a0, constructor f_7000289ac0, object of 0xA8 bytes) whose [+0x98] holds the battle's 64-bit hash;
// its state machine loads bin/script_event_data/event_encount.bin (0x40-byte records), looks the hash up
// (f_70002bad00(table, &key) -> parsed record of 0x59 bytes), fills the battle configuration from the record
// (f_70002f60c0) and starts the battle. Here:
//  - the capture's entry point (f_700028ef70: x0 = the process manager, x1 = the allocator) builds a class A process
//    through that factory instead of the capture scene;
//  - the constructor hook then writes our own hash in [+0x98];
//  - the lookup hook answers that hash with a record of our own: Snorlax's real record as the mould (so everything the
//    game keeps in it stays valid) with the species, level and moves replaced.
// LARECOMP_WILDBATTLE=1 turns it on. Test settings (until the touched Pokémon is read): LARECOMP_WILDBATTLE_SPECIES=<n>
// (default 19), _LEVEL=<n> (3), _MOVES=<a,b,c,d> (33,45,0,0), _MAX=<n> (battles per run, default 1), _A6=1 (the "mode 3"
// flag of the process), _STATE_LOG=1 (log the process' state changes).
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hle.h"

void rt_register_fnhook(uint64_t addr, int (*fn)(Cpu *c, uint64_t addr));  // runtime/fnhook.c
void rt_call_original(Cpu *c, uint64_t addr);
uint64_t renderer_present_count(void);

#define ENCOUNTER_TRIGGER 0x7000305ad0ull  // a touched wild Pokémon (x0 = it, x1 -> the world's object): starts the capture
#define GOT_ALLOCATOR 0x70015faab8ull      // GOT slot -> pointer to the object holding the allocator at +0x68
#define CLASS_A_DTOR0 0x7000289f60ull
#define CLASS_A_DTOR1 0x7000289fd0ull
#define STATIC_FACTORY 0x700017f0a0ull  // (x0 manager, x1 allocator, x8 -> out handle): creates a class A process
#define STATIC_CTOR 0x7000289ac0ull     // class A constructor (x0 = the object)
#define TABLE_LOOKUP 0x70002bad00ull    // (x0 table, x1 -> u64 key) -> parsed record
#define SNORLAX_HASH 0x343e35139d576e27ull
#define WILD_HASH 0x57494c4442415454ull  // "WILDBATT": ours

static bool g_pending;  // the next class A constructor is ours
static int g_count, g_max = 1;
static int g_species = 19, g_level = 3, g_moves[4] = {33, 45, 0, 0};
static int g_a6;

#define R32(p, o) (*(volatile uint32_t *)((uintptr_t)(p) + (o)))
#define R8(p, o) (*(volatile uint8_t *)((uintptr_t)(p) + (o)))
#define R64(p, o) (*(volatile uint64_t *)((uintptr_t)(p) + (o)))

// The encounter trigger replaced whole: the game's own one starts the capture scene (f_700028ef70) and then keeps
// configuring the handle it gets back, which a battle process can't take. x1 -> [x1] -> [+0x68] is the process manager,
// [[*GOT 0x70015faab8]+0x68] the allocator (both as in the game's call to f_700028ef70).
static bool g_active;  // a static battle process of ours is alive: the touch that keeps calling the trigger is swallowed

// The touched Pokémon's own battle/capture configuration. f_7000305ad0 gets, in x0, the interface (+0x48) of the capture
// configuration object (0x2b8 bytes) that f_70003058f0 already built from the wild Pokémon (species, form, level, nature,
// IVs, shiny...); the battle's configuration is the same kind of object, filled by f_70002f60c0 from a table record. So
// instead of trusting a hand-made record, the trigger keeps a copy of the real configuration and the hook on f_70002f60c0
// lays it over the record-built one (keeping the word at +0x298, which that function sets to 1 for a static battle).
// LARECOMP_WILDBATTLE_USECFG=0 keeps the synthetic record (species / level from LARECOMP_WILDBATTLE_*).
#define CFG_SIZE 0x2b8
#define CONFIG_FROM_RECORD 0x70002f60c0ull  // (x0 = configuration, x1 = record)
static uint8_t g_cfg[CFG_SIZE] __attribute__((aligned(16)));
static bool g_cfg_valid, g_usecfg = true;
static uint8_t g_rec[0x60] __attribute__((aligned(16)));  // (the synthetic record, below)

static int config_hook(Cpu *c, uint64_t addr) {
    uint64_t cfg = c->x[0], rec = c->x[1];
    if (rec != (uint64_t)(uintptr_t)g_rec) return 0;  // (a real record: the game's own)
    uint64_t lr = c->x[30];
    rt_call_original(c, addr);  // the configuration from our synthetic record
    c->x[30] = lr;
    if (g_usecfg && g_cfg_valid) {
        uint8_t from_rec[CFG_SIZE];
        memcpy(from_rec, (const void *)(uintptr_t)cfg, CFG_SIZE);
        int n = 0;
        for (int o = 0; o < CFG_SIZE; o += 4) {
            if (memcmp(from_rec + o, g_cfg + o, 4) && n++ < 90)
                LOG("[wild]   cfg +%03x: record-built %08x, real %08x\n", o, *(const uint32_t *)(from_rec + o), *(const uint32_t *)(g_cfg + o));
        }
        LOG("[wild] real configuration laid over the record's (%d words differ)\n", n);
        uint32_t keep = *(const uint32_t *)(from_rec + 0x298);
        memcpy((void *)(uintptr_t)cfg, g_cfg, CFG_SIZE);
        *(uint32_t *)(uintptr_t)(cfg + 0x298) = keep;
    }
    return 1;
}

// Research: the game's own species / form accessors (f_70003058f0 reads them from [x22] to build the capture's
// configuration), tried on the objects the trigger gets, to find which one is the touched Pokémon's data.
static void probe_identity(Cpu *c) {
    uint64_t cand[3] = {c->x[0], c->x[0] - 0x48, R64(c->x[2], 0)};
    const char *name[3] = {"x0", "x0-0x48", "*x2"};
    uint64_t lr = c->x[30], sp = c->sp, save[32];
    memcpy(save, c->x, sizeof save);
    for (int i = 0; i < 3; i++) {
        c->x[0] = cand[i];
        rt_call_original(c, 0x70002185d0ull);
        uint64_t species = c->x[0] & 0xffff;
        c->x[0] = cand[i];
        rt_call_original(c, 0x70002186a0ull);
        uint64_t form = c->x[0] & 0xff;
        LOG("[wild]   probe %-8s %llx -> species %llu form %llu\n", name[i], (unsigned long long)cand[i], (unsigned long long)species,
            (unsigned long long)form);
    }
    memcpy(c->x, save, sizeof save);
    c->x[30] = lr;
    c->sp = sp;
}

static int trigger_hook(Cpu *c, uint64_t addr) {
    (void)addr;
    if (g_active) return 1;  // (swallowed: our battle is running)
    if (g_count >= g_max) return 0;  // (the game's capture runs)
    g_count++;
    if (getenv("LARECOMP_WILDBATTLE_PROBE")) probe_identity(c);
    if (g_usecfg) {
        memcpy(g_cfg, (const void *)(uintptr_t)(c->x[0] - 0x48), CFG_SIZE);
        g_cfg_valid = true;
    }
    uint64_t lr = c->x[30];
    uint64_t mgr = R64(R64(c->x[1], 0), 0x68);
    uint64_t alloc = R64(R64(R64(GOT_ALLOCATOR, 0), 0), 0x68);
    LOG("[wild] f%llu encounter intercepted (pokemon obj %llx, manager %llx, allocator %llx): building the static battle process\n",
        (unsigned long long)renderer_present_count(), (unsigned long long)c->x[0], (unsigned long long)mgr, (unsigned long long)alloc);
    uint64_t out[2] = {0, 0};
    g_pending = true;
    g_active = true;
    c->x[0] = mgr;
    c->x[1] = alloc;
    c->x[8] = (uint64_t)(uintptr_t)out;
    rt_call_original(c, STATIC_FACTORY);
    g_pending = false;
    LOG("[wild] factory returned, handle %llx\n", (unsigned long long)out[0]);
    c->x[30] = lr;
    c->x[0] = 0;
    return 1;
}

// Research (LARECOMP_WILDBATTLE_DEBUG=1): 240 presents after the battle's process is gone, dump the frame's render passes
// (build/pass_NNN.png) and every draw (the F9 request of the renderer): what a black screen after the battle is made of.
extern volatile int g_debug_frame_request;
static volatile long g_end_frame;  // the present the battle's process was destroyed in (fade-in timing)
static volatile long g_dbg_frame;  // the same, for the debug dump (which clears it)

#ifdef _WIN32
#include <windows.h>
static DWORD WINAPI debug_thread(LPVOID unused) {
    (void)unused;
    for (;;) {
        Sleep(50);
        if (g_dbg_frame && (long)renderer_present_count() >= g_dbg_frame + 240) {
            LOG("[wild] f%llu debug dump requested\n", (unsigned long long)renderer_present_count());
            g_debug_frame_request = 1;
            g_dbg_frame = 0;
        }
    }
    return 0;
}
#endif

// The game's screen fade (the script natives FadeIn_/FadeOut_, f_70001a7680 / f_70001a7860, end in these): the manager is
// [[*GOT 0x70015facf8]]. f_700011ece0(manager, 0, type, frames) fades in; f_700011ee50(manager, 0, type, ...) is the
// fade-out the object factories and FadeOut_ use. A script fades out before the battle and in after it; ours has no script,
// so (LARECOMP_WILDBATTLE_FADEIN=1) the last fade-out's type is repeated as a fade-in some presents after the battle.
#define FADE_IN_FN 0x700011ece0ull
#define FADE_OUT_FN 0x700011ee50ull
#define GOT_FADE 0x70015facf8ull
#define FIELD_UPDATE 0x70002f8bb0ull
static int g_fade_type = 1, g_fade_seen;
static bool g_fadein_on;
static int g_fadein_delay = 30, g_fadein_frames = 10;
static bool g_fadein_done = true;

static int fade_log_hook(Cpu *c, uint64_t addr) {
    LOG("[wild] f%llu fade %s: x1 %llx x2(type) %llx x3 %llx x4 %llx x5 %llx\n", (unsigned long long)renderer_present_count(),
        addr == FADE_IN_FN ? "in " : "out", (unsigned long long)c->x[1], (unsigned long long)c->x[2], (unsigned long long)c->x[3],
        (unsigned long long)c->x[4], (unsigned long long)c->x[5]);
    if (addr == FADE_OUT_FN) { g_fade_type = (int)c->x[2]; g_fade_seen = 1; }
    return 0;
}

static int field_hook(Cpu *c, uint64_t addr) {
    (void)addr;
    if (!g_fadein_done && g_end_frame && (long)renderer_present_count() >= g_end_frame + g_fadein_delay) {
        g_fadein_done = true;
        uint64_t save[32], lr = c->x[30], sp = c->sp;
        memcpy(save, c->x, sizeof save);
        c->x[0] = R64(R64(GOT_FADE, 0), 0);
        c->x[1] = 0;
        c->x[2] = (uint64_t)g_fade_type;
        c->x[3] = (uint64_t)g_fadein_frames;
        LOG("[wild] f%llu fading in by hand: manager %llx type %d frames %d (last fade-out %s)\n",
            (unsigned long long)renderer_present_count(), (unsigned long long)c->x[0], g_fade_type, g_fadein_frames, g_fade_seen ? "seen" : "NOT seen");
        rt_call_original(c, FADE_IN_FN);
        memcpy(c->x, save, sizeof save);
        c->x[30] = lr;
        c->sp = sp;
    }
    return 0;  // (the world's update runs)
}

static int dtor_hook(Cpu *c, uint64_t addr) {
    LOG("[wild] f%llu class A object %llx destroyed\n", (unsigned long long)renderer_present_count(), (unsigned long long)c->x[0]);
    g_active = false;
    g_end_frame = g_dbg_frame = (long)renderer_present_count();
    if (g_fadein_on) g_fadein_done = false;
    (void)addr;
    return 0;
}

static int ctor_hook(Cpu *c, uint64_t addr) {
    uint64_t lr = c->x[30], self = c->x[0];
    rt_call_original(c, addr);
    if (g_pending) {
        g_pending = false;
        R64(self, 0x98) = WILD_HASH;
        // as the script native CallWildBattle (f_700017e440) sets them after the factory: hash, a0 = a1 = 0, a2 = 1,
        // a3..a5 = !(flag bits 3, 1, 2) of the script's flags argument (0 here: all 1), a6 = 0 unless a special case
        // (the constructor's defaults are a0 = a1 = 1, a2 = 0 and would be wrong). LARECOMP_WILDBATTLE_RAWCTOR=1 keeps them.
        if (!getenv("LARECOMP_WILDBATTLE_RAWCTOR")) {
            R8(self, 0xa0) = 0;
            R8(self, 0xa1) = 0;
            R8(self, 0xa2) = 1;
            R8(self, 0xa3) = 1;
            R8(self, 0xa4) = 1;
            R8(self, 0xa5) = 1;
            R8(self, 0xa6) = 0;
        }
        if (g_a6) R8(self, 0xa6) = 1;
        LOG("[wild] class A object %llx: hash %llx, flags a0..a5 %02x%02x%02x%02x%02x%02x a6 %02x, state %u\n", (unsigned long long)self,
            (unsigned long long)R64(self, 0x98), R8(self, 0xa0), R8(self, 0xa1), R8(self, 0xa2), R8(self, 0xa3), R8(self, 0xa4),
            R8(self, 0xa5), R8(self, 0xa6), R32(self, 0x68));
    }
    c->x[30] = lr;
    return 1;
}

// the parsed record (f_70002bab70): +0 hash, +8 species, +0xc form, +0xd level, +0x10 shiny, +0x14 gender, +0x18 nature,
// +0x20/+0x28 text references, +0x30 ability, +0x34..+0x40 moves (u32), +0x44/+0x46 V1/V2, +0x48 IVs, +0x4e EVs,
// +0x54..+0x58 stat boosts
static void log_record(const char *what, const uint8_t *r) {
    uint32_t sp = *(const uint32_t *)(r + 8);
    LOG("[wild] %s: hash %llx species %u form %u level %u shiny %u gender %u nature %u ability %u moves %u,%u,%u,%u\n", what,
        (unsigned long long)*(const uint64_t *)r, sp, r[0xc], r[0xd], *(const uint32_t *)(r + 0x10), *(const uint32_t *)(r + 0x14),
        *(const uint32_t *)(r + 0x18), *(const uint32_t *)(r + 0x30), *(const uint32_t *)(r + 0x34), *(const uint32_t *)(r + 0x38),
        *(const uint32_t *)(r + 0x3c), *(const uint32_t *)(r + 0x40));
}

static int lookup_hook(Cpu *c, uint64_t addr) {
    uint64_t key = R64(c->x[1], 0);
    if (key != WILD_HASH) return 0;  // (a real lookup)
    uint64_t lr = c->x[30], sk = SNORLAX_HASH;
    c->x[1] = (uint64_t)(uintptr_t)&sk;
    rt_call_original(c, addr);  // the real Snorlax record: the mould
    uint64_t rec = c->x[0];
    c->x[30] = lr;
    memcpy(g_rec, (const void *)(uintptr_t)rec, sizeof g_rec);
    log_record("mould (Snorlax)", g_rec);
    *(uint64_t *)g_rec = WILD_HASH;
    *(uint32_t *)(g_rec + 8) = (uint32_t)g_species;
    g_rec[0xc] = 0;
    g_rec[0xd] = (uint8_t)g_level;
    for (int i = 0; i < 4; i++) *(uint32_t *)(g_rec + 0x34 + 4 * i) = (uint32_t)g_moves[i];
    memset(g_rec + 0x54, 0, 5);  // no stat boosts
    log_record("answering with", g_rec);
    c->x[0] = (uint64_t)(uintptr_t)g_rec;
    return 1;
}

// research: the process' state changes (the switch of f_7000289b40 reads [obj+0x68])
static int state_hook(Cpu *c, uint64_t addr) {
    static uint32_t last = 0xffffffff;
    uint32_t st = R32(c->x[0], 0x68);
    if (st != last) {
        last = st;
        LOG("[wild] f%llu process %llx state %u\n", (unsigned long long)renderer_present_count(), (unsigned long long)c->x[0], st);
    }
    (void)addr;
    return 0;
}

__attribute__((constructor)) static void wildbattle_init(void) {
    const char *e = getenv("LARECOMP_WILDBATTLE");
    if (!e || !*e || *e == '0') return;
    if (getenv("LARECOMP_WILDBATTLE_SPECIES")) g_species = atoi(getenv("LARECOMP_WILDBATTLE_SPECIES"));
    if (getenv("LARECOMP_WILDBATTLE_LEVEL")) g_level = atoi(getenv("LARECOMP_WILDBATTLE_LEVEL"));
    if (getenv("LARECOMP_WILDBATTLE_MAX")) g_max = atoi(getenv("LARECOMP_WILDBATTLE_MAX"));
    if (getenv("LARECOMP_WILDBATTLE_A6")) g_a6 = atoi(getenv("LARECOMP_WILDBATTLE_A6"));
    if (getenv("LARECOMP_WILDBATTLE_MOVES")) {
        char buf[64];
        snprintf(buf, sizeof buf, "%s", getenv("LARECOMP_WILDBATTLE_MOVES"));
        int n = 0;
        for (char *t = strtok(buf, ","); t && n < 4; t = strtok(NULL, ",")) g_moves[n++] = atoi(t);
        for (; n < 4; n++) g_moves[n] = 0;
    }
    rt_register_fnhook(ENCOUNTER_TRIGGER, trigger_hook);
    rt_register_fnhook(CLASS_A_DTOR0, dtor_hook);
    rt_register_fnhook(CLASS_A_DTOR1, dtor_hook);
    rt_register_fnhook(FADE_IN_FN, fade_log_hook);
    rt_register_fnhook(FADE_OUT_FN, fade_log_hook);
    if (getenv("LARECOMP_WILDBATTLE_FADEIN")) {
        g_fadein_on = atoi(getenv("LARECOMP_WILDBATTLE_FADEIN")) != 0;
        if (g_fadein_on) rt_register_fnhook(FIELD_UPDATE, field_hook);
        if (getenv("LARECOMP_WILDBATTLE_FADE_TYPE")) { g_fade_type = atoi(getenv("LARECOMP_WILDBATTLE_FADE_TYPE")); g_fade_seen = 1; }
        if (getenv("LARECOMP_WILDBATTLE_FADE_DELAY")) g_fadein_delay = atoi(getenv("LARECOMP_WILDBATTLE_FADE_DELAY"));
        if (getenv("LARECOMP_WILDBATTLE_FADE_FRAMES")) g_fadein_frames = atoi(getenv("LARECOMP_WILDBATTLE_FADE_FRAMES"));
    }
    rt_register_fnhook(STATIC_CTOR, ctor_hook);
    rt_register_fnhook(TABLE_LOOKUP, lookup_hook);
    if (getenv("LARECOMP_WILDBATTLE_USECFG")) g_usecfg = atoi(getenv("LARECOMP_WILDBATTLE_USECFG")) != 0;
    if (g_usecfg) rt_register_fnhook(CONFIG_FROM_RECORD, config_hook);
    if (getenv("LARECOMP_WILDBATTLE_STATE_LOG")) rt_register_fnhook(0x7000289b40ull, state_hook);
#ifdef _WIN32
    if (getenv("LARECOMP_WILDBATTLE_DEBUG")) CreateThread(NULL, 0, debug_thread, NULL, 0, NULL);
#endif
    LOG("[wild] on: species %d level %d moves %d,%d,%d,%d (max %d battle(s))\n", g_species, g_level, g_moves[0], g_moves[1],
        g_moves[2], g_moves[3], g_max);
}
