"""Per-instruction replacements applied by gen.py (guest address -> function(gen, addr) returning C code).

Derived from LASRecomp (MIT, (c) 2026 Peiikai). The Link's Awakening patches were removed: their
addresses belong to that game's executable. Let's Go patches (widescreen etc.) go here later.
"""

PATCHES = {}
