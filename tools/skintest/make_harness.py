"""make_harness.py <vertex shader .glsl> [out.cpp]: a C++ program that runs a translated vertex shader on the CPU.

The renderer's GLSL (LARECOMP_SAVE_MSL=1 -> build/glsl/<addr>_vs.glsl) is a scalar state machine (switch on pc) over
uint / float registers: with prelude.inc's GLSL look-alikes it compiles as C++. main.inc feeds it the inputs of one
skinned draw dumped by the renderer (LARECOMP_DUMP_SKIN=<n> -> build/skin/) and prints gl_Position next to a
reference linear-blend skinning, to tell a shader translation bug from bad inputs.

The generated .cpp contains the game's (translated) shader code: it stays local (build/, never committed).
"""
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
src = open(sys.argv[1], encoding='utf-8').read()
body = src[src.index('void main() {'):].replace('void main() {', 'static void shader_main() {', 1)
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join('build', 'skintest.cpp')
with open(out, 'w', encoding='utf-8', newline='\n') as f:
    f.write(open(os.path.join(here, 'prelude.inc'), encoding='utf-8').read())
    f.write(body)
    f.write(open(os.path.join(here, 'main.inc'), encoding='utf-8').read())
print(out)
