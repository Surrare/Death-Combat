#!/usr/bin/env python3
"""Regenerates the embedded fallback shader strings in src/gfx/renderer.cpp from the files in shaders/.
Run from the repository root after editing any shader:  python3 tools/embed_shaders.py"""
import re, sys
FILES = {'LIT_VS': 'lit.vert', 'LIT_FS': 'lit.frag', 'DEPTH_VS': 'depth.vert', 'DEPTH_FS': 'depth.frag',
         'UNLIT_VS': 'unlit.vert', 'UNLIT_FS': 'unlit.frag', 'UI_VS': 'ui.vert', 'UI_FS': 'ui.frag',
         'POST_VS': 'post.vert', 'BRIGHT_FS': 'bright.frag', 'BLUR_FS': 'blur.frag',
         'COMPOSITE_FS': 'composite.frag', 'MASK_FS': 'mask.frag', 'SSAO_FS': 'ssao.frag'}
path = 'src/gfx/renderer.cpp'
src = open(path, encoding='utf8').read()
start = src.index('// ---------------------------------------------------------------- shaders ----')
end = src.index('// ------------------------------------------------------------------- init ----')
out = src[start:src.index('static const char*', start)]
for name, f in FILES.items():
    txt = open('shaders/' + f, encoding='utf8').read().lstrip('﻿').rstrip()
    if ')GLSL"' in txt:
        sys.exit(f'{f} contains the raw-string terminator')
    out += f'static const char* {name} = R"GLSL({txt}\n)GLSL";\n\n'
open(path, 'w', encoding='utf8').write(src[:start] + out + src[end:])
print('embedded', len(FILES), 'shaders')
