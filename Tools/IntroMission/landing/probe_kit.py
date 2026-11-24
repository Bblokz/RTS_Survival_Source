# Lineup of fence / gate / light / tower candidates next to the depot for a look + measurements. Not saved; destroys
# them again in kit_clear(). Writes kit.txt (mesh local bbox, components incl. lights).
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/landing/landing_lib.py').read())
ENV = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/'
KIT = [
    ('bp', ENV + 'Industrial/BP_Industrial_Dest_FenceWire'), ('bp', ENV + 'Airport/BP_Airport_Dest_FenceWire'),
    ('bp', ENV + 'Military/ConcreteWall/BP_Dest_PostApo_ConcreteFence'), ('bp', ENV + 'Military/ConcreteWall/BP_Dest_PostApo_ConcreteFenceLong'),
    ('bp', ENV + 'Military/ConcreteWall/BP_Dest_PostApo_ConcreteFenceCorner'), ('bp', ENV + 'Military/ConcreteWall/BP_Dest_PostApo_ConcreteFenceGate'),
    ('bp', ENV + 'Military/ConcreteWall/BP_Dest_Military_ConcreteFence_1'), ('bp', ENV + 'Military/ConcreteWall/BP_Dest_Military_ConcreteFence_2'),
    ('bp', '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Wall/Rus_Building_GateWired_Soviet'),
    ('bp', '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/BP_RusWallGateA'),
    ('bp', ENV + 'Dest_SovietFlag'),
    ('bp', ENV + 'Industrial/Poles/BP_Industrial_Dest_CrushablePoleLight_1'), ('bp', ENV + 'Industrial/Poles/BP_Industrial_Dest_CrushablePoleLight_2'),
    ('bp', ENV + 'Industrial/Poles/BP_Industrial_Dest_CrushablePoleLight_3'), ('bp', ENV + 'Civilian/Poles/BP_Civilian_Dest_CrushablePoleLight'),
    ('sm', '/Game/MilitaryAirport/Meshes/Props/SM_FloodLight_1'), ('sm', '/Game/MilitaryAirport/Meshes/Props/SM_FloodLight_2'),
    ('sm', '/Game/MilitaryAirport/Meshes/Props/SM_FloodLight_3'), ('sm', '/Game/MilitaryAirport/Meshes/Props/SM_FloodLight_4'),
    ('sm', '/Game/Underground/Meshes/Props/SM_Stand_Up_Floodlight'), ('sm', '/Game/MilitaryBaseCamp/Meshes/SM_LampPost'),
    ('bp', '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_GuardTowerMG_Soviet'),
    ('bp', '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_GuardTower'),
    ('bp', '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_LargeGuardTower'),
]
out = []
def kit_clear():
    clear_prefix('KIT_')
kit_clear()
for k, (kind, path) in enumerate(KIT):
    u = -3400 + (k % 12) * 620
    v = -1400 if k < 12 else 1000
    x, y = W(u, v)
    name = path.split('/')[-1]
    try:
        a = spawn_bp(path, 'KIT_%02d_%s' % (k, name), (x, y, 147), YAW) if kind == 'bp' else \
            spawn_mesh(path, 'KIT_%02d_%s' % (k, name), (x, y, 147), YAW)
    except Exception as ex:
        out.append('FAIL %s %s' % (name, ex)); continue
    out.append('%02d %s  at u %d v %d' % (k, name, u, v))
    for c in a.get_components_by_class(unreal.SceneComponent):
        line = '    %s %s' % (c.get_class().get_name(), c.get_name())
        if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
            bb = c.static_mesh.get_bounding_box()
            rl = c.get_editor_property('relative_location'); sc = c.get_editor_property('relative_scale3d'); rr = c.get_editor_property('relative_rotation')
            line += ' mesh %s min(%.0f,%.0f,%.0f) max(%.0f,%.0f,%.0f) rel(%.0f,%.0f,%.0f) rot(%.0f,%.0f,%.0f) scale(%.2f,%.2f,%.2f) vis %s' % (
                c.static_mesh.get_name(), bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z, rl.x, rl.y, rl.z,
                rr.pitch, rr.yaw, rr.roll, sc.x, sc.y, sc.z, c.is_visible())
        if isinstance(c, unreal.LightComponent):
            line += ' LIGHT intensity %.1f units %s colour %s rel %s' % (c.intensity, c.get_editor_property('intensity_units') if hasattr(c, 'intensity_units') else '', c.light_color, c.get_editor_property('relative_location'))
        out.append(line)
open(ROOT + 'kit.txt', 'w').write('\n'.join(out))
eye = W(-600, -5600)
_defer = view_shot('kit_lineup', (eye[0], eye[1], 2600), (W(0, -200)[0], W(0, -200)[1], 200))
print('kit spawned', len(KIT))
