// Guest function hooks. Every generated function starts with FN_ENTER (runtime/cpu.h); while g_fnhook is set it
// calls rt_fnhook first, and a hook that returns 1 has handled the call - the guest function returns at once (the
// hook sets x0 etc. as the function would). rt_call_original runs the game's own function from inside a hook
// (wrapping). Cost when no hook is registered: one predictable branch per call.
//
// Investigation (docs/MODO-BATALHA.md): LARECOMP_HOOK_DUMP="<addr>:<arg>,<arg>...[;<addr>:...]" logs, the first
// LARECOMP_HOOK_DUMP_MAX (default 4) times each function is entered, the registers x0..x7 and the memory the args
// name: "x2:30" = 0x30 bytes at x2, "*x0:400" = 0x400 bytes at the pointer stored at x0. LARECOMP_HOOK_DUMP_FRAMES=
// <first>-<last> limits it to those presents (the count then starts there).
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gamelib.h"
#include "hle.h"

volatile int g_fnhook;

typedef int (*FnHook)(Cpu *c, uint64_t addr);
#define NHOOK 1024
static struct { uint64_t addr; FnHook fn; } hooks[NHOOK];
static __thread uint64_t bypass;  // rt_call_original: the next entry of this function is the original running

void rt_register_fnhook(uint64_t addr, FnHook fn) {
    uint32_t s = (uint32_t)((addr >> 2) * 0x9E3779B1u) & (NHOOK - 1);
    while (hooks[s].addr && hooks[s].addr != addr) s = (s + 1) & (NHOOK - 1);
    hooks[s].addr = addr;
    hooks[s].fn = fn;
    g_fnhook = 1;
}

int rt_fnhook(Cpu *c, uint64_t addr) {
    if (bypass == addr) { bypass = 0; return 0; }
    uint32_t s = (uint32_t)((addr >> 2) * 0x9E3779B1u) & (NHOOK - 1);
    while (hooks[s].addr) {
        if (hooks[s].addr == addr) return hooks[s].fn(c, addr);
        s = (s + 1) & (NHOOK - 1);
    }
    return 0;
}

// The game's function at addr (gamelib tables), run as if called: x0.. as set by the caller of the hook.
void rt_call_original(Cpu *c, uint64_t addr) {
    const GameLibTables *t = gamelib_tables();
    for (uint32_t m = 0; t && m < t->nmods; m++) {
        const GameLibModule *md = &t->mods[m];
        uint64_t lo = 0, hi = *md->count;
        while (lo < hi) {
            uint64_t mid = (lo + hi) / 2;
            if (md->addrs[mid] < addr) lo = mid + 1;
            else hi = mid;
        }
        if (lo < *md->count && md->addrs[lo] == addr) {
            bypass = addr;
            md->fns[lo](c);
            return;
        }
    }
    LOG("[fnhook] no game function at %llx\n", (unsigned long long)addr);
}

// ---- LARECOMP_HOOK_DUMP
typedef struct { uint64_t addr; int nargs; struct { int reg, deref; uint32_t len; } arg[8]; _Atomic int hits; } Dump;
static Dump dumps[32];
static int ndumps, dump_max = 4;

static bool guest_ptr(uint64_t p) { return p >= 0x7000000000ull && p < 0x7800000000ull; }

static void hexdump(const char *what, uint64_t p, uint32_t len) {
    LOG("[hook]   %s @%llx (%u bytes):\n", what, (unsigned long long)p, len);
    if (!guest_ptr(p)) { LOG("[hook]     (not guest memory)\n"); return; }
    const uint8_t *b = (const uint8_t *)(uintptr_t)p;
    for (uint32_t o = 0; o < len; o += 16) {
        char line[128];
        int n = snprintf(line, sizeof line, "[hook]     +%03x:", o);
        for (uint32_t k = 0; k < 16 && o + k < len; k += 4) n += snprintf(line + n, sizeof line - n, " %08x", *(const uint32_t *)(b + o + k));
        LOG("%s\n", line);
    }
}

static long dump_from = -1, dump_to = -1;  // LARECOMP_HOOK_DUMP_FRAMES=<first>-<last>: only during those presents

static int dump_hook(Cpu *c, uint64_t addr) {
    extern uint64_t renderer_present_count(void);
    uint64_t frame = renderer_present_count();
    if (dump_from >= 0 && ((long)frame < dump_from || (long)frame > dump_to)) return 0;
    for (int i = 0; i < ndumps; i++) {
        Dump *d = &dumps[i];
        if (d->addr != addr || atomic_fetch_add(&d->hits, 1) >= dump_max) continue;
        LOG("[hook] f%llu %llx entered (#%d) from lr %llx: x0 %llx x1 %llx x2 %llx x3 %llx x4 %llx x5 %llx x6 %llx x7 %llx\n",
            (unsigned long long)frame, (unsigned long long)addr, d->hits, (unsigned long long)c->x[30], (unsigned long long)c->x[0], (unsigned long long)c->x[1],
            (unsigned long long)c->x[2], (unsigned long long)c->x[3], (unsigned long long)c->x[4], (unsigned long long)c->x[5],
            (unsigned long long)c->x[6], (unsigned long long)c->x[7]);
        for (int k = 0; k < d->nargs; k++) {
            uint64_t p = c->x[d->arg[k].reg];
            char what[16];
            snprintf(what, sizeof what, "%sx%d", d->arg[k].deref ? "*" : "", d->arg[k].reg);
            if (d->arg[k].deref) p = guest_ptr(p) ? *(const uint64_t *)(uintptr_t)p : 0;
            hexdump(what, p, d->arg[k].len);
        }
    }
    return 0;  // (the game's function runs as usual)
}

__attribute__((constructor)) static void hook_dump_init(void) {
    const char *e = getenv("LARECOMP_HOOK_DUMP");
    if (!e) return;
    if (getenv("LARECOMP_HOOK_DUMP_MAX")) dump_max = atoi(getenv("LARECOMP_HOOK_DUMP_MAX"));
    if (getenv("LARECOMP_HOOK_DUMP_FRAMES")) {
        const char *f = getenv("LARECOMP_HOOK_DUMP_FRAMES");
        dump_from = atol(f);
        dump_to = strchr(f, '-') ? atol(strchr(f, '-') + 1) : dump_from;
    }
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", e);
    for (char *fn = strtok(buf, ";"); fn && ndumps < 32; fn = strtok(NULL, ";")) {
        Dump *d = &dumps[ndumps];
        char *colon = strchr(fn, ':');
        d->addr = strtoull(fn, NULL, 16);
        for (char *a = colon ? colon + 1 : NULL; a && *a && d->nargs < 8;) {
            int deref = *a == '*';
            if (deref) a++;
            if (*a != 'x') break;
            int reg = (int)strtol(a + 1, &a, 10);
            uint32_t len = *a == ':' ? (uint32_t)strtoul(a + 1, &a, 16) : 0x40;
            d->arg[d->nargs].reg = reg;
            d->arg[d->nargs].deref = deref;
            d->arg[d->nargs].len = len;
            d->nargs++;
            while (*a == ',') a++;
        }
        ndumps++;
    }
    for (int i = 0; i < ndumps; i++) rt_register_fnhook(dumps[i].addr, dump_hook);
}
