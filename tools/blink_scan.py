"""blink_scan.py <dir> <first> <last>: frames that blink - content in frame n found in neither n-1 nor n+1 within 4 px
(something that appears for one frame) or missing from n but present in both neighbours (something that vanishes for
one frame). Frames are frame_NNNNN.png (LARECOMP_SCREENSHOT_EVERY=1), compared at 480x270."""
import os
import subprocess
import sys

import numpy as np

W, H = 480, 270
ff = os.environ.get('FFMPEG', 'ffmpeg')


def load(path):
    raw = subprocess.run([ff, '-v', 'error', '-i', path, '-vf', f'scale={W}:{H}', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(H, W, 3).astype(np.float32)


def nearest(m, srcs):
    nov = np.full(m.shape[:2], 1e9, np.float32)
    for src in srcs:
        for dy in range(-4, 5, 2):
            for dx in range(-4, 5, 2):
                nov = np.minimum(nov, np.abs(m - np.roll(np.roll(src, dy, 0), dx, 1)).mean(axis=2))
    return nov


def load_series(d, first, last):
    """A run of consecutive frames through one ffmpeg (one process per frame took minutes for a 900-frame route)."""
    count = 0
    while first + count <= last and os.path.exists(f'{d}/frame_{first + count:05d}.png'):
        count += 1
    if not count:
        return {}
    raw = subprocess.run([ff, '-v', 'error', '-start_number', str(first), '-i', f'{d}/frame_%05d.png', '-frames:v', str(count),
                          '-vf', f'scale={W}:{H}', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True, check=True).stdout
    arr = np.frombuffer(raw, np.uint8).reshape(-1, H, W, 3).astype(np.float32)
    return {first + i: arr[i] for i in range(len(arr))}


d, first, last = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
frames = {}
n = first
while n <= last:  # (consecutive runs; a gap is skipped)
    run = load_series(d, n, last)
    frames.update(run)
    n += max(len(run), 1)
R = 4  # (shifts of up to 4 px, in steps of 2, as nearest() above)


def novelty(m, n):
    """Per pixel of frame m: the smallest difference to frame n shifted by up to R px (nearest(), without copies)."""
    best = None
    src = np.pad(frames[n], ((R, R), (R, R), (0, 0)), mode='edge')
    for dy in range(-R, R + 1, 2):
        for dx in range(-R, R + 1, 2):
            d = np.abs(frames[m] - src[R + dy:R + dy + H, R + dx:R + dx + W]).mean(axis=2)
            best = d if best is None else np.minimum(best, d)
    return best


pair = {}  # (m, n) -> novelty(m, n): each neighbour pair once in each direction
def nov(m, n):
    if (m, n) not in pair:
        pair[(m, n)] = novelty(m, n) > 24
    return pair[(m, n)]


for n in range(first + 1, last):
    if n - 1 not in frames or n not in frames or n + 1 not in frames:
        continue
    # A blink is local in time: the neighbours agree with each other where frame n differs from both. Without that,
    # a camera moving faster than the 4 px tolerance (walking) reads as "appears" on every frame.
    steady = ~nov(n - 1, n + 1) & ~nov(n + 1, n - 1)
    appear = float((nov(n, n - 1) & nov(n, n + 1) & steady).mean()) * 100   # in n, in neither neighbour
    vanish = float((nov(n - 1, n) & nov(n + 1, n) & steady).mean()) * 100   # in both neighbours, not in n
    for k in [k for k in pair if max(k) < n - 1]:
        del pair[k]
    if appear > 0.15 or vanish > 0.15:
        print(f'{n}: appears {appear:.2f}%  vanishes {vanish:.2f}%')
