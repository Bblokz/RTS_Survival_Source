# Unreal (editor python): creates / updates DA_<Building>_Deploy (UNomadicDeployAnimationData) from the baked event
# timeline and the smoke screen preset, and sets it as DeployAnimationData on the truck blueprint.
# Smoke screen (picked 2026-11-10, "3 Smoke Screen"): light smoke bursts around the footprint at the very start that hide
# the first seconds, then the base emerges; only heavy pieces (structures, mast) kick dust where they land. Every burst
# also plays on pack-up when the progress falls past it (smoke hides the truck re-forming at the end).
# Globals: BUILDING, WORK (dir with deploy_events.json / config.json), FOLDER, DEPLOY_MESH, TRUCK_BP.
import json

import unreal

EAL = unreal.EditorAssetLibrary
FX = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/FX/'
LIGHT_SMOKE = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke'
HEAVY_DUST = FX + 'NS_Deploy_HeavyDust'
HQ_FOOTPRINT_RADIUS = 980.0  # smoke scales below were tuned on the SD HQ (about 20 x 19 m)

config = json.load(open(WORK + '/config.json'))
events = json.load(open(WORK + '/deploy_events.json'))['events']
mesh = unreal.load_asset(DEPLOY_MESH)
bounds = mesh.get_bounds()
centre, ext = bounds.origin, bounds.box_extent
smoke = config.get('smoke_screen', {})
# building expansions (config "mode": "bxp") are a few metres across: their smoke may shrink further than a building's
MIN_SIZE_FACTOR = smoke.get('min_size_factor', 0.25 if config.get('mode') == 'bxp' else 0.55)
size_factor = max(MIN_SIZE_FACTOR, min(1.6, max(ext.x, ext.y) / HQ_FOOTPRINT_RADIUS))
ring = smoke.get('ring', 0.7)  # fraction of the footprint half-extent where the ring of smoke stands


def burst(progress, location, scale, system, sound=False):
    b = unreal.NomadicDeployEffectBurst()
    b.set_editor_property('progress', float(progress))
    b.set_editor_property('local_location', unreal.Vector(*location))
    b.set_editor_property('scale', float(scale))
    b.set_editor_property('system', unreal.load_asset(system))
    b.set_editor_property('play_construction_sound', sound)
    return b


# six points on an ellipse around the footprint + the centre, on the ground (z = bounds bottom)
ground = centre.z - ext.z
points = [(centre.x + ext.x * ring * dx, centre.y + ext.y * ring * dy, ground)
          for dx, dy in ((-1, -0.6), (0.9, -0.8), (-1, 0.5), (0.8, 0.7), (0.0, 0.0), (-0.2, 1.0))]
bursts = []
waves = smoke.get('waves', [[0.01, 1.7, [0, 1, 2, 3, 4, 5]], [0.16, 1.4, [0, 1, 2, 3]], [0.32, 1.2, [2, 3, 4, 5]]])
for progress, scale, indices in waves:
    for i in indices:
        bursts.append(burst(progress, points[i], scale * size_factor, LIGHT_SMOKE, sound=(i == 4)))
heavy = 0
for e in events:
    if e['cat'] not in ('structure', 'mast'):
        continue
    scale = (1.6 if e['cat'] == 'mast' else 1.1) * min(max(e['size'] / 300.0, 0.8), 1.4)
    bursts.append(burst(e['t'], e['centre'], scale, HEAVY_DUST, sound=True))
    heavy += 1

# extra bursts at fixed spots (config "extra_bursts": [{"progress", "location" [x,y,z mesh cm], "scale", "system":
# "smoke" | "dust"}]), e.g. to cover runtime building attachments (actors on sockets) that appear when the deploy
# finishes and vanish when the pack-up starts: put a burst at progress ~0.985 on each attachment socket
for extra in config.get('extra_bursts', []):
    system = LIGHT_SMOKE if extra.get('system', 'dust') == 'smoke' else HEAVY_DUST
    bursts.append(burst(extra['progress'], extra['location'], extra.get('scale', 1.0), system, extra.get('sound', False)))
    heavy += 1 if system == HEAVY_DUST else 0

path = FOLDER + '/DA_%s_Deploy' % BUILDING
if EAL.does_asset_exist(path):
    da = unreal.load_asset(path)
else:
    da = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_%s_Deploy' % BUILDING, FOLDER,
                                                                 unreal.NomadicDeployAnimationData,
                                                                 unreal.DataAssetFactory())
da.set_editor_property('deploy_mesh', mesh)
da.set_editor_property('effect_bursts', bursts)
da.set_editor_property('deploy_mesh_bounds_scale', float(config.get('bounds_scale', 3.0)))
print('data asset', path, 'bursts', len(bursts), '(smoke', len(bursts) - heavy, 'heavy dust', heavy, ') size factor',
      round(size_factor, 2), 'saved', EAL.save_loaded_asset(da, False))

bp = unreal.load_asset(TRUCK_BP)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('deploy_animation_data', da)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
if cdo.get_editor_property('deploy_animation_data') != da:
    cdo.set_editor_property('deploy_animation_data', da)
print('truck', TRUCK_BP, 'DeployAnimationData =', cdo.get_editor_property('deploy_animation_data').get_name(),
      'saved', EAL.save_loaded_asset(bp, False))
