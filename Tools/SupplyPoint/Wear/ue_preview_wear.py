"""Wear-pass preview renders in a THROWAWAY editor (never the shared one; nothing is saved):
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Builds the wear materials in memory under /Game/_SupplyPointWearPreview, then renders BP_SupplyPoint_TowerCrane with
the live materials (before) and with the wear materials at two wear levels. Logs WEARPREVIEW lines (shader stats too).
Set env WEAR_TAG to suffix the output names.
"""
import os
import traceback

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/SupplyPointWear/renders'
BP = '/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/BP_SupplyPoint_TowerCrane'
TRUCK = '/Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/BP_Ger_OpelSupplyTruck'
HOME = '/Game/_SupplyPointWearPreview'
TAG = os.environ.get('WEAR_TAG', '')
REBUILD_EXISTING = True
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Wear/sp_wear_materials.py').read())
NS = {}
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py').read(), NS)
NS['RENDER_DIR'] = OUT
S = {'t': 0.0, 'pass': 0, 'busy': False}
# extra wear level for the comparison (the default instance is the "medium" look)
HEAVY = {'RustAmount': 0.48, 'RustLowBoost': 0.45, 'StreakAmount': 0.6, 'PaintFade': 0.5}


def log(msg):
    unreal.log('WEARPREVIEW ' + str(msg))


def stats(m):
    try:
        s = MEL.get_statistics(m)
        log(f'{m.get_name()} PS instructions {s.num_pixel_shader_instructions} VS {s.num_vertex_shader_instructions} '
            f'samplers {s.num_samplers}')
    except Exception as error:  # noqa: BLE001
        log(f'{m.get_name()} stats unavailable: {error}')


def build_all():
    os.makedirs(OUT, exist_ok=True)
    assets = build(HOME)
    assets['MI_SupplyPoint_CraneSteel_Heavy'] = make_instance(HOME, 'MI_SupplyPoint_CraneSteel_Heavy',
                                                              assets['M_SupplyPoint_WornSteel'], scalars=HEAVY)
    for name in ('M_SupplyPoint_WornSteel', 'M_SupplyPoint_Timber'):
        stats(assets[name])
    return assets


def override(comps, assets, steel_name):
    for c in comps:
        if not isinstance(c, unreal.StaticMeshComponent) or not c.static_mesh:
            continue
        plan = ASSIGN.get(c.static_mesh.get_name())
        if not plan:
            continue
        for index, slot in enumerate(c.static_mesh.get_editor_property('static_materials')):
            want = plan.get(str(slot.get_editor_property('material_slot_name')))
            if want == 'MI_SupplyPoint_CraneSteel':
                want = steel_name
            if want:
                c.set_material(index, assets[want])


def shoot(name, assets, steel_name, azimuth, elevation, padding, fov=18.0, res=1800):
    spawned = []
    try:
        actors, comps = NS['_stage_parts']([dict(asset=BP, yaw=0), dict(asset=TRUCK, loc=(120, 330, 0), yaw=0),
                                            dict(asset='/Engine/BasicShapes/Plane', loc=(0, 0, -1),
                                                 scale=(45.0, 45.0, 1.0))], spawned)
        if steel_name:
            override(comps, assets, steel_name)
        frame = [c for c in comps if not (isinstance(c, unreal.StaticMeshComponent) and c.static_mesh
                                          and c.static_mesh.get_name() == 'Plane')]
        NS['_capture_staged'](name + TAG, actors, frame, spawned, res=res, azimuth=azimuth, elevation=elevation,
                              fov=fov, padding=padding, light_intensity=8.0, ambient_intensity=1.5)
    finally:
        NS['_destroy'](spawned)


def render(assets, suffix):
    world = NS['_world']()
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    try:
        views = (('live', None),) if not assets else (('before', None), ('after', 'MI_SupplyPoint_CraneSteel'),
                                                       ('heavy', 'MI_SupplyPoint_CraneSteel_Heavy'))
        for label, steel in views:
            shoot(f'rts_{label}{suffix}', assets, steel, 54.2, 32.4, 0.9)
            shoot(f'low_{label}{suffix}', assets, steel, 20.0, 14.0, 0.62, fov=24.0)
    finally:
        unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 1')


def tick(delta):
    if S['busy']:
        return
    S['t'] += delta
    S['busy'] = True
    try:
        if S['pass'] == 0 and S['t'] > 8.0:
            S['pass'] = 1
            # WEAR_LIVE_ONLY=1: no scratch build, render the saved (live) materials only
            S['assets'] = {} if os.environ.get('WEAR_LIVE_ONLY') else build_all()
            log('built, waiting for shaders')
            S['t'] = 0.0
        elif S['pass'] == 1 and S['t'] > (20.0 if not S['assets'] else 50.0):
            S['pass'] = 2
            render(S['assets'], '_warmup')
            S['t'] = 0.0
        elif S['pass'] == 2 and S['t'] > 10.0:
            S['pass'] = 3
            render(S['assets'], '')
            log('DONE')
            unreal.SystemLibrary.quit_editor()
    except Exception:
        log('FAILED ' + traceback.format_exc())
        S['pass'] = 99
        unreal.SystemLibrary.quit_editor()
    finally:
        S['busy'] = False


S['handle'] = unreal.register_slate_post_tick_callback(tick)
