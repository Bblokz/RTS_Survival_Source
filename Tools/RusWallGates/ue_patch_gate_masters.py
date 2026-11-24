"""Unreal editor python: give the Soviet wall concrete masters the gate VAT switch (for the moving gate leaves).

- M_RusBunker_Concrete and M_RusBunker_Concrete_WallHoles get WPO = StaticSwitch(WallGateVAT, MF_WallGate_VAT, 0),
  default OFF, so every existing wall/bunker instance compiles as before.
- A first version of this script added NomadicDeploy -> MF_NomadicDeploy_VAT here; that switch is repointed to
  MF_WallGate_VAT and renamed WallGateVAT (MF_NomadicDeploy_VAT reads UV4/5, which fails on these masters' skeletal
  mesh usage: GPU skin vertex factories take at most 4 UV sets).
- The holes master's WorldPosition node is switched to "excluding material offsets", so a hole is tested against the
  REST pose of a VAT leaf and travels with it when the leaf opens (no change for meshes without WPO).
Idempotent. Run ue_gate_mf.py first.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
LIB = unreal.NomadicDeployEditorLibrary
MF_PATH = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/Materials/MF_WallGate_VAT'
NOMADIC_MF = 'MF_NomadicDeploy_VAT'
SWITCH, OLD_SWITCH = 'WallGateVAT', 'NomadicDeploy'
MATS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/'


def find_all(material, cls_name):
    out = []
    for i in range(80):
        e = unreal.find_object(material, '%s_%d' % (cls_name, i))
        if e is not None:
            out.append(e)
    return out


def add_switch(material):
    existing, existing_output = LIB.get_world_position_offset_input(material)
    call = MEL.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, -1400, 1400)
    call.set_material_function(unreal.load_asset(MF_PATH))
    zero = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -1400, 1560)
    switch = MEL.create_material_expression(material, unreal.MaterialExpressionStaticSwitchParameter, -1100, 1450)
    switch.set_editor_property('parameter_name', SWITCH)
    switch.set_editor_property('default_value', False)
    switch.set_editor_property('group', 'WallGate')
    ok = MEL.connect_material_expressions(call, '', switch, 'True') and MEL.connect_material_expressions(zero, '', switch, 'False')
    if existing is None:
        return ok and LIB.connect_world_position_offset(switch, '')
    add = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, -800, 1450)
    ok = ok and MEL.connect_material_expressions(existing, existing_output, add, 'A')
    ok = ok and MEL.connect_material_expressions(switch, '', add, 'B')
    return ok and LIB.connect_world_position_offset(add, '')


def repoint_old(material):
    """The earlier NomadicDeploy switch on this master -> WallGateVAT + MF_WallGate_VAT. True if found."""
    calls = [c for c in find_all(material, 'MaterialExpressionMaterialFunctionCall')
             if c.get_editor_property('material_function') and
             c.get_editor_property('material_function').get_name() == NOMADIC_MF]
    switches = [s for s in find_all(material, 'MaterialExpressionStaticSwitchParameter')
                if str(s.get_editor_property('parameter_name')) == OLD_SWITCH]
    if not calls and not switches:
        return False
    for c in calls:
        c.set_material_function(unreal.load_asset(MF_PATH))
    for s in switches:
        s.set_editor_property('parameter_name', SWITCH)
        s.set_editor_property('group', 'WallGate')
    return True


def rest_pose_world_position(material):
    changed = []
    for e in find_all(material, 'MaterialExpressionWorldPosition'):
        e.set_editor_property('world_position_shader_offset',
                              unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        changed.append(e.get_name())
    return changed


for name in ('M_RusBunker_Concrete', 'M_RusBunker_Concrete_WallHoles'):
    m = unreal.load_asset(MATS + name)
    names = [str(n) for n in MEL.get_static_switch_parameter_names(m)]
    if repoint_old(m):
        ok, action = LIB.wire_switch_to_world_position_offset(m, SWITCH), 'repointed'
    elif SWITCH in names:
        ok, action = LIB.wire_switch_to_world_position_offset(m, SWITCH), 'rewired'
    else:
        ok, action = add_switch(m), 'patched'
    wp = rest_pose_world_position(m) if name.endswith('_WallHoles') else []
    LIB.refresh_material(m)
    print(name, action, 'ok', ok, 'switches', [str(n) for n in MEL.get_static_switch_parameter_names(m)],
          'wpo', LIB.describe_world_position_offset(m), 'restpose', wp, 'saved', EAL.save_loaded_asset(m, False))
