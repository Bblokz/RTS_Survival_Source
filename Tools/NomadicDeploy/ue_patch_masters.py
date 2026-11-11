# Unreal (editor python, uecp_call.py py). Adds the nomadic VAT deploy to the master materials of a building mesh,
# behind the static switch parameter NomadicDeploy (default OFF):
#   WPO = existing WPO (if any) + StaticSwitch(NomadicDeploy, True: MF_NomadicDeploy_VAT, False: 0)
# With the switch off every existing material / instance compiles as before; the per-building deploy material
# instances (ue_deploy_mis.py) turn it on. Incremental edit only (never clears a live master). Idempotent: a master that
# already has the switch gets its WPO link re-made through UNomadicDeployEditorLibrary (older script versions wired the
# pin through UECP, which old masters ignored at compile time).
# Material-attributes masters are skipped (ue_deploy_mis.py gives those slots M_NomadicDeploy_MSPlain).
# Global: MESH (static mesh path).
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
LIB = unreal.NomadicDeployEditorLibrary
MF_PATH = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/Materials/MF_NomadicDeploy_VAT'
SWITCH = 'NomadicDeploy'


def root_material(mi):
    cur = mi
    while isinstance(cur, unreal.MaterialInstance):
        cur = cur.get_editor_property('parent')
    return cur


def has_switch(material):
    return SWITCH in [str(n) for n in MEL.get_static_switch_parameter_names(material)]


def add_switch(material):
    existing, existing_output = LIB.get_world_position_offset_input(material)
    call = MEL.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, -1400, 1400)
    call.set_material_function(unreal.load_asset(MF_PATH))
    zero = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -1400, 1560)
    switch = MEL.create_material_expression(material, unreal.MaterialExpressionStaticSwitchParameter, -1100, 1450)
    switch.set_editor_property('parameter_name', SWITCH)
    switch.set_editor_property('default_value', False)
    switch.set_editor_property('group', 'NomadicDeploy')
    wired = MEL.connect_material_expressions(call, '', switch, 'True') and MEL.connect_material_expressions(zero, '', switch, 'False')
    if existing is None:
        return wired and LIB.connect_world_position_offset(switch, ''), False
    add = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, -800, 1450)
    wired = wired and MEL.connect_material_expressions(existing, existing_output, add, 'A')
    wired = wired and MEL.connect_material_expressions(switch, '', add, 'B')
    return wired and LIB.connect_world_position_offset(add, ''), True


def rewire(material):
    # by the switch's name: UECP node indices don't follow creation order on every material, so an earlier
    # index-based wiring could have fed WPO from the wrong node (silently: no deploy motion on those masters)
    return LIB.wire_switch_to_world_position_offset(material, SWITCH)


def patch(material):
    before = LIB.describe_world_position_offset(material)
    if material.get_editor_property('use_material_attributes'):
        return 'SKIPPED material attributes master'
    if has_switch(material):
        action, ok, kept = 'rewired', rewire(material), None
    else:
        (ok, kept), action = add_switch(material), 'patched'
    LIB.refresh_material(material)
    saved = EAL.save_loaded_asset(material, False)
    if not ok:
        print('ERROR master WPO wiring failed:', material.get_name())
    if not saved:
        print('ERROR master not saved (file locked by a running headless editor?):', material.get_name())
    return '%s ok=%s kept_existing_wpo=%s before[%s] after[%s] saved=%s' % (
        action, ok, kept, before, LIB.describe_world_position_offset(material), saved)


mesh = unreal.load_asset(MESH)
done = set()
for slot in mesh.get_editor_property('static_materials'):
    mi = slot.material_interface
    if mi is None:
        continue
    master = root_material(mi)
    key = master.get_path_name()
    if key in done:
        continue
    done.add(key)
    try:
        print('MASTER', key.split('.')[-1], '|', patch(master))
    except Exception as exc:
        print('MASTER', key, '| FAILED', repr(exc))
