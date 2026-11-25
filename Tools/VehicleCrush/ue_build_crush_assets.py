"""Headless build of the vehicle run-over assets named by URTSVehicleCrushSettings.

Run: UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -nullrhi -ExecutePythonScript=<this> -ABSLOG=<log>
Inputs:
  VoiceLines/RunOverScreams/RunOverScream_{Male,Female}_NN.wav  (gen_run_over_screams.py, picked takes)
  Saved/VehicleCrush/T_CrushBloodSmear.png                       (gen_crush_blood_texture.py)
Outputs:
  /Game/RTS_Survival/Sounds/VoiceLines/RunOverScreams/RunOverScream_*   (sound class RTS_SClass_VoiceLine)
  /Game/RTS_Survival/SpecialEffects/VehicleCrush/T_CrushBloodSmear
  /Game/RTS_Survival/SpecialEffects/VehicleCrush/M_CrushBloodDecal       (deferred decal, fades with SetFadeOut)
"""
import glob
import os
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SCREAM_SRC = os.path.join(PROJECT, "VoiceLines", "RunOverScreams")
SMEAR_SRC = os.path.join(PROJECT, "Saved", "VehicleCrush", "T_CrushBloodSmear.png")
SCREAM_DST = "/Game/RTS_Survival/Sounds/VoiceLines/RunOverScreams"
FX_DST = "/Game/RTS_Survival/SpecialEffects/VehicleCrush"
SOUND_CLASS = "/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_VoiceLine"


def log(msg):
    unreal.log("VEHICLE_CRUSH_BUILD " + msg)


def import_file(src, dst_path, name):
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = dst_path
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(dst_path + "/" + name)


def import_screams():
    sound_class = unreal.load_asset(SOUND_CLASS)
    wavs = sorted(glob.glob(os.path.join(SCREAM_SRC, "RunOverScream_*.wav")))
    if not wavs:
        raise RuntimeError("no scream wavs in " + SCREAM_SRC)
    for wav in wavs:
        name = os.path.splitext(os.path.basename(wav))[0]
        sound = import_file(wav, SCREAM_DST, name)
        if not isinstance(sound, unreal.SoundWave):
            raise RuntimeError("import failed: " + wav)
        if sound_class:
            sound.set_editor_property("sound_class_object", sound_class)
        EAL.save_loaded_asset(sound, False)
        log("scream %s %.2f s" % (name, sound.get_editor_property("duration")))


def link(src, src_out, dst, dst_in):
    if not MEL.connect_material_expressions(src, src_out, dst, dst_in):
        raise RuntimeError("link failed %s.%s -> %s" % (src_out, dst_in, dst.get_name()))


def build_decal_material():
    if not os.path.exists(SMEAR_SRC):
        raise RuntimeError("missing " + SMEAR_SRC)
    texture = import_file(SMEAR_SRC, FX_DST, "T_CrushBloodSmear")
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_loaded_asset(texture, False)

    name = "M_CrushBloodDecal"
    path = FX_DST + "/" + name
    mat = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, FX_DST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    smear = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 0)
    smear.set_editor_property("texture", texture)
    smear.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

    tint = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, -300)
    tint.set_editor_property("parameter_name", "BloodTint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    base = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, -200)
    link(smear, "RGB", base, "A")
    link(tint, "", base, "B")

    # Wet: the thick pool is glossy, thin film less so.
    wet = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -500, 150)
    wet.set_editor_property("const_a", 0.55)
    wet.set_editor_property("const_b", 0.12)
    link(smear, "A", wet, "Alpha")

    # SetFadeOut drives the lifetime opacity; without it the decal would pop out.
    lifetime = MEL.create_material_expression(mat, unreal.MaterialExpressionDecalLifetimeOpacity, -900, 400)
    strength = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 550)
    strength.set_editor_property("parameter_name", "BloodOpacity")
    strength.set_editor_property("default_value", 1.0)
    op1 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, 400)
    link(smear, "A", op1, "A")
    link(lifetime, "", op1, "B")
    op2 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 450)
    link(op1, "", op2, "A")
    link(strength, "", op2, "B")

    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(wet, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(op2, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    log("decal material %d expressions" % MEL.get_num_material_expressions(mat))


def run():
    import_screams()
    build_decal_material()
    log("DONE")


try:
    run()
except Exception as exc:  # report and still quit, so the headless run never hangs
    unreal.log_error("VEHICLE_CRUSH_BUILD FAILED " + str(exc))
unreal.SystemLibrary.quit_editor()
