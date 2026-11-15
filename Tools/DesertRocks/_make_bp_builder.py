# One-off: derives ue_blueprints.py from the Ore crushable rock builder (kept for the record).
import shutil

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreCrushableRocks/ue_blueprints.py'
p = 'D:/UE5Projects/RTS_Survival/Source/Tools/DesertRocks/ue_blueprints.py'
s = open(SRC, encoding='utf-8').read()


def rep(old, new):
    global s
    if old not in s:
        raise SystemExit('not found: ' + old[:80])
    s = s.replace(old, new)


def between(start, end, new):
    global s
    i = s.index(start)
    j = s.index(end)
    s = s[:i] + new + s[j:]


a = s.index('import json')
s = '''"""Desert rock kit: the base and the 16 rock Blueprints, headless (run ue_import.py first).

  bash run_ue.sh ue_blueprints.py bps -nullrhi

Idempotent; never deletes a Blueprint. Report: Saved/DesertRocks/blueprints_report.json.

/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/
  BP_DesertRock_Base   a DUPLICATE of BP_OreCrushRock_Base (parent BP_NaturalCover -> C++ ADestructableActorHealth),
                       made once; it keeps that Blueprint's graph (crush + death) and its WallHoleDamage / NavBlock
                       components. ADestructableActorHealth::TakeDamage forwards every hit to WallHoleDamage.
  BP_DesertRock_<L|M><N>_<Kind>   graph-less children: mesh, health, holes (DamagedMaterials = MI_DesertRock_WallHoles,
                       NoGrid), nav box (NavArea_Null: nobody paths through a desert rock), aim points.
Not crushable: Crushable and Overlap Tanks are off, so the base graph only runs the death by damage
(WallHoleDamage.CollapseIntoPieces: the rock bursts into SM_DesertRock_Debris_* chunks and the actor goes).
"""
''' + s[a:]
rep("HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks'",
    "HOME = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks'")
rep("SRC_BASE = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/BP_BerlinCover_Base'",
    "SRC_BASE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Blueprints/BP_OreCrushRock_Base'\n"
    "ORE_DEBRIS = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Meshes/SM_OreRock_Debris_%d'\n"
    "HOLES_MI = HOME + '/Materials/MI_DesertRock_WallHoles'")
rep("BASE_NAME = 'BP_OreCrushRock_Base'", "BASE_NAME = 'BP_DesertRock_Base'")
rep("CustomMapItems/Environment/OreCrushableRocks/out/rocks.json", "CustomMapItems/Environment/DesertRockKit/Model/out/rocks.json")
rep("Saved/OreMountains/rocks/blueprints_report.json", "Saved/DesertRocks/blueprints_report.json")
between("RULES = {", "TIP_SPEED", '''# hole radius: cm per mm of calibre, capped so a hole never opens through the rock (medium rocks are 1-1.8 m thick)
RULES = {
    'medium': dict(health=2500.0, reduction=20.0, radius=0.8, max_hole=70.0, min_hole=12.0, area_per_piece=3000.0,
                   max_hit=3, collapse_pieces=30, spacing=45.0, aims=1),
    'large': dict(health=6000.0, reduction=25.0, radius=1.0, max_hole=110.0, min_hole=15.0, area_per_piece=4000.0,
                  max_hit=4, collapse_pieces=80, spacing=70.0, aims=3),
}
''')
rep("'ROCK_BP '", "'DROCK_BP '")
between("def nav_area_class(name):", "# ---------------------------------------------------------------- templates", '''def nav_area_class(name=None):
    return unreal.NavArea_Null.static_class()


''')
rep("nav_area_class(RULES['small']['area'])", "nav_area_class()")
between("def setup_holes_template", "def save(bp)", '''def setup_holes_template(comp, debris, rule):
    """Procedural shell holes on the rock (NoGrid, two-sided masked instance); a death by damage bursts the rock into
    debris (called by the base graph, so collapse_into_chunks_on_death off)."""
    comp.set_editor_property('WallMeshComponentName', 'MainMesh')
    comp.set_editor_property('DamagedMaterials', [unreal.load_asset(HOLES_MI)])
    comp.set_editor_property('DebrisMeshes', debris)
    hole = comp.get_editor_property('HoleSettings')
    hole.set_editor_property('hole_radius_per_calibre_mm', rule['radius'])
    hole.set_editor_property('max_hole_radius', rule['max_hole'])
    comp.set_editor_property('HoleSettings', hole)
    grid = comp.get_editor_property('GridSettings')
    grid.set_editor_property('grid_axis', unreal.WallHoleGridAxis.NO_GRID)
    grid.set_editor_property('hole_centre_outset_share', 0.0)
    grid.set_editor_property('min_hole_radius', rule['min_hole'])
    grid.set_editor_property('area_per_debris_piece', rule['area_per_piece'])
    grid.set_editor_property('max_pieces_per_hit', rule['max_hit'])
    grid.set_editor_property('max_collapse_pieces', rule['collapse_pieces'])
    grid.set_editor_property('collapse_piece_spacing', rule['spacing'])
    comp.set_editor_property('GridSettings', grid)
    collapse = comp.get_editor_property('CollapseSettings')
    collapse.set_editor_property('collapse_into_chunks_on_death', False)
    collapse.set_editor_property('destroy_owner_delay_seconds', 3.0)
    collapse.set_editor_property('death_speed_scale', 0.8)
    collapse.set_editor_property('collapse_sound', unreal.load_asset(DEATH_CUE))
    collapse.set_editor_property('collapse_attenuation', unreal.load_asset(COLLAPSE_ATT))
    collapse.set_editor_property('collapse_concurrency', unreal.load_asset(COLLAPSE_CONC))
    comp.set_editor_property('CollapseSettings', collapse)


''')
between("def build_one", "def main()", '''def aim_points(mesh, count):
    b = mesh.get_bounding_box()
    z = round(max(b.max.z * 0.5, 30.0))
    pts = [unreal.Vector(0.0, 0.0, z)]
    if count > 1:
        dx = round((b.max.x - b.min.x) * 0.25)
        pts += [unreal.Vector(dx, 0.0, round(z * 0.8)), unreal.Vector(-dx, 0.0, round(z * 0.8))]
    return pts


def build_one(rock, info, base, debris):
    rule = RULES[info['class']]
    mesh = unreal.load_asset(f'{MESH}/SM_DesertRock_{rock}')
    if mesh is None:
        raise RuntimeError('mesh missing SM_DesertRock_' + rock)
    bp, created = get_or_create('BP_DesertRock_' + rock, base)
    comps = named(bp)
    box, top_cm = nav_box(info)
    setup_mesh_template(own(bp, comps, 'MainMesh'), mesh)
    setup_health_template(own(bp, comps, 'HealthComp'), rule['health'], rule['reduction'])
    setup_holes_template(own(bp, comps, 'WallHoleDamage'), debris, rule)
    setup_nav_box(own(bp, comps, 'NavBlock'), box, nav_area_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('Crushable', False)
    cdo.set_editor_property('Overlap Tanks', False)
    cdo.set_editor_property('health', rule['health'])
    cdo.set_editor_property('aim_offset_points', aim_points(mesh, rule['aims']))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = save(bp)
    return dict(blueprint=bp.get_path_name(), created=created, saved=saved, size_class=info['class'],
                health=rule['health'], damage_reduction=rule['reduction'], hole=[rule['radius'], rule['max_hole']],
                top_cm=round(top_cm),
                nav_box=[round(box['cx']), round(box['cy']), round(box['hu']), round(box['hv']), round(box['yaw'], 1),
                         round(box['z1'])])


''')
rep('''    for i in range(6):
        m = unreal.load_asset(f'{MESH}/SM_OreRock_Debris_{i}')
        if m is None:
            raise RuntimeError('debris missing: run ue_import.py')
        debris.append(m)''', '''    for i in range(6):
        m = unreal.load_asset(f'{MESH}/SM_DesertRock_Debris_{i}')
        if m is None:
            # prototype runs (not every rock built yet): borrow the Ore chunks
            m = unreal.load_asset(ORE_DEBRIS % i)
            report.setdefault('borrowed_debris', []).append(i)
        debris.append(m)''')
open(p, 'w', encoding='utf-8').write(s)
print('ok', s.count('OreRock'), 'OreRock mentions')
