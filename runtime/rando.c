// Randomizer hooks (LASRecomp's seeds for Link's Awakening). Let's Go has no randomizer: every hook is a no-op so
// the shared runtime (romfs lookups, generated code, menu) keeps its call sites.
#include "rando.h"

void rando_init(void) {}
const char *rando_seed_dir(void) { return NULL; }
uint32_t rando_word(uint64_t addr) { (void)addr; return 0; }
bool rando_romfs_override(const char *path, char *out, size_t cap) { (void)path; (void)out; (void)cap; return false; }
void rando_boot(void) {}
void rando_select(const char *seed) { (void)seed; }
