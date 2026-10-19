"""ssao_temporal_sheet.py <off dir> <on dir> <out png> <frame>...: enhanced occlusion crops (single | temporal | difference)
around the player, contrast stretched (occlusion x4) so trails show."""
import sys, os
import numpy as np
from PIL import Image, ImageDraw

off, on, out = sys.argv[1:4]
frames = [int(a) for a in sys.argv[4:]]
crop = (390, 230, 890, 560)

def ao(d, n):
    a = np.asarray(Image.open(os.path.join(d, 'ao_%05d.png' % n)).convert('L'), dtype=np.float32) / 255.0
    return a[crop[1]:crop[3], crop[0]:crop[2]]

def enh(a):  # occlusion 1 - a, stretched x4, as darkness
    return Image.fromarray(np.clip(255 * (1 - 4 * (1 - a)), 0, 255).astype(np.uint8)).convert('RGB')

def diffimg(a, b):  # temporal darker: red, lighter: green, x8
    d = (b - a) * 8
    rgb = np.full(a.shape + (3,), 255, np.float32)
    rgb[..., 1] -= np.clip(-d, 0, 1) * 255
    rgb[..., 2] -= np.clip(-d, 0, 1) * 255
    rgb[..., 0] -= np.clip(d, 0, 1) * 255
    rgb[..., 2] -= np.clip(d, 0, 1) * 255
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))

w, h = crop[2] - crop[0], crop[3] - crop[1]
sheet = Image.new('RGB', (3 * w + 20, len(frames) * (h + 18)), 'white')
dr = ImageDraw.Draw(sheet)
for i, n in enumerate(frames):
    a, b = ao(off, n), ao(on, n)
    y = i * (h + 18)
    dr.text((4, y + 2), 'frame %d: single | temporal | temporal - single (red: darker, green: lighter, x8)' % n, fill='black')
    sheet.paste(enh(a), (0, y + 16))
    sheet.paste(enh(b), (w + 10, y + 16))
    sheet.paste(diffimg(a, b), (2 * w + 20, y + 16))
sheet.save(out)
print(out)
