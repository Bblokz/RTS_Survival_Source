"""Unreal editor python: material instances, hole instances, collision and bounds for the Soviet wall gates.

Run after ue_import_gates.py and ue_patch_gate_masters.py. Idempotent.
All gate materials hang under the wall's own concrete (MI_Rus_BunkerConcrete_05 / its _WallHoles twin):
  frame slot Concrete  -> MI_Rus_BunkerConcrete_05 (exactly the wall)
  frame slot GateSteel -> MI_RusWallGate_Steel (same master, ConcreteDiffuse = T_RusWallGate_Atlas)
  leaf slots           -> MI_RusWallGate<X>_Leaf_<Slot>: the above + WallGateVAT switch ON + the gate's VAT
Hole instances (UWallHoleDamageComponent DamagedMaterials) mirror that under MI_Rus_BunkerConcrete_05_WallHoles, no grid.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates'
MESHES, TEX, MATS, HOLES = ROOT + '/Meshes', ROOT + '/Textures', ROOT + '/Materials', ROOT + '/Holes'
CONCRETE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/MI_Rus_BunkerConcrete_05'
CONCRETE_HOLES = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Meshes/Holes/MI_Rus_BunkerConcrete_05_WallHoles'
STEEL_PARAMS = {'Normal_Power': 0.3, 'Metallic_Multiplier': 0.85, 'Roughness_Multiplier': 0.95, 'SandStrength': 0.35,
                'DiffuseTilling': 1.0}
FRAMES = 61
for f in (MATS, HOLES):
    EAL.make_directory(f)


def mic(name, folder, parent):
    path = folder + '/' + name
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else \
        TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset(parent) if isinstance(parent, str) else parent)
    return mi


def steel(mi):
    MEL.set_material_instance_texture_parameter_value(mi, 'ConcreteDiffuse', unreal.load_asset(TEX + '/T_RusWallGate_Atlas'))
    for k, v in STEEL_PARAMS.items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)


def vat(mi, key):
    MEL.set_material_instance_static_switch_parameter_value(mi, 'WallGateVAT', True)
    MEL.set_material_instance_texture_parameter_value(mi, 'WallGateVatOffset', unreal.load_asset('%s/T_RusWallGate%s_VAT_Offset' % (TEX, key)))
    MEL.set_material_instance_texture_parameter_value(mi, 'WallGateVatRot', unreal.load_asset('%s/T_RusWallGate%s_VAT_Rot' % (TEX, key)))
    MEL.set_material_instance_scalar_parameter_value(mi, 'WallGateVatFrames', float(FRAMES))


def no_grid(mi, mesh):
    box = mesh.get_bounding_box()
    vals = {'WallGridAlongY': 0.0, 'WallGridMinAlong': box.min.x, 'WallGridMaxAlong': box.max.x,
            'WallGridMinZ': box.min.z, 'WallGridMaxZ': box.max.z, 'WallGridCellSize': 150.0,
            'WallGridColumns': 0.0, 'WallGridRows': 1.0}
    for k, v in vals.items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, float(v))


def done(mi):
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    return mi


steel_mi = mic('MI_RusWallGate_Steel', MATS, CONCRETE)
steel(steel_mi)
done(steel_mi)
steel_holes = mic('MI_RusWallGate_Steel_WallHoles', HOLES, CONCRETE_HOLES)
steel(steel_holes)
done(steel_holes)

report = {}
for key in ('A', 'C'):
    frame = unreal.load_asset('%s/SM_RusWallGate%s_Frame' % (MESHES, key))
    leaf = unreal.load_asset('%s/SM_RusWallGate%s_Leaf' % (MESHES, key))
    # frame: wall concrete + gate steel; holes without grid
    frame.set_material(0, unreal.load_asset(CONCRETE))
    frame.set_material(1, steel_mi)
    fc = mic('MI_RusWallGate%s_Frame_Concrete_WallHoles' % key, HOLES, CONCRETE_HOLES)
    no_grid(fc, frame)
    done(fc)
    fs = mic('MI_RusWallGate%s_Frame_Steel_WallHoles' % key, HOLES, steel_holes)
    no_grid(fs, frame)
    done(fs)
    report['%s frame holes' % key] = [fc.get_name(), fs.get_name()]
    # leaf: VAT children per slot, plus VAT hole instances
    leaf_slots, leaf_holes = [], []
    for i, slot in enumerate(leaf.get_editor_property('static_materials')):
        sname = str(slot.material_slot_name)
        if sname == 'Concrete':
            intact_parent, holes_parent = CONCRETE, CONCRETE_HOLES
        else:
            intact_parent, holes_parent = steel_mi, steel_holes
        li = mic('MI_RusWallGate%s_Leaf_%s' % (key, sname), MATS, intact_parent)
        vat(li, key)
        done(li)
        leaf.set_material(i, li)
        lh = mic('MI_RusWallGate%s_Leaf_%s_WallHoles' % (key, sname), HOLES, holes_parent)
        vat(lh, key)
        no_grid(lh, leaf)
        done(lh)
        leaf_slots.append(li.get_name())
        leaf_holes.append(lh.get_name())
    report['%s leaf slots' % key] = leaf_slots
    report['%s leaf holes' % key] = leaf_holes
    for m in (frame, leaf):
        EAL.save_loaded_asset(m, False)
for k, v in report.items():
    print(k, v)
