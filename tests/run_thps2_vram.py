#!/usr/bin/env python3
"""Compile the actual engine texture cache and exercise both host configurations."""
from pathlib import Path
import subprocess
r = Path(__file__).resolve().parents[1]
b = r / '.build/thps2-vram-validation'
b.mkdir(parents=True, exist_ok=True)
core = r / 'vendor/flycast/core'
subprocess.run(['clang', '-O2', '-c', str(core/'deps/xxhash/xxhash.c'), '-o', str(b/'xxhash.o')], check=True)
for name, flags in [('fault-free', ['-DTARGET_NO_EXCEPTIONS']), ('desktop', [])]:
    cmd = ['clang++', '-std=c++11', '-O2', '-Wno-deprecated-declarations', '-DTARGET_NO_REC', '-DHOST_CPU=CPU_ARM64', '-DNO_VERIFY', *flags, '-I'+str(core), '-I'+str(core/'libretro-common/include'), str(r/'tests/test_thps2_vram.cpp'), str(core/'rend/TexCache.cpp'), str(b/'xxhash.o'), '-Wl,-dead_strip', '-o', str(b/name)]
    subprocess.run(cmd, check=True)
    subprocess.run([str(b/name)], check=True)
