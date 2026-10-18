#!/usr/bin/env python3
"""Aggregate harness/build_live live.txt (make LIVE=1) by call site."""
import collections, subprocess, sys
exe, path = sys.argv[1], sys.argv[2]
depth = int(sys.argv[3]) if len(sys.argv) > 3 else 2
rows = [l.split() for l in open(path)]
addrs = sorted({a for r in rows for a in r[1:1 + 6]})
out = subprocess.run(['addr2line', '-f', '-C', '-s', '-e', exe] + addrs, capture_output=True, text=True).stdout.split('\n')
name = {a: (out[2 * i].split('(')[0] + ':' + out[2 * i + 1].split(':')[-1].split(' ')[0]) for i, a in enumerate(addrs)}
skip = ('alloc', 'operator new', 'jj_calloc', 'jj_malloc', 'SDL_CreateRGBSurface', 'new_surface', 'traced')
agg = collections.Counter(); cnt = collections.Counter()
total = 0
for r in rows:
    size = int(r[0]); total += size
    frames = [name[a] for a in r[1:] if not any(s in name[a] for s in skip)]
    key = ' < '.join(frames[:depth])
    agg[key] += size; cnt[key] += 1
print('total live', total)
for k, v in agg.most_common(40):
    print('%8d %5d  %s' % (v, cnt[k], k))
