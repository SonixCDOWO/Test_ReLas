
static float h2f(uint16_t h) { uint s = (h & 0x8000u) << 16, e = (h >> 10) & 0x1F, m = h & 0x3FF;
  if (e == 0) return as_f(s) + (s ? -1.0f : 1.0f) * m * 5.9604644775390625e-8f;
  if (e == 31) return as_f(s | 0x7F800000u | (m << 13));
  return as_f(s | ((e + 112) << 23) | (m << 13)); }
static size_t load(const char *p, void *dst, size_t cap) { FILE *f = fopen(p, "rb"); if (!f) return 0; size_t n = fread(dst, 1, cap, f); fclose(f); return n; }
int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "build/skin";
  char path[512]; static uint8_t vb[1 << 20];
  snprintf(path, sizeof path, "%s/cb3.bin", dir); load(path, cb3, sizeof cb3);
  snprintf(path, sizeof path, "%s/cb4.bin", dir); size_t pal = load(path, cb4, sizeof cb4);
  snprintf(path, sizeof path, "%s/verts.bin", dir); size_t nb = load(path, vb, sizeof vb);
  const int stride = 56; int nv = (int)(nb / stride);
  printf("palette %zu bytes, %d vertices\n", pal, nv);
  for (int i = 0; i < nv; i++) {
    const uint8_t *p = vb + i * stride; float f[3]; memcpy(f, p, 12);
    a0 = vec4(f[0], f[1], f[2], 1);
    uint16_t h[4]; memcpy(h, p + 12, 8); a1 = vec4(h2f(h[0]), h2f(h[1]), h2f(h[2]), h2f(h[3]));
    float t[2]; memcpy(t, p + 28, 8); a3 = vec4(t[0], t[1], 0, 1); memcpy(t, p + 36, 8); a4 = vec4(t[0], t[1], 0, 1);
    a7 = {p[44], p[45], p[46], p[47]}; a11 = {p[48], p[49], p[50], p[51]};
    a12 = vec4(p[52] / 255.f, p[53] / 255.f, p[54] / 255.f, p[55] / 255.f);
    shader_main();
    // reference: linear blend skinning with the palette (column-major 4x4 per bone), weights from a12
    float ref[3] = {0, 0, 0};
    for (int k = 0; k < 4; k++) {
      float w = a12[k]; if (!w) continue; const float *m = (const float *)&cb4[a11[k] * 4];
      for (int r = 0; r < 3; r++) ref[r] += w * (m[r] * f[0] + m[4 + r] * f[1] + m[8 + r] * f[2] + m[12 + r]);
    }
    printf("v%-2d pos %8.3f %8.3f %8.3f bones %3u %3u w %.2f %.2f | skinned(ref) %8.3f %8.3f %8.3f | gl_Position %9.4f %9.4f %9.4f %9.4f\n",
           i, f[0], f[1], f[2], a11[0], a11[1], a12[0], a12[1], ref[0], ref[1], ref[2], gl_Position.x, gl_Position.y, gl_Position.z, gl_Position.w);
  }
}
