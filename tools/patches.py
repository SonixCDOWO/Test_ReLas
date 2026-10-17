"""Per-instruction replacements applied by gen.py (guest address -> function(gen, addr) returning C code).

Derived from LASRecomp (MIT, (c) 2026 Peiikai). The Link's Awakening patches were removed: their
addresses belong to that game's executable. Let's Go patches (widescreen etc.) go here later.
"""

PATCHES = {}


# 60 fps: the frame counters the capture and battle scenes' timelines run on (found with the store tracer, docs/PESQUISA-60FPS.md).
# Each steps once per update; at 60 fps they step on every other one (rt_scene_tick, runtime/hle/fps60.c) so those scenes
# keep the game's 30 steps a second. Everywhere else rt_scene_tick() is 1 and the store is the game's own.
def _scene_counter(addr_expr, reg):
    def patch(gen, a):
        return ('{ extern int rt_scene_tick(void); uint64_t ea = %s; if (rt_scene_tick()) ST32(ea, (uint32_t)c->x[%d]); }' % (addr_expr, reg))
    return patch


PATCHES[0x7000b88340] = _scene_counter('c->x[19] + 0x58ull', 8)   # str w8, [x19, #0x58]  (the sequence object's step, f_7000b88310)
PATCHES[0x7000b6acc8] = _scene_counter('c->x[9] + c->x[11]', 12)  # str w12, [x9, x11]  (the per-track counters of a timeline)


# The capture ring's counter (+0x78 of the object in x19): counts up while two fields match, back to 0 otherwise. The
# result is in w8 (computed by the csinc before); on the update that doesn't tick, keep the old value (w9).
def _ring_counter(gen, a):
    return ('{ extern int rt_scene_tick(void); uint64_t ea = c->x[19] + 0x78ull; '
            'if (!rt_scene_tick() && (uint32_t)c->x[8] == (uint32_t)c->x[9] + 1) c->x[8] = (uint32_t)c->x[9]; '
            'ST32(ea, (uint32_t)c->x[8]); }')


PATCHES[0x70002d2a44] = _ring_counter
