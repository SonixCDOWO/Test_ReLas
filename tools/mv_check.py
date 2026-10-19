"""mv_check.py <dir> <frame>...: are the dumped motion vectors right? (runtime/nvn/vulkan/exec.c mv_run)

Capture: a scripted walk with LARECOMP_SHOT_AT=<N-1>,<N>,<N+1> and LARECOMP_MV_DUMP=<N> (the game images and the
vectors land in build/frames/), e.g. LARECOMP_INPUT="...,1490:STICK_RIGHT:200", then tools/mv_check.py build/frames N.
Measured (Pikachu, Pallet Town, 1280x720): walking right 14 px/frame, error 19.8 -> 1.9; walking down 8.6 px/frame,
error 14.3 -> 3.0 (the rest: the characters, whose own motion level 1 does not have).

For frame N: the previous game image (N-1) is warped by the vectors (each pixel fetches N-1 at uv - v) and compared with
N. Right vectors: the warped error is well below the error of N-1 as it is (where the camera moved). Also tries the
vectors with y negated (a wrong NDC y convention shows as the negated one winning) and frame offsets (a dump one
frame off its image).
"""
import sys, os
import numpy as np
from PIL import Image

d = sys.argv[1]
frames = [int(a) for a in sys.argv[2:]]

def img(n):
    p = os.path.join(d, 'frame_%05d.png' % n)
    return np.asarray(Image.open(p).convert('RGB'), dtype=np.float32) if os.path.exists(p) else None

def bilinear(im, x, y):
    h, w = im.shape[:2]
    x = np.clip(x, 0, w - 1.001); y = np.clip(y, 0, h - 1.001)
    x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
    a = im[y0, x0]; b = im[y0, x0 + 1]; c = im[y0 + 1, x0]; e = im[y0 + 1, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + e * fx) * fy

for n in frames:
    enc = np.asarray(Image.open(os.path.join(d, 'mv_%05d.png' % n)).convert('RGB'), dtype=np.float32)
    v = (enc[..., :2] / 255.0 - 0.5) / 32.0  # uv units: this frame's uv - the previous frame's
    # (the game's images are saved flipped - lower-left origin - and the vectors' PNG with them: in the PNG, y is down
    # where the vectors' y is up)
    v[..., 1] = -v[..., 1]
    h, w = v.shape[:2]
    px = np.abs(v * [w, h])
    print('frame %d: |v| mean %.2f px, max %.1f px, moving pixels %.0f%%' % (
        n, np.linalg.norm(v * [w, h], axis=2).mean(), px.max(), 100 * (np.linalg.norm(v * [w, h], axis=2) > 0.25).mean()))
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    for k in (0, -1, 1):
        cur, prev = img(n + k), img(n + k - 1)
        if cur is None or prev is None or cur.shape[:2] != (h, w):
            continue
        m = np.ones((h, w), bool)
        m[:20] = m[-20:] = False; m[:, :20] = m[:, -20:] = False  # (edges: pixels coming from outside)
        box = os.environ.get('MV_BOX')  # MV_BOX=x0,y0,x1,y1: only that region (e.g. the player, whom the camera follows)
        if box:
            x0, y0, x1, y1 = (int(t) for t in box.split(','))
            mb = np.zeros((h, w), bool)
            mb[y0:y1, x0:x1] = True
            m &= mb
        base = np.abs(cur - prev).mean(axis=2)[m].mean()
        out = []
        for sy in (1, -1):
            px_ = xx + 0.5 - v[..., 0] * w - 0.5
            py_ = yy + 0.5 - sy * v[..., 1] * h - 0.5
            warped = bilinear(prev, px_, py_)
            out.append(np.abs(cur - warped).mean(axis=2)[m].mean())
        print('   image N%+d vs N%+d: error unwarped %.2f, warped %.2f (y negated: %.2f)' % (k, k - 1, base, out[0], out[1]))
