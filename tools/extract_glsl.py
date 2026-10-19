# (validation: the built-in shaders of runtime/nvn/vulkan/shaders.c into files for glslc)
import re, codecs, os, sys
# extract_glsl.py <shaders.c> <out dir> [shader names returned directly, e.g. mv.frag ...]
# Without names: the SSAO shaders (assembled from their parts).
src_path, out_dir, names = sys.argv[1], sys.argv[2], sys.argv[3:]
s = open(src_path, encoding='utf-8').read()
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')

def lits(text):
    return ''.join(codecs.decode(p, 'unicode_escape') for p in LIT.findall(text))

def decl(name):
    i = s.index(name)
    j = s.index(';\n', i)
    return lits(s[i + len(name):j])

def direct(name):
    i = s.index('if (!strcmp(name, "%s"))' % name)
    i = s.index('return', i)
    j = s.index(';\n', i)
    return lits(s[i:j])

if names == ['gi']:
    g = s.index('if (!strncmp(name, "gi_", 3))')

    def gdecl(name):
        i = s.index(name, g)
        j = s.index(';\n', i)
        return lits(s[i + len(name):j])
    common = gdecl('static const char *common =')
    for n in ('normal', 'trace', 'blur', 'temporal', 'composite'):
        open(os.path.join(out_dir, 'gi_' + n + '.frag'), 'w').write(common + gdecl('static const char *%s =' % n))
elif names:
    for n in names:
        open(os.path.join(out_dir, n), 'w').write(direct(n))
else:
    common = decl('static const char *common =')
    ao = decl('static const char *ao =')
    blur = decl('static const char *blur =')
    temporal = decl('static const char *temporal =')
    for n, t in [('ssao', common + ao), ('blur', common + blur), ('temporal', common + temporal),
                 ('apply', direct('ssao_apply.frag'))]:
        open(os.path.join(out_dir, n + '.frag'), 'w').write(t)
print('written')
