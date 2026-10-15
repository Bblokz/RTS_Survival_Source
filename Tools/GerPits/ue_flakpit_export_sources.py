"""Exports what the German 3.7 cm Flak pit is authored against (the Flak 37 team weapon gun, its poses and the flak
crew animations) to the Blender folder, and prints the team weapon's crew spots and the gun's bones.

Run as a commandlet: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -NullRHI
Writes FBX files to D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Source and
flakpit_sources.json (animation lengths, crew spots).
"""
import json
import unreal

OUT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Source'
FLAK_ANIMS = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/TeamWeaponAnims/Flak/'
FLAK = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Flak37mm/'

MESHES = [FLAK + 'Mesh/SK_TW_Flak37MM']
ANIMS = [
    FLAK_ANIMS + 'AN_Infantry_Flak_Load_Standing',
    FLAK_ANIMS + 'AN_Infantry_Flak_ControlWheels',
    FLAK_ANIMS + 'AN_Infantry_Flak_CrouchedScan',
    FLAK + 'FireAnims/Anim_TW_Flak37_Base',
    FLAK + 'FireAnims/Anim_TW_Flak37_Fire',
    FLAK + 'FireAnims/Anim_TW_Flak37_Packed',
    FLAK + 'FireAnims/Anim_TW_Flak37_Up35',
]
MONTAGES = [FLAK_ANIMS + 'Montage_TW_FlakLoadStanding', FLAK_ANIMS + 'Montage_TW_FlakCrouched',
            FLAK_ANIMS + 'Montage_TW_FlakWheelController', FLAK + 'FireAnims/Montage_TWFlak37_Fire']


def export(asset, exporter):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = '%s/%s.fbx' % (OUT_DIR, asset.get_name())
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    task.exporter = exporter
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log_warning('FLAKPIT_EXPORT %s %s' % (asset.get_name(), ok))


def crew_spots():
    spots = {}
    bp = unreal.load_asset(FLAK + 'Ger_TW_Flak37MM')
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        component = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(component, unreal.SceneComponent):
            location = component.get_editor_property('relative_location')
            rotation = component.get_editor_property('relative_rotation')
            spots[component.get_name()] = {'class': component.get_class().get_name(),
                                          'location': [location.x, location.y, location.z],
                                          'rotation': [rotation.pitch, rotation.yaw, rotation.roll]}
    cdo = unreal.get_default_object(bp.generated_class())
    try:
        embedded = cdo.get_editor_property('M_TeamWeaponConfig').get_editor_property('m_embedded_settings')
        spots['turret_height'] = embedded.get_editor_property('turret_height')
    except Exception as error:
        spots['turret_height'] = str(error)
    return spots


info = {}
for path in MESHES:
    mesh = unreal.load_asset(path)
    export(mesh, unreal.SkeletalMeshExporterFBX())
    skeleton = mesh.get_editor_property('skeleton')
    info['sockets'] = [(s.get_editor_property('socket_name'), s.get_editor_property('bone_name'),
                        [s.get_editor_property('relative_location').x, s.get_editor_property('relative_location').y,
                         s.get_editor_property('relative_location').z]) for s in skeleton.sockets] \
        if hasattr(skeleton, 'sockets') else 'n/a'
    info['materials'] = [str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('materials')]
for path in ANIMS:
    anim = unreal.load_asset(path)
    export(anim, unreal.AnimSequenceExporterFBX())
    info[anim.get_name()] = {'length': anim.get_play_length()}
for path in MONTAGES:
    montage = unreal.load_asset(path)
    info[montage.get_name()] = {'length': montage.get_play_length()}
info['crew_spots'] = crew_spots()
with open(OUT_DIR + '/flakpit_sources.json', 'w') as handle:
    json.dump(info, handle, indent=1, default=str)
unreal.log_warning('FLAKPIT_EXPORT info %s' % json.dumps(info, default=str))
