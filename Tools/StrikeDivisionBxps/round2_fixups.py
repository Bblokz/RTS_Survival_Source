"""Headless fix-ups after the round-2 test (UnrealEditor-Cmd -run=pythonscript, real RHI for the mask check):
selection materials + energy component on the van and shop (copied from the cipher room, which lacks both),
DT_Items old rows against HEAD (copied to /Game/__SDBxp2Tmp), mask-live check of the van hole master."""
import json
import unreal

OUT = []


def flush():
    open('D:/UE5Projects/RTS_Survival/Saved/SDBxpRound2/fixups.txt', 'w', encoding='utf-8').write(chr(10).join(OUT))


SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SLIB = unreal.SubobjectDataBlueprintFunctionLibrary
G = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps'


def comp(bp, cls):
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        obj = SLIB.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), bp)
        if obj is not None and isinstance(obj, cls):
            return obj
    return None


loft = unreal.load_asset(G + '/Barracks/SniperLoft/Ger_Bxp_BarracksSniperLoft')
loft_sel = comp(loft, unreal.SelectionComponent)
selected = loft_sel.get_editor_property('SelectedMaterial')
deselected = loft_sel.get_editor_property('DeselectedMaterial')
OUT.append('loft selection %s %s' % (selected.get_name() if selected else None, deselected.get_name() if deselected else None))
for path in (G + '/MechDepot/RadioVan/Ger_Bxp_MechDepotRadioVan', G + '/MechDepot/WurfrahmenShop/Ger_Bxp_MechDepotWurfrahmenShop'):
    bp = unreal.load_asset(path)
    sel = comp(bp, unreal.SelectionComponent)
    sel.set_editor_property('SelectedMaterial', selected)
    sel.set_editor_property('DeselectedMaterial', deselected)
    energy = comp(bp, unreal.BuildingExpansionEnergyComponent)
    if energy is None:
        root = SDS.k2_gather_subobject_data_for_blueprint(bp)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.BuildingExpansionEnergyComponent,
                                              blueprint_context=bp)
        handle, fail = SDS.add_new_subobject(params)
        OUT.append('energy add %s %s' % (path.split('/')[-1], 'ok' if fail.is_empty() else str(fail)))
        SDS.rename_subobject(handle, unreal.Text('BxpEnergy'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    ok = unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    OUT.append('fixed %s saved=%s energy=%s sel=%s' % (path.split('/')[-1], ok,
               comp(bp, unreal.BuildingExpansionEnergyComponent) is not None,
               comp(bp, unreal.SelectionComponent).get_editor_property('SelectedMaterial')))

flush()
now = unreal.load_asset('/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionItems/DT_Items')
head = unreal.load_asset('/Game/__SDBxp2Tmp/DT_Items')
OUT.append('head loaded %s' % head)
rows_now = {r['Name']: r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(now))}
rows_head = {r['Name']: r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(head))}
changed = [n for n, r in rows_head.items() if rows_now.get(n) != r]
OUT.append('DT_Items head=%d now=%d changed_old_rows=%s new=%s' % (len(rows_head), len(rows_now), changed,
           sorted(set(rows_now) - set(rows_head))))
for name in changed[:3]:
    OUT.append('  head %s' % json.dumps(rows_head[name])[:300])
    OUT.append('  now  %s' % json.dumps(rows_now.get(name))[:300])

MEL = unreal.MaterialEditingLibrary
for master in (G + '/MechDepot/RadioVan/Materials/M_SD_PBR_BcNormalOrm',):
    masked = unreal.load_asset(master + '_WallHoles')
    source = unreal.load_asset(master)
    OUT.append('mask %s masked=%d opaque=%d blend=%s' % (master.split('/')[-1],
               MEL.get_statistics(masked).num_pixel_shader_instructions,
               MEL.get_statistics(source).num_pixel_shader_instructions, masked.get_editor_property('blend_mode')))

sp = unreal.load_asset('/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner')
cdo = unreal.get_default_object(sp.generated_class())
OUT.append('spawner %d %d' % (len(cdo.get_editor_property('BuildingExpansionMap')), len(cdo.get_editor_property('BxpPreviewMeshMap'))))
open('D:/UE5Projects/RTS_Survival/Saved/SDBxpRound2/fixups.txt', 'w', encoding='utf-8').write('\n'.join(OUT))
