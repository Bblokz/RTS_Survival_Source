"""Creates the bullet fly-by assets in /Game/RTS_Survival/Sounds/BulletFlyBy (run once, needs the
UBulletFlyByDataAsset class in the editor DLL):
  DA_BulletFlyBy   - the sounds picked at random + tuning (UBulletFlyByDataAsset)
  Att_BulletFlyBy  - spatialized, reaches past MaxCameraDistance + PassRadius so zoom fading stays in code
  SC_BulletFlyBy   - engine-side cap of 3 fly-bys, quietest stopped first

Headless:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script=D:/UE5Projects/RTS_Survival/Source/Tools/BulletFlyBy/create_bullet_flyby_assets.py -unattended
Existing assets are updated in place, so rerunning resets their values to the ones below.
"""
import unreal

FOLDER = "/Game/RTS_Survival/Sounds/BulletFlyBy"
WEAPONS_SOUND_CLASS = "/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_Weapons.RTS_SClass_Weapons"

# The short "fast" fly-bys (0.2-0.7 s) read as a passing bullet; the plain/deep/slow/designed ones are 0.6-3 s.
FLYBY_SOUNDS = [
    "/Game/WARFARE_SOUNDS/Weapons/Bullets/Wavs/bullet_flyby_fast_%02d" % i for i in range(1, 13)
] + [
    "/Game/WARFARE_SOUNDS/Weapons/Bullets/Wavs/bullet_flyby_fast_15",
    "/Game/RTS_Survival/Sounds/WeaponSounds/SmallArms/Rifles/Rus/Mosin/bullet_flyby_fast_13",
    "/Game/RTS_Survival/Sounds/WeaponSounds/SmallArms/Rifles/Rus/Mosin/bullet_flyby_fast_14",
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def get_or_create(name, asset_class, factory):
    path = "%s/%s" % (FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    asset = asset_tools.create_asset(name, FOLDER, asset_class, factory)
    if asset is None:
        raise RuntimeError("could not create " + path)
    return asset


attenuation = get_or_create("Att_BulletFlyBy", unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
settings = attenuation.get_editor_property("attenuation")
settings.set_editor_property("attenuate", True)
settings.set_editor_property("spatialize", True)
settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
settings.set_editor_property("attenuation_shape_extents", unreal.Vector(4700.0, 0.0, 0.0))
settings.set_editor_property("falloff_distance", 1500.0)
settings.set_editor_property("distance_algorithm", unreal.AttenuationDistanceModel.LINEAR)
attenuation.set_editor_property("attenuation", settings)

concurrency = get_or_create("SC_BulletFlyBy", unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
concurrency_settings = concurrency.get_editor_property("concurrency")
concurrency_settings.set_editor_property("max_count", 3)
concurrency_settings.set_editor_property("resolution_rule", unreal.MaxConcurrentResolutionRule.STOP_QUIETEST)
concurrency.set_editor_property("concurrency", concurrency_settings)

data_factory = unreal.DataAssetFactory()
data_factory.set_editor_property("data_asset_class", unreal.BulletFlyByDataAsset)
data = get_or_create("DA_BulletFlyBy", unreal.BulletFlyByDataAsset, data_factory)
sounds = []
for sound_path in FLYBY_SOUNDS:
    sound = unreal.load_asset(sound_path)
    if sound is None:
        unreal.log_warning("BulletFlyBy: missing sound " + sound_path)
        continue
    sounds.append(sound)
data.set_editor_property("fly_by_sounds", sounds)
data.set_editor_property("sound_class", unreal.load_asset(WEAPONS_SOUND_CLASS))
data.set_editor_property("attenuation", attenuation)
data.set_editor_property("concurrency", concurrency)
# The camera starts ~3000 cm from its focus; full volume up to 2600, none past 4000.
data.set_editor_property("max_camera_distance", 4000.0)
data.set_editor_property("full_volume_camera_distance", 2600.0)

for asset in (attenuation, concurrency, data):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError("could not save " + asset.get_path_name())
unreal.log("BulletFlyBy: saved %s with %d sounds, %s, %s" % (
    data.get_path_name(), len(sounds), attenuation.get_path_name(), concurrency.get_path_name()))
