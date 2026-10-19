"""ssao_temporal_check.py <off dir> <on dir>: temporal SSAO against single-frame SSAO, from LARECOMP_SSAO_DUMP images (ao_N.png)
of two runs with the same scripted input (walking right 1490-1550, then still).

- ghost: pixels where the temporal occlusion is darker than the single-frame one by more than 2% (5 levels) - old
  occlusion left behind where it no longer is (the single-frame image has no history); after the stop it must go to 0
- noise: mean |occlusion - its 5x5 box blur| (high-frequency energy: the sampling noise accumulation should remove)
"""
import sys, os
import numpy as np
from PIL import Image

off, on = sys.argv[1:3]

def load(d, n):
    return np.asarray(Image.open(os.path.join(d, 'ao_%05d.png' % n)).convert('L'), dtype=np.float32)

def box5(a):
    p = np.pad(a, 2, mode='edge')
    c = np.cumsum(np.cumsum(p, 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    return (c[5:, 5:] - c[:-5, 5:] - c[5:, :-5] + c[:-5, :-5]) / 25.0

print('frame   ghost>2%   lighter>2%   noise single   noise temporal')
for n in list(range(1495, 1560, 5)) + [1561, 1565, 1570, 1575, 1580]:
    a, b = load(off, n), load(on, n)
    d = b - a
    na = np.abs(a - box5(a)).mean()
    nb = np.abs(b - box5(b)).mean()
    tag = 'walking' if n < 1552 else 'still'
    print('%d   %6.2f%%    %6.2f%%      %6.3f         %6.3f   %s' % (n, (d < -5).mean() * 100, (d > 5).mean() * 100, na, nb, tag))
