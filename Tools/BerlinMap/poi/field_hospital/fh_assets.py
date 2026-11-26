"""Editor Python (run with fh_py.py <port> fh_assets.py): the Berlin field hospital assets in
/Game/RTS_Survival/Blueprints/Environment/Berlin/FieldHospital/. Re-runnable: existing assets are updated, not duplicated.

  T_FactionFlag_RedCross      UISkins/Flags/T_FactionFlag_RedCross.png (make_redcross_flag.py)
  MI_RedCrossBanner           M_SovietBanner child (waving cloth), wind values of MI_StrikeDivisionBanner
  Dest_RedCrossFlag           copy of Dest_SovietFlag (destructible pole + banner) with the Red Cross banner
  M_FieldHospital_CellarVoid  unlit black: the dark cellar doorway
  M_FieldHospital_Sheet       lit cloth with a texture parameter: the Red Cross sheet spread on the ground
  BP_BerlinFieldHospital      dressing round SM_BerlinRuin_E5_Shell1St_A's front door, pivot = the shell's pivot,
                              actor tag BerlinFieldHospital (Mission_Berlin_FieldHospital reads its door and rally
                              points from this actor's transform). Visual only: no collision, no navigation effect.
"""
import unreal

DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/FieldHospital'
PNG = r'D:\UE5Projects\RTS_Survival\UISkins\Flags\T_FactionFlag_RedCross.png'
SOVIET_FLAG = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Dest_SovietFlag'
BANNER_MASTER = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/Mat/M_SovietBanner'
WIND_SOURCE = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/FactionFlags/MI_StrikeDivisionBanner'
FURN = '/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Meshes/'
BAGS = '/Game/RTS_Survival/Meshes/Props/SandBags/LooseSandBags/'
TAG = 'BerlinFieldHospital'

lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
report = []


def log(*parts):
    report.append(' '.join(str(p) for p in parts))


def texture():
    task = unreal.AssetImportTask()
    for key, value in (('filename', PNG), ('destination_path', DIR), ('destination_name', 'T_FactionFlag_RedCross'),
                       ('automated', True), ('replace_existing', True), ('save', False)):
        task.set_editor_property(key, value)
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(DIR + '/T_FactionFlag_RedCross')
    tex.set_editor_property('srgb', True)
    lib.save_loaded_asset(tex, False)
    log('texture', tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    return tex


def material_instance(name, parent, tex):
    path = f'{DIR}/{name}'
    mi = unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(
        name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(parent))
    wind = unreal.load_asset(WIND_SOURCE)
    for p in wind.get_editor_property('scalar_parameter_values'):
        mel.set_material_instance_scalar_parameter_value(mi, p.parameter_info.name, p.parameter_value)
    mel.set_material_instance_texture_parameter_value(mi, 'Banner', tex)
    lib.save_loaded_asset(mi, False)
    log('mi', name)
    return mi


def new_material(name):
    path = f'{DIR}/{name}'
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    return tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())


def void_material():
    m = new_material('M_FieldHospital_CellarVoid')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    black = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
    black.set_editor_property('constant', unreal.LinearColor(0.004, 0.0035, 0.003, 1))
    mel.connect_material_property(black, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lib.save_loaded_asset(m, False)
    log('material void')
    return m


def sheet_material(tex):
    m = new_material('M_FieldHospital_Sheet')
    sample = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    sample.set_editor_property('parameter_name', 'Sheet')
    sample.set_editor_property('texture', tex)
    mel.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 250)
    rough.set_editor_property('r', 0.95)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    lib.save_loaded_asset(m, False)
    log('material sheet')
    return m


def components(bp):
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    out = {}
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
        if obj is not None:
            out[obj.get_name().replace('_GEN_VARIABLE', '')] = (h, obj)
    return sub, out


def red_cross_flag(banner_mi):
    path = f'{DIR}/Dest_RedCrossFlag'
    if not lib.does_asset_exist(path):
        lib.duplicate_asset(SOVIET_FLAG, path)
    bp = unreal.load_asset(path)
    _, comps = components(bp)
    banner = comps['StaticMesh1'][1]
    banner.set_editor_property('override_materials', [banner_mi])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    lib.save_loaded_asset(bp, False)
    log('flag', path)


def add_mesh(sub, bp, root, name, mesh, loc, rot=(0, 0, 0), scale=(1, 1, 1), material=None):
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = sub.add_new_subobject(params)
    sub.rename_subobject(handle, unreal.Text(name))
    comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    comp.set_editor_property('static_mesh', unreal.load_asset(mesh))
    comp.set_editor_property('relative_location', unreal.Vector(*loc))
    comp.set_editor_property('relative_rotation', unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    comp.set_editor_property('relative_scale3d', unreal.Vector(*scale))
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    if material is not None:
        comp.set_editor_property('override_materials', [material])
    return comp


def add_arrow(sub, bp, root, name, loc):
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.ArrowComponent, blueprint_context=bp)
    handle, fail = sub.add_new_subobject(params)
    sub.rename_subobject(handle, unreal.Text(name))
    comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    comp.set_editor_property('relative_location', unreal.Vector(*loc))
    comp.set_editor_property('arrow_color', unreal.Color(r=255, g=40, b=40, a=255))


# Door of SM_BerlinRuin_E5_Shell1St_A: centre (500, 210), 1.3 m wide (y 145-275), 3 m high; wall inner face x ~472.
# Squads appear at (640, 210) and walk to (1150, 40) / (1150, 380): keep that lane free of props.
DRESSING = [
    # name, mesh, location, (roll, pitch, yaw), scale, material key
    ('CellarVoid', '/Engine/BasicShapes/Plane', (466, 210, 146), (0, -90, 0), (3.05, 1.36, 1), 'void'),
    ('BagsLeft1', BAGS + 'SM_Ger_LooseSandBag_3', (560, 118, 0), (0, 0, 90), (1, 1, 1), None),
    ('BagsLeft2', BAGS + 'SM_Ger_LooseSandBag_1', (660, 112, 0), (0, 0, 84), (1, 1, 1), None),
    ('BagsLeft3', BAGS + 'SM_Ger_LooseSandBag_4', (600, 120, 30), (0, 0, 96), (0.95, 0.95, 0.95), None),
    ('BagsRight1', BAGS + 'SM_Ger_LooseSandBag_2', (560, 306, 0), (0, 0, -90), (1, 1, 1), None),
    ('BagsRight2', BAGS + 'SM_Ger_LooseSandBag_5', (660, 312, 0), (0, 0, -84), (1, 1, 1), None),
    ('BagsRight3', BAGS + 'SM_Ger_LooseSandBag_3', (610, 304, 30), (0, 0, -98), (0.95, 0.95, 0.95), None),
    ('BagsLeft4', BAGS + 'SM_Ger_LooseSandBag_2', (750, 104, 0), (0, 0, 80), (1, 1, 1), None),
    ('BagsLeft5', BAGS + 'SM_Ger_LooseSandBag_5', (700, 116, 30), (0, 0, 92), (0.95, 0.95, 0.95), None),
    ('BagsRight4', BAGS + 'SM_Ger_LooseSandBag_4', (750, 318, 0), (0, 0, -80), (1, 1, 1), None),
    ('BagsRight5', BAGS + 'SM_Ger_LooseSandBag_1', (705, 308, 30), (0, 0, -94), (0.95, 0.95, 0.95), None),
    ('Sheet', '/Engine/BasicShapes/Plane', (1000, -330, 3), (0, 0, 90), (4.2, 2.5, 1), 'sheet'),
    ('Cot1', FURN + 'SM_BerlinFurn_02_IronBed', (760, -170, 0), (0, 0, 2), (1, 1, 1), None),
    ('Cot2', FURN + 'SM_BerlinFurn_02_IronBed', (760, -330, 0), (0, 0, -3), (1, 1, 1), None),
    ('Cot3', FURN + 'SM_BerlinFurn_02_IronBed', (780, 640, 0), (0, 0, 176), (1, 1, 1), None),
    ('Shelf', FURN + 'SM_BerlinFurn_26_CellarShelf', (560, -40, 0), (0, 0, 180), (1, 1, 1), None),
    ('Crates', FURN + 'SM_BerlinFurn_30_BarrelAndCrates', (620, 470, 0), (0, 0, 35), (1, 1, 1), None),
    ('Lamp', FURN + 'SM_BerlinFurn_29_LampAndRug', (900, 520, 0), (0, 0, -20), (1, 1, 1), None),
    ('DoorLeaf', FURN + 'SM_BerlinFurn_31_DoorLeafFallen', (700, -520, 0), (0, 0, 70), (1, 1, 1), None),
]


def hospital_bp(materials):
    path = f'{DIR}/BP_BerlinFieldHospital'
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.Actor)
    bp = tools.create_asset('BP_BerlinFieldHospital', DIR, unreal.Blueprint, factory)
    sub, comps = components(bp)
    root = sub.k2_gather_subobject_data_for_blueprint(bp)[0]
    for name, mesh, loc, rot, scale, mat in DRESSING:
        add_mesh(sub, bp, root, name, mesh, loc, rot, scale, materials.get(mat))
    for name, loc in (('CellarDoor', (640, 210, 30)), ('MedicRally1', (1150, 40, 30)), ('MedicRally2', (1150, 380, 30))):
        add_arrow(sub, bp, root, name, loc)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('tags', [TAG])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    lib.save_loaded_asset(bp, False)
    log('bp', path, len(DRESSING), 'meshes')


if not lib.does_directory_exist(DIR):
    lib.make_directory(DIR)
tex = texture()
banner = material_instance('MI_RedCrossBanner', BANNER_MASTER, tex)
red_cross_flag(banner)
void = void_material()
sheet_master = sheet_material(tex)
hospital_bp({'void': void, 'sheet': sheet_master})
print('\n'.join(report))
