"""mv_errmap.py <dirA> <dirB> <frame> <out>: per-pixel reprojection error of frame N+1 from N with each dir's vectors
(dump N), side by side: A error | B error | B worse than A (red) / better (green) | B's velocity magnitude."""
import sys, os
import numpy as np
from PIL import Image

a_dir, b_dir, n, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]


def img(d, k):
    return np.asarray(Image.open(os.path.join(d, 'frame_%05d.png' % k)).convert('RGB'), dtype=np.float32)


def vec(d):
    enc = np.asarray(Image.open(os.path.join(d, 'mv_%05d.png' % n)).convert('RGB'), dtype=np.float32)
    v = (enc[..., :2] / 255.0 - 0.5) / 32.0
    v[..., 1] = -v[..., 1]
    return v


def bilinear(im, x, y):
    h, w = im.shape[:2]
    x = np.clip(x, 0, w - 1.001); y = np.clip(y, 0, h - 1.001)
    x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
    return (im[y0, x0] * (1 - fx) + im[y0, x0 + 1] * fx) * (1 - fy) + (im[y0 + 1, x0] * (1 - fx) + im[y0 + 1, x0 + 1] * fx) * fy


def err(d):
    cur, prev, v = img(d, n + 1), img(d, n), vec(d)
    h, w = v.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    warped = bilinear(prev, xx - v[..., 0] * w, yy - v[..., 1] * h)
    return np.abs(cur - warped).mean(axis=2), np.linalg.norm(v * [w, h], axis=2)


ea, _ = err(a_dir)
eb, mb = err(b_dir)
d = eb - ea
rgb = np.full(ea.shape + (3,), 255.0)
rgb[..., 1] -= np.clip(d * 4, 0, 255); rgb[..., 2] -= np.clip(d * 4, 0, 255)
rgb[..., 0] -= np.clip(-d * 4, 0, 255); rgb[..., 2] -= np.clip(-d * 4, 0, 255)
tiles = [np.clip(ea * 3, 0, 255), np.clip(eb * 3, 0, 255)]
tiles = [np.stack([t] * 3, -1) for t in tiles] + [rgb, np.stack([np.clip(mb * 8, 0, 255)] * 3, -1)]
h, w = ea.shape
sheet = np.full((h, 4 * w + 30, 3), 255.0)
for i, t in enumerate(tiles):
    sheet[:, i * (w + 10):i * (w + 10) + w] = t
Image.fromarray(sheet.astype(np.uint8)).resize((sheet.shape[1] // 2, h // 2)).save(out)
worse = d > 20
print('B worse by >20 levels: %.2f%% of pixels; better: %.2f%%' % (worse.mean() * 100, (d < -20).mean() * 100))
ys, xs = np.nonzero(worse)
if len(xs):
    print('worse region bbox: x %d-%d, y %d-%d' % (xs.min(), xs.max(), ys.min(), ys.max()))
