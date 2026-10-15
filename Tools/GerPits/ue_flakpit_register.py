"""Registers the German Flak 37 pit with the async spawner: its expansion class and its placement preview mesh.

Run inside the editor (bridge pyfile or the editor's Python console) after build_gerpit_bxps.py Flak37.
"""
import unreal

SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Flak37'
PREVIEW_MESH = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/SM_GerPit_Flak'

spawner = unreal.load_asset(SPAWNER)
cdo = unreal.get_default_object(spawner.generated_class())
spawner.modify()
expansion_type = unreal.BuildingExpansionType.BTX_GER_FLAK37_PIT
classes = cdo.get_editor_property('building_expansion_map')
classes[expansion_type] = unreal.load_asset(BXP).generated_class()
cdo.set_editor_property('building_expansion_map', classes)
previews = cdo.get_editor_property('bxp_preview_mesh_map')
previews[expansion_type] = unreal.load_asset(PREVIEW_MESH)
cdo.set_editor_property('bxp_preview_mesh_map', previews)
saved = unreal.EditorLoadingAndSavingUtils.save_packages([spawner.get_outermost()], False)
cdo = unreal.get_default_object(spawner.generated_class())
print('FLAKPIT_REGISTER saved', saved, cdo.get_editor_property('building_expansion_map')[expansion_type],
      cdo.get_editor_property('bxp_preview_mesh_map')[expansion_type].get_name())
