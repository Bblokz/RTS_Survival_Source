exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/landing/landing_lib.py').read())
out = []
for v in (-4000, -3600, -3200, -2800, -2500, 2500, 2800, 3200, 3600, 4000):
    row = []
    for u in range(-5200, 5201, 400):
        x, y = W(u, v)
        z = landscape_z(x, y)
        row.append('%5.0f' % (z if z is not None else -1))
    out.append('v%5d: ' % v + ' '.join(row))
out.append('u cols: ' + ' '.join('%5d' % u for u in range(-5200, 5201, 400)))
for u in (-5200, -4800, -4400, -4000, 4000, 4400, 4800, 5200):
    row = []
    for v in range(-4000, 4001, 400):
        x, y = W(u, v)
        z = landscape_z(x, y)
        row.append('%5.0f' % (z if z is not None else -1))
    out.append('u%5d: ' % u + ' '.join(row))
out.append('v cols: ' + ' '.join('%5d' % v for v in range(-4000, 4001, 400)))
# tile top + what sits near the slab
x, y = W(0, 0); out.append('slab centre hit %s' % (ground_hit(x, y),))
for a in all_actors():
    p = a.get_actor_location()
    u, v = L(p.x, p.y)
    if abs(u) < 7000 and abs(v) < 6000 and not isinstance(a, (unreal.Landscape, unreal.LandscapeStreamingProxy)):
        out.append('NEAR %-40s %-45s u %6.0f v %6.0f z %5.0f yaw %5.0f' % (a.get_actor_label()[:40], a.get_class().get_name()[:45], u, v, p.z, a.get_actor_rotation().yaw))
open(ROOT + 'ring.txt', 'w').write('\n'.join(out))
print('ok', len(out))
