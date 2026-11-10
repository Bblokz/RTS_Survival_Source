"""Compare a mesh's collision with its visible surface (system Python + numpy).

Inputs (written by the editor side):
  <tris.bin>   int16 slot + 9 float32 per triangle (mesh space), from a mesh description dump.
  <coll.json>  {"rays": [[sx,sy,sz,dx,dy,dz,len], ...], "meshes": {name: [hit distance or null per ray]}}
Prints, per mesh and per side, how far the collision surface sits in front of the visible surface along each ray.
The wall-hole component centres holes on the collision hit, so gaps beyond ~30 cm mean holes land in air.

Usage: python collision_gap.py <tris.bin> <coll.json> [concrete_slot]
"""
import json
import struct
import sys

import numpy as np


def load_triangles(path):
    raw = open(path, 'rb').read()
    record = struct.calcsize('<h9f')
    count = len(raw) // record
    slots = np.zeros(count, dtype=np.int16)
    tris = np.zeros((count, 3, 3), dtype=np.float64)
    for index in range(count):
        values = struct.unpack_from('<h9f', raw, index * record)
        slots[index] = values[0]
        tris[index] = np.array(values[1:]).reshape(3, 3)
    return tris, slots


def first_hits(tris, origins, directions):
    """Möller–Trumbore against every triangle; returns (distance, triangle index) per ray, inf / -1 when missed."""
    v0, v1, v2 = tris[:, 0], tris[:, 1], tris[:, 2]
    e1, e2 = v1 - v0, v2 - v0
    distances = np.full(len(origins), np.inf)
    indices = np.full(len(origins), -1)
    for ray, (origin, direction) in enumerate(zip(origins, directions)):
        p = np.cross(direction, e2)
        det = np.einsum('ij,ij->i', e1, p)
        valid = np.abs(det) > 1e-9
        inv = np.where(valid, 1.0 / np.where(valid, det, 1.0), 0.0)
        s = origin - v0
        u = np.einsum('ij,ij->i', s, p) * inv
        q = np.cross(s, e1)
        v = (q @ direction) * inv
        t = np.einsum('ij,ij->i', e2, q) * inv
        hit = valid & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0)
        if hit.any():
            best = np.argmin(np.where(hit, t, np.inf))
            distances[ray], indices[ray] = t[best], best
    return distances, indices


def side_name(direction):
    axis = int(np.argmax(np.abs(direction)))
    return ('-X', '+X', '-Y', '+Y')[axis * 2 + (0 if direction[axis] > 0 else 1)] if axis < 2 else 'Z'


def main():
    tris, slots = load_triangles(sys.argv[1])
    data = json.load(open(sys.argv[2]))
    concrete_slot = int(sys.argv[3]) if len(sys.argv) > 3 else None
    rays = np.array(data['rays'], dtype=np.float64)
    origins, directions = rays[:, :3], rays[:, 3:6]
    render, render_tri = first_hits(tris, origins, directions)
    sides = [side_name(d) for d in directions]
    for name, hits in data['meshes'].items():
        print(f'== {name}')
        coll = np.array([np.inf if h is None else h for h in hits])
        for side in ('-X', '+X', '-Y', '+Y'):
            mask = np.array([s == side for s in sides]) & np.isfinite(render)
            if concrete_slot is not None:
                mask &= np.array([render_tri[i] >= 0 and slots[render_tri[i]] == concrete_slot for i in range(len(rays))])
            both = mask & np.isfinite(coll)
            gaps = render[both] - coll[both]
            missed = int((mask & ~np.isfinite(coll)).sum())
            if both.sum() == 0:
                print(f'  {side}: no rays')
                continue
            print(f'  {side}: rays={int(mask.sum())} median gap={np.median(gaps):.0f} cm p90={np.percentile(gaps, 90):.0f} '
                  f'within 30 cm={100 * np.mean(gaps < 30):.0f}% behind surface={100 * np.mean(gaps < -5):.0f}% '
                  f'collision missed={missed}')


if __name__ == '__main__':
    main()
