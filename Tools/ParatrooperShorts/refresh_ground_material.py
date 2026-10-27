"""Recompile the existing meadow graph without changing its texture or RVT connections."""
from pathlib import Path
import shutil
import unreal

project = Path('D:/UE5Projects/RTS_Survival')
relative = Path('Content/RTS_Survival/Shorts/M_ParatrooperMeadowDiverseGround.uasset')
backup = project / 'Source/Tools/ParatrooperShorts/Before/GroundShaderRefresh' / relative
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(project / relative, backup)
material = unreal.load_asset('/Game/RTS_Survival/Shorts/M_ParatrooperMeadowDiverseGround')
library = unreal.MaterialEditingLibrary
for prop in [unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_NORMAL, unreal.MaterialProperty.MP_ROUGHNESS]:
    source = library.get_material_property_input_node(material, prop)
    assert source
    assert library.connect_material_property(source, '', prop)
library.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
print('SHORTS_GROUND_SHADER_REFRESHED graph preserved, recovery copy:', backup)
