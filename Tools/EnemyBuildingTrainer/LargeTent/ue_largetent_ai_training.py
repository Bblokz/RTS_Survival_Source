"""Soviet large tent: AI training activity (Source/Docs/EnemyAI/EnemyBuildingTraining.md).

Run headless (never in the shared editor), e.g.
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
      -unattended -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate

Adds to Rus_Building_LargeTent only (the _Soviet child inherits everything; its inherited templates are not touched):
  - EnemyBuildingTrainer (UEnemyBuildingTrainerComponent, Infantry)
  - AITrainingExit in front of the +X gable door, +X out
  - while training (tag AITrainingActivity, idle look is the template default):
      * lit doorways: a warm card in the recessed U-shaped door seam of both gables (rectangle + diamond for the
        rounded bottom; the card sits between the seam panel and the gable wall, so the wall hides its overflow)
      * lit windows: warm cards just outside the five sloped window panels (+Y: 2, -Y: 2, one of them in two parts)
      * a warm point light and a light-pool decal in front of the exit door
      * stove smoke (P_RTS_RusBarracks_Smoke, the Soviet base chimney smoke) from a stove pipe on the +Y roof slope
  - the stove pipe and its roof collar are permanent (the tent's death destroys the actor, nothing floats)
Idempotent: components are found by variable name and updated; materials are created once and updated.
Geometry comes from the SM_RusLargeTent OBJ export (door seam at y -139..-61, recessed 6 cm; window panel corners).
"""
import json
import math
import traceback

import unreal

RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings'
PARENT_BP = RUS + '/Rus_Building_LargeTent'
CHILD_BP = RUS + '/Rus_Building_LargeTent_Soviet'
HOME = RUS + '/Tent/AITraining'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/EnemyBuildingTrainer/LargeTent/edit_result.json'

ACTIVITY = 'AITrainingActivity'
PLANE = '/Engine/BasicShapes/Plane'
CYLINDER = '/Engine/BasicShapes/Cylinder'
LAMP_GLASS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/M_BF_LampGlass'
LAMP_POOL = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/M_BF_LampPoolDecal'
PIPE_MATERIAL = '/Game/RTS_Survival/Blueprints/Environment/FrozenLake/TripleCrown/Materials/MI_TC_BlackenedSteel'
SMOKE = '/Game/RTS_Survival/SpecialEffects/Realistic_Starter_VFX_Pack/Particles/Smoke/P_RTS_RusBarracks_Smoke'

# All tuning in one place: a tuning pass is a rerun.
TUNE = {
    'warm': (1.0, 0.45, 0.14),
    'glass_base': (0.20, 0.10, 0.04),
    'window_glow': 5.0,
    'door_glow': 8.0,
    'flicker': 0.12,
    'pool_colour': (1.0, 0.55, 0.22),
    'pool_intensity': 0.6,
    'light_lumens': 1200.0,
    'light_radius': 650.0,
    'light_colour': (255, 158, 82),
    'smoke_scale': 0.6,
    'window_inset_cm': 4.0,
    'window_offset_cm': 2.0,
}
# Porch foot of the Zeltpodest (ue_largetent_podest.py, 2026-11-22); was (1040, -100) in front of the +X gable door.
EXIT_LOCATION = (1720.0, -100.0, 0.0)
DOOR_X = 697.0  # between the recessed seam panel (x 694) and the gable wall (x 700)
DOOR_Y = -100.0
PIPE_XY = (-330.0, 110.0)
PIPE_BOTTOM_Z = 222.0  # 5 cm into the +Y roof slope (roof z 226.9 here)
PIPE_TOP_Z = 360.0
PIPE_DIAMETER = 13.0

# Window panels from the mesh: (x0, x1, top (y, z), bottom (y, z)); y sign = side.
WINDOWS = {
    'AITraining_WindowGlow_PY_A': (266.0, 431.0, (196.5, 176.0), (223.5, 86.0)),
    'AITraining_WindowGlow_PY_B': (-197.0, -32.0, (196.0, 176.0), (218.0, 86.0)),
    'AITraining_WindowGlow_NY_C': (-431.0, -265.0, (-216.5, 175.0), (-232.0, 85.0)),
    'AITraining_WindowGlow_NY_DTop': (265.0, 431.0, (-216.5, 175.0), (-229.0, 130.0)),
    'AITraining_WindowGlow_NY_DBot': (265.0, 431.0, (-229.0, 130.0), (-232.0, 85.0)),
}

SUBS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary
MEL = unreal.MaterialEditingLibrary
R = {'errors': [], 'created': [], 'updated': []}


def V(x, y, z):
    return unreal.Vector(x, y, z)


def lin(c):
    return unreal.LinearColor(c[0], c[1], c[2], 1.0)


# ---------------------------------------------------------------- materials
def make_mi(name, parent_path, vectors, scalars):
    path = HOME + '/' + name
    mi = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mi is None:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        R['created'].append(path)
    MEL.set_material_instance_parent(mi, unreal.load_asset(parent_path))
    for key, value in vectors.items():
        MEL.set_material_instance_vector_parameter_value(mi, key, lin(value))
    for key, value in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, key, value)
    MEL.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    return mi


def make_materials():
    unreal.EditorAssetLibrary.make_directory(HOME)
    window = make_mi('MI_RusTent_WindowGlow', LAMP_GLASS,
                     {'BF_LampColor': TUNE['warm'], 'BF_LampGlassBase': TUNE['glass_base']},
                     {'BF_LampGlow': TUNE['window_glow'], 'BF_LampFlicker': TUNE['flicker']})
    door = make_mi('MI_RusTent_DoorGlow', LAMP_GLASS,
                   {'BF_LampColor': TUNE['warm'], 'BF_LampGlassBase': TUNE['glass_base']},
                   {'BF_LampGlow': TUNE['door_glow'], 'BF_LampFlicker': TUNE['flicker']})
    pool = make_mi('MI_RusTent_LightPool', LAMP_POOL, {'BF_LampPoolColor': TUNE['pool_colour']},
                   {'BF_LampPoolIntensity': TUNE['pool_intensity']})
    return window, door, pool


# ---------------------------------------------------------------- subobjects
def handles_by_name(bp):
    out = {}
    for handle in SUBS.k2_gather_subobject_data_for_blueprint(bp):
        data = LIB.get_data(handle)
        out[str(LIB.get_variable_name(data))] = handle
    return out


def actor_handle(bp):
    # The first handle is the actor itself (checked on this Blueprint: display name Rus_Building_LargeTent).
    handle = SUBS.k2_gather_subobject_data_for_blueprint(bp)[0]
    if hasattr(LIB, 'is_component') and LIB.is_component(LIB.get_data(handle)):
        raise RuntimeError('first subobject handle is a component, not the actor')
    return handle


def set_visible(comp, value):
    try:
        comp.set_editor_property('visible', value)
    except Exception:
        comp.set_visibility(value, False)
    R.setdefault('visible_set', {})[comp.get_name()] = comp.is_visible()


def ensure(bp, name, cls, parent_name):
    names = handles_by_name(bp)
    if name in names:
        handle = names[name]
        R['updated'].append(name)
    else:
        parent = names[parent_name] if parent_name else actor_handle(bp)
        params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp)
        handle, fail = SUBS.add_new_subobject(params)
        if not LIB.is_handle_valid(handle):
            raise RuntimeError('add %s failed: %s' % (name, fail))
        if not SUBS.rename_subobject(handle, unreal.Text(name)):
            raise RuntimeError('rename to %s failed' % name)
        R['created'].append(name)
        handle = handles_by_name(bp)[name]
    template = LIB.get_object_for_blueprint(LIB.get_data(handle), bp)
    if template is None or template.get_class() != cls.static_class():
        raise RuntimeError('%s template %s is not %s' % (name, template, cls))
    return template


def quiet(comp, tagged=True):
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)
    comp.set_editor_property('cast_shadow', False)
    comp.set_editor_property('receives_decals', False)
    comp.set_editor_property('component_tags', [unreal.Name(ACTIVITY)] if tagged else [])
    if tagged:
        set_visible(comp, False)  # idle look


def place(comp, location, rotation, scale=(1.0, 1.0, 1.0)):
    comp.set_editor_property('relative_location', V(*location))
    comp.set_editor_property('relative_rotation', rotation)
    comp.set_editor_property('relative_scale3d', V(*scale))


def glow_card(bp, name, material, location, rotation, scale):
    comp = ensure(bp, name, unreal.StaticMeshComponent, 'BuildingMeshComponent')
    comp.set_editor_property('static_mesh', unreal.load_asset(PLANE))
    comp.set_editor_property('override_materials', [material])
    quiet(comp)
    place(comp, location, rotation, scale)


def door_cards(bp, door_mi):
    for suffix, sign in (('PX', 1.0), ('NX', -1.0)):
        # Plane normal +Z -> +X (pitch -90) or -X (pitch +90); local X then spans height, local Y the width.
        pitch = -90.0 * sign
        rect_rot = unreal.Rotator(roll=0.0, pitch=pitch, yaw=0.0)
        glow_card(bp, 'AITraining_DoorGlow_' + suffix, door_mi, (DOOR_X * sign, DOOR_Y, 117.5), rect_rot, (1.55, 0.90, 1.0))
        # Rounded seam bottom (V from y -139 / -61 at z 45 down to z ~8): a diamond, upper half under the rectangle.
        diamond_rot = unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0.0, pitch=0.0, yaw=45.0), rect_rot)
        side = 45.0 * math.sqrt(2.0) / 100.0
        glow_card(bp, 'AITraining_DoorGlowBottom_' + suffix, door_mi, (DOOR_X * sign, DOOR_Y, 45.0), diamond_rot,
                  (side, side, 1.0))


def window_cards(bp, window_mi):
    for name, (x0, x1, top, bottom) in WINDOWS.items():
        dy, dz = bottom[0] - top[0], bottom[1] - top[1]
        slant = math.hypot(dy, dz)
        side_sign = 1.0 if top[0] > 0 else -1.0
        # outward normal in the YZ plane, perpendicular to the slant, pointing away from the tent and up
        ny, nz = -dz / slant, dy / slant
        if ny * side_sign < 0:
            ny, nz = -ny, -nz
        offset = TUNE['window_offset_cm']
        centre = ((x0 + x1) / 2.0, (top[0] + bottom[0]) / 2.0 + ny * offset, (top[1] + bottom[1]) / 2.0 + nz * offset)
        rot = unreal.MathLibrary.make_rot_from_xz(V(1.0, 0.0, 0.0), V(0.0, ny, nz))
        inset = TUNE['window_inset_cm']
        scale = ((x1 - x0 - 2 * inset) / 100.0, max(slant - 2 * inset, slant * 0.6) / 100.0, 1.0)
        glow_card(bp, name, window_mi, centre, rot, scale)
        R.setdefault('windows', {})[name] = {'centre': [round(c, 1) for c in centre], 'normal': [0, round(ny, 3), round(nz, 3)],
                                             'scale': [round(s, 3) for s in scale]}


def light_and_pool(bp, pool_mi):
    light = ensure(bp, 'AITraining_DoorLight', unreal.PointLightComponent, 'BuildingMeshComponent')
    light.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
    light.set_editor_property('intensity', TUNE['light_lumens'])
    light.set_editor_property('attenuation_radius', TUNE['light_radius'])
    light.set_editor_property('light_color', unreal.Color(r=TUNE['light_colour'][0], g=TUNE['light_colour'][1],
                                                          b=TUNE['light_colour'][2], a=255))
    light.set_editor_property('cast_shadows', False)
    light.set_editor_property('source_radius', 4.0)
    light.set_editor_property('component_tags', [unreal.Name(ACTIVITY)])
    set_visible(light, False)
    place(light, (770.0, DOOR_Y, 150.0), unreal.Rotator(0.0, 0.0, 0.0))

    pool = ensure(bp, 'AITraining_LightPool', unreal.DecalComponent, 'BuildingMeshComponent')
    pool.set_editor_property('decal_material', pool_mi)
    pool.set_editor_property('decal_size', V(80.0, 150.0, 150.0))
    pool.set_editor_property('component_tags', [unreal.Name(ACTIVITY)])
    set_visible(pool, False)
    # projects straight down; covers x 730..1030 in front of the +X gable (wall at x 700-716 stays outside)
    place(pool, (880.0, DOOR_Y, 0.0), unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))


def stove(bp):
    pipe_material = unreal.load_asset(PIPE_MATERIAL)
    height = PIPE_TOP_Z - PIPE_BOTTOM_Z
    pipe = ensure(bp, 'AITraining_StovePipe', unreal.StaticMeshComponent, 'BuildingMeshComponent')
    pipe.set_editor_property('static_mesh', unreal.load_asset(CYLINDER))
    pipe.set_editor_property('override_materials', [pipe_material])
    quiet(pipe, tagged=False)
    set_visible(pipe, True)
    place(pipe, (PIPE_XY[0], PIPE_XY[1], (PIPE_TOP_Z + PIPE_BOTTOM_Z) / 2.0), unreal.Rotator(0.0, 0.0, 0.0),
          (PIPE_DIAMETER / 100.0, PIPE_DIAMETER / 100.0, height / 100.0))
    collar = ensure(bp, 'AITraining_StovePipeCollar', unreal.StaticMeshComponent, 'BuildingMeshComponent')
    collar.set_editor_property('static_mesh', unreal.load_asset(CYLINDER))
    collar.set_editor_property('override_materials', [pipe_material])
    quiet(collar, tagged=False)
    set_visible(collar, True)
    # flat collar on the +Y roof slope (roof z 226.9 here, slope 20 deg; +Y slope needs a positive roll)
    place(collar, (PIPE_XY[0], PIPE_XY[1], 227.5), unreal.Rotator(roll=20.0, pitch=0.0, yaw=0.0), (0.24, 0.24, 0.07))

    smoke = ensure(bp, 'AITraining_StoveSmoke', unreal.NiagaraComponent, 'BuildingMeshComponent')
    try:
        smoke.set_editor_property('asset', unreal.load_asset(SMOKE))
    except Exception:
        smoke.set_asset(unreal.load_asset(SMOKE), False)
    smoke.set_editor_property('auto_activate', False)  # the trainer activates it; no puff at spawn
    smoke.set_editor_property('component_tags', [unreal.Name(ACTIVITY)])
    s = TUNE['smoke_scale']
    place(smoke, (PIPE_XY[0], PIPE_XY[1], PIPE_TOP_Z + 2.0), unreal.Rotator(0.0, 0.0, 0.0), (s, s, s))


def trainer_and_exit(bp):
    trainer = ensure(bp, 'EnemyBuildingTrainer', unreal.EnemyBuildingTrainerComponent, None)
    trainer.set_editor_property('trainer_kind', unreal.EnemyBuildingTrainerKind.INFANTRY)
    trainer.set_editor_property('training_time_mlt', 1.0)
    trainer.set_editor_property('exit_point_component_name', unreal.Name('AITrainingExit'))
    exit_point = ensure(bp, 'AITrainingExit', unreal.SceneComponent, 'BuildingMeshComponent')
    place(exit_point, EXIT_LOCATION, unreal.Rotator(0.0, 0.0, 0.0))


# ---------------------------------------------------------------- verify
def dump_instance(bp_path, location):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, V(*location), unreal.Rotator(0.0, 0.0, 0.0))
    rows = {}
    for comp in actor.get_components_by_class(unreal.ActorComponent):
        name = comp.get_name()
        if not (name.startswith('AITraining') or name == 'EnemyBuildingTrainer'):
            continue
        row = {'class': comp.get_class().get_name(), 'tags': [str(t) for t in comp.component_tags]}
        if isinstance(comp, unreal.SceneComponent):
            loc = comp.get_world_location() - V(*location)
            row['loc'] = [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]
            row['visible'] = comp.is_visible()
            parent = comp.get_attach_parent()
            row['parent'] = parent.get_name() if parent else None
        if isinstance(comp, unreal.StaticMeshComponent):
            row['materials'] = [m.get_name() if m else None for m in comp.get_materials()]
        if isinstance(comp, unreal.EnemyBuildingTrainerComponent):
            row['kind'] = str(comp.trainer_kind)
        rows[name] = row
    actor.destroy_actor()
    return rows


def main():
    window_mi, door_mi, pool_mi = make_materials()
    bp = unreal.load_asset(PARENT_BP)
    trainer_and_exit(bp)
    door_cards(bp, door_mi)
    window_cards(bp, window_mi)
    light_and_pool(bp, pool_mi)
    stove(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, False):
        raise RuntimeError('saving the parent failed')
    child = unreal.load_asset(CHILD_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(child)
    if not unreal.EditorAssetLibrary.save_loaded_asset(child, False):
        raise RuntimeError('saving the child failed')
    parent_rows = dump_instance(PARENT_BP, (0.0, 0.0, 0.0))
    child_rows = dump_instance(CHILD_BP, (0.0, 8000.0, 0.0))
    R['parent'] = parent_rows
    R['child_matches_parent'] = {k: (v.get('loc'), v.get('tags'), v.get('visible')) ==
                                 (parent_rows.get(k, {}).get('loc'), parent_rows.get(k, {}).get('tags'),
                                  parent_rows.get(k, {}).get('visible')) for k, v in child_rows.items()}
    R['child_missing'] = sorted(set(parent_rows) - set(child_rows))
    R['dirty_after'] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]


try:
    main()
except Exception:
    R['errors'].append(traceback.format_exc())
import os
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(R, open(OUT, 'w'), indent=1, default=str)
print('LARGETENT_AI_TRAINING errors=%d created=%d' % (len(R['errors']), len(R['created'])))
