"""Registers the three round-2 expansions in the async spawner (class + preview mesh maps) and DT_Items (one row
each, borrowed icons). Sent into the editor with round2_editor_lib.py (round2_build.py lib "register()")."""


def register():
    spawner_bp = load('/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner')
    cdo = unreal.get_default_object(spawner_bp.generated_class())
    entries = {
        'RadioVan': unreal.BuildingExpansionType.BTX_GER_MECH_DEPOT_RADIO_VAN,
        'WurfrahmenShop': unreal.BuildingExpansionType.BTX_GER_MECH_DEPOT_WURFRAHMEN_SHOP,
        'SniperLoft': unreal.BuildingExpansionType.BTX_GER_BARRACKS_SNIPER_LOFT,
    }
    classes = cdo.get_editor_property('BuildingExpansionMap')
    previews = cdo.get_editor_property('BxpPreviewMeshMap')
    before = (len(classes), len(previews))
    for key, bxp_type in entries.items():
        job = JOBS[key]
        classes[bxp_type] = unreal.load_class(None, job['home'] + '/' + job['bp'] + '.' + job['bp'] + '_C')
        previews[bxp_type] = load(job['home'] + '/Meshes/' + job['mesh'] + '_PreviewMesh')
    spawner_bp.modify()
    cdo.set_editor_property('BuildingExpansionMap', classes)
    cdo.set_editor_property('BxpPreviewMeshMap', previews)
    unreal.EditorLoadingAndSavingUtils.save_packages([spawner_bp.get_outermost()], False)
    cdo = unreal.get_default_object(spawner_bp.generated_class())
    after = (len(cdo.get_editor_property('BuildingExpansionMap')), len(cdo.get_editor_property('BxpPreviewMeshMap')))
    print('SPAWNER before', before, 'after', after)
    for bxp_type in entries.values():
        print('  ', bxp_type, cdo.get_editor_property('BuildingExpansionMap').get(bxp_type),
              cdo.get_editor_property('BxpPreviewMeshMap').get(bxp_type).get_name())


ROWS = {
    'BTX_GerMechDepotRadioVan': (
        '/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/HQ/T_GER_BXP_HQRadar_Brush',
        'A Kfz. 17 radio van under a camo net with a 12 m aerial tower on the container: the depot sees far further.',
        'Radio Van'),
    'BTX_GerMechDepotWurfrahmenShop': (
        '/Game/RTS_Survival/Blueprints/GameUI/WeaponUI/WeaponIcons/Medium/WpUI_Ger_Turret_PanzerWerfer_C_0_Brush',
        'A lean-to where a 251 is fitted with Wurfrahmen 40 racks: while it stands, Panzerwerfers reload faster.',
        'Wurfrahmen Shop'),
    'BTX_GerBarracksSniperLoft': (
        '/Game/RTS_Survival/Textures/GameUI/SquadWeaponIcons/T_SquadWeaponIcon_KarSniper_Brush',
        'A timber loft behind the barracks with a Kar98k sniper behind sandbags; it covers the front half circle.',
        'Sniper Loft'),
}


def find_asset_path(name):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    found = [str(a.package_name) for a in reg.get_assets(unreal.ARFilter(package_paths=['/Game'], recursive_paths=True))
             if str(a.asset_name) == name]
    return found[0] if found else None


def register_items():
    import json
    dt = load('/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionItems/DT_Items')
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
    before = [r['Name'] for r in rows]
    template = next(r for r in rows if r['Name'] == 'BTX_GerForgeSchreckFrame')
    keys = list(template.keys())
    for row_name, (brush_hint, description, tooltip) in ROWS.items():
        if row_name in before:
            continue
        brush = brush_hint if EAL.does_asset_exist(brush_hint) else find_asset_path(brush_hint.split('/')[-1])
        brush_asset = load(brush)
        row = dict(template)
        row['Name'] = row_name
        for key in keys:
            if key.startswith('Built_ImageBrush'):
                row[key] = brush_asset.get_path_name()
            elif key.startswith('Description'):
                row[key] = description
            elif key.startswith('Name_'):
                row[key] = row_name
            elif key.startswith('Tooltip'):
                row[key] = tooltip
        rows.append(row)
    dt.modify()
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows))
    dt.modify()
    EAL.save_asset(dt.get_path_name().split('.')[0], False)
    after = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
    unchanged = all(any(a == b for a in after) for b in [r for r in json.loads(json.dumps(rows)) if r['Name'] in before])
    print('DT_Items rows', len(before), '->', len(after), 'old rows unchanged', unchanged)
    for r in after:
        if r['Name'] in ROWS:
            print('  ', json.dumps(r)[:400])
