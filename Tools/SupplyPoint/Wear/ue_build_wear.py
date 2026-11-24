"""Final build of the supply point wear materials (sp_wear_materials.py) into the real Materials folder, and the
assignment on the six supply point meshes. Run ONCE in the shared editor (UECP execute_python):
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Wear/ue_build_wear.py').read())
Check get_pie_status first (no mesh edits while PIE runs). Saves only its own assets and the five meshes, and writes
the result to Saved/SupplyPointWear/build_result.txt (execute_python output comes back tripled).
To retune later, edit the MI parameters; do not rerun build_steel/build_timber on the live masters.
"""
import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Materials'
MESHES = '/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Meshes/'
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Wear/sp_wear_materials.py').read())
RESULT = []


def note(msg):
    RESULT.append(msg)
    unreal.log('WEARBUILD ' + msg)


def save(asset):
    ok = EAL.save_loaded_asset(asset, False)
    note(f'save {asset.get_path_name()} {ok}')
    return ok


def assign(assets):
    for mesh_name, plan in ASSIGN.items():
        mesh = unreal.load_asset(MESHES + mesh_name)
        for index, slot in enumerate(mesh.get_editor_property('static_materials')):
            slot_name = str(slot.get_editor_property('material_slot_name'))
            want = plan.get(slot_name)
            if want:
                mesh.set_material(index, assets[want])
                note(f'{mesh_name} slot {index} {slot_name} -> {want}')
        save(mesh)


def main():
    if EAL.does_asset_exist(HOME + '/M_SupplyPoint_WornSteel'):
        raise RuntimeError('already built: retune the instances instead')
    assets = build(HOME)
    for asset in assets.values():
        save(asset)
    assign(assets)
    for name in ('M_SupplyPoint_WornSteel', 'M_SupplyPoint_Timber'):
        s = MEL.get_statistics(assets[name])
        note(f'{name} PS {s.num_pixel_shader_instructions} samplers {s.num_samplers}')


try:
    main()
finally:
    open(unreal.Paths.project_saved_dir() + 'SupplyPointWear/build_result.txt', 'w').write('\n'.join(RESULT))
