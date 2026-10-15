"""Headless editor step: exports the sand flipbook textures the stone impact is made grey from.
Run: UnrealEditor-Cmd <uproject> -run=pythonscript -script=export_sand_flipbooks.py <output folder>"""
import sys
import unreal

OUT = sys.argv[-1]
MATERIALS = '/Game/RTS_Survival/SpecialEffects/AllExplosions/Materials/'
for name in ['MI_Explosion_Sand_11', 'MI_Explosion_Sand_12']:
    mi = unreal.load_asset(MATERIALS + name)
    for p in mi.texture_parameter_values:
        tex = p.parameter_value
        task = unreal.AssetExportTask()
        task.object = tex
        task.filename = f'{OUT}/{tex.get_name()}.png'
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.TextureExporterPNG()
        ok = unreal.Exporter.run_asset_export_task(task)
        unreal.log(f'RTS_STONE_TEX {name} {p.parameter_info.name} {tex.get_path_name()} '
                   f'{tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()} '
                   f'{tex.get_editor_property("compression_settings")} srgb={tex.get_editor_property("srgb")} '
                   f'lod={tex.get_editor_property("lod_group")} exported={ok}')
