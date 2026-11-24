"""Offline: what the navmesh looks like inside a .umap on disk (name-table strings only, nothing is loaded).

    python umap_nav_scan.py <file.umap> [<file2.umap> ...]
"""
import os
import re
import sys

KEYS = (b'RecastNavMeshDataChunk', b'RecastNavMesh', b'NavigationDataChunk', b'NavMeshBoundsVolume', b'SKIRT_',
        b'ExponentialHeightFog', b'LandscapeComponent', b'HorizonSkirt')
for path in sys.argv[1:]:
    data = open(path, 'rb').read()
    print(path, os.path.getsize(path), 'bytes')
    for k in KEYS:
        print('   %-26s %d' % (k.decode(), len(re.findall(re.escape(k), data))))
    names = sorted(set(m.group(0).decode() for m in re.finditer(rb'RecastNavMesh[A-Za-z0-9_\-]*', data)))
    print('   names:', names[:20])
