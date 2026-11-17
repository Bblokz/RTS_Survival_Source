"""Editor-side helpers for the 2026-11-17 Strike Division BXP round (radio van, Wurfrahmen shop, sniper loft).
Sent into the live editor (UECP execute_python) by round2_build.py together with a one-line call such as
`import_job('RadioVan')`. No os / file access: the sandbox blocks it. Every function saves only its own assets.
Hole materials need Tools/WallChunks/make_wall_holes_material.py concatenated before this file.
"""
import unreal

EAL = unreal.EditorAssetLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SLIB = unreal.SubobjectDataBlueprintFunctionLibrary

BLENDER = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/StrikeDivisionBXPs/'
GERBXPS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps'
SHARED_HOLES = GERBXPS + '/SharedHoles'
PREVIEW_MATERIAL = '/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_ConstructionPrev'
GRID = '/Engine/EngineMaterials/WorldGridMaterial'

SANDBAG = '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas'
MODULE = '/Game/RTS_Survival/Meshes/Military/MilitaryBaseScene/ModuleAtlas/M_MB_ModuleAtlas'
PROPS = '/Game/RTS_Survival/Meshes/Military/MilitaryBaseScene/Props/Atlas/M_MB_PropsAtlas'
WEAPON_RACK = ('/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT1/OpelBlitz_Barracks/'
               'TempTentBarracks/M_WeaponRack')
SDKFZ = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/'
HULL_251 = SDKFZ + 'Sdkfz-251_75mm/Meshes/Materials/MI_Sdkfz_251_Hull_DesertAPO'
FRONT_WHEEL_251 = SDKFZ + 'Sdkfz-250/Meshes/Materials/FrontWheels/M_Sdkfz-250_FrontWheel'
WHEELS_251 = SDKFZ + 'Sdkfz-250/Meshes/Materials/MI_Sdkfz-250_Wheels'
TRACKS_251 = SDKFZ + 'Sdkfz-250/Meshes/Materials/Tracks/MI_Sdkfz-250_Tracks'
SANDBAG_HOLES = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/Holes/MI_GerPit_GerSandBag_WallHoles'

VAN_HOME = GERBXPS + '/MechDepot/RadioVan'
VAN_MASTER = VAN_HOME + '/Materials/M_SD_PBR_BcNormalOrm'
VAN_MI = VAN_HOME + '/Materials/MI_GerMechDepot_RadioVan'

# Per expansion: Blender model folder, UE home, FBX base name, slot name -> material, debris count, sockets to zero,
# holeable slot names (-> hole material key), base Blueprint to copy, new Blueprint name.
JOBS = {
    'RadioVan': dict(
        model=BLENDER + 'MechDepot_RadioVan/Model/', home=VAN_HOME, mesh='SM_GerMechDepot_RadioVan', debris=0,
        slots={'M_GerMechDepot_RadioVan': VAN_MI, 'M_SandbagScaffoldAtals': SANDBAG, 'M_MB_ModuleAtlas': MODULE},
        holes={'M_GerMechDepot_RadioVan': 'van', 'M_SandbagScaffoldAtals': 'sandbag'},
        sockets=('Aerial', 'Van'),
        base_bp='/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/CommunicationsCenter/'
                'BXPs/CipherRoom/Ger_Bxp_CommCipherRoom',
        bp='Ger_Bxp_MechDepotRadioVan'),
    'WurfrahmenShop': dict(
        model=BLENDER + 'MechDepot_WurfrahmenShop/Model/', home=GERBXPS + '/MechDepot/WurfrahmenShop',
        mesh='SM_GerMechDepot_WurfrahmenShop', debris=8,
        slots={'M_MB_ModuleAtlas': MODULE, 'Sdkfz_251_D_psd': HULL_251, 'Sdkfz_251_DiffuseCamo': HULL_251,
               'M_LeftWheel': TRACKS_251, 'Sdkfz_251_Wheel_D_psd': FRONT_WHEEL_251, 'Tracks_Wheels_D_psd': WHEELS_251},
        holes={'M_MB_ModuleAtlas': 'module', 'Sdkfz_251_D_psd': 'hull251'},
        sockets=('Roof', 'Rockets', 'Jig'),
        base_bp='/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/CommunicationsCenter/'
                'BXPs/CipherRoom/Ger_Bxp_CommCipherRoom',
        bp='Ger_Bxp_MechDepotWurfrahmenShop'),
    'SniperLoft': dict(
        model=BLENDER + 'Barracks_SniperLoft/Model/', home=GERBXPS + '/Barracks/SniperLoft',
        mesh='SM_GerBarracks_SniperLoft', debris=6,
        slots={'M_SandbagScaffoldAtals': SANDBAG, 'M_WeaponRack': WEAPON_RACK, 'M_MB_ModuleAtlas': MODULE,
               'M_MB_PropsAtlas': PROPS},
        holes={'M_SandbagScaffoldAtals': 'sandbag', 'M_WeaponRack': 'weaponrack', 'M_MB_ModuleAtlas': 'module'},
        sockets=('Turret', 'Aim_Parapet', 'Aim_Net', 'Aim_Trestle', 'Aim_Landing'),
        base_bp=GERBXPS + '/Forge/SchreckFrame/Ger_Bxp_LFactorySchreckFrame',
        bp='Ger_Bxp_BarracksSniperLoft'),
}


def save(asset):
    if not EAL.save_loaded_asset(asset, False):
        print('SAVE FAILED', asset.get_path_name())


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError('missing ' + path)
    return asset


# ------------------------------------------------------------------------------------------------- meshes

def import_fbx(filename, destination, name):
    """Legacy FBX importer (Interchange ignores FbxImportUI): normals as authored, no materials, no collision."""
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    ASSET_TOOLS.import_asset_tasks([task])
    return load(destination + '/' + name)


def slot_names(mesh):
    return [str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('static_materials')]


def finish_mesh(mesh, material_for_slot, collision=None, sockets=()):
    for index, slot in enumerate(slot_names(mesh)):
        path = material_for_slot(slot)
        if path is None:
            print('  UNMAPPED SLOT', mesh.get_name(), index, slot)
            continue
        mesh.set_material(index, load(path))
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.set_editor_property('enabled', False)
    mesh.set_editor_property('nanite_settings', nanite)
    for socket_name in sockets:
        socket = mesh.find_socket(socket_name)
        if socket:
            # Blender SOCKET_ empties import at pitch 90; every socket here is a plain position / identity frame.
            socket.set_editor_property('relative_rotation', unreal.Rotator(0, 0, 0))
            socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
    if collision is not None:
        mesh.set_editor_property('complex_collision_mesh', collision)
        body = mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    save(mesh)


def import_job(key):
    job = JOBS[key]
    meshes = job['home'] + '/Meshes'
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        collision = import_fbx(job['model'] + job['mesh'] + '_Collision.FBX', meshes, job['mesh'] + '_Collision')
        main = import_fbx(job['model'] + job['mesh'] + '.FBX', meshes, job['mesh'])
        preview = import_fbx(job['model'] + job['mesh'] + '_PreviewMesh.FBX', meshes, job['mesh'] + '_PreviewMesh')
        debris = [import_fbx(job['model'] + '%s_Debris_%d.FBX' % (job['mesh'], i), meshes + '/Debris',
                             '%s_Debris_%d' % (job['mesh'], i)) for i in range(job['debris'])]
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    finish_mesh(collision, lambda slot: GRID)
    finish_mesh(main, lambda slot: job['slots'].get(slot), collision, job['sockets'])
    finish_mesh(preview, lambda slot: PREVIEW_MATERIAL, collision)
    for piece in debris:
        finish_mesh(piece, lambda slot: job['slots'].get(slot))
    box = main.get_bounding_box()
    print('MESH', key, 'tris', main.get_num_triangles(0), 'slots', slot_names(main), 'bounds', box.min, box.max)
    for socket_name in job['sockets']:
        socket = main.find_socket(socket_name)
        print('  socket', socket_name, socket.get_editor_property('relative_location') if socket else 'MISSING')
    print('  debris', [(d.get_name(), slot_names(d)) for d in debris])


# ------------------------------------------------------------------------------------------------- van material

def import_texture(filename, destination, name, kind):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    ASSET_TOOLS.import_asset_tasks([task])
    texture = load(destination + '/' + name)
    if kind == 'normal':
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property('srgb', False)
        # Baked as an OpenGL (+Y) tangent normal map; Unreal expects DirectX.
        texture.set_editor_property('flip_green_channel', True)
    if kind == 'masks':
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property('srgb', False)
    save(texture)
    return texture


def van_material():
    """Plain (no material attributes) PBR master: BaseColor, Normal, ORM (R AO, G roughness, B metallic), plus the
    van's instance. Plain on purpose: make_wall_holes_material.py cannot convert attribute masters."""
    folder = VAN_HOME + '/Materials'
    textures = VAN_HOME + '/Textures'
    source = JOBS['RadioVan']['model'] + 'Textures/'
    bc = import_texture(source + 'T_GerMechDepot_RadioVan_BC.png', textures, 'T_GerMechDepot_RadioVan_BC', 'color')
    nrm = import_texture(source + 'T_GerMechDepot_RadioVan_N.png', textures, 'T_GerMechDepot_RadioVan_N', 'normal')
    orm = import_texture(source + 'T_GerMechDepot_RadioVan_ORM.png', textures, 'T_GerMechDepot_RadioVan_ORM', 'masks')
    if not EAL.does_asset_exist(VAN_MASTER):
        material = ASSET_TOOLS.create_asset('M_SD_PBR_BcNormalOrm', folder, unreal.Material, unreal.MaterialFactoryNew())
        base = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, -300)
        base.set_editor_property('parameter_name', 'BaseColor')
        base.set_editor_property('texture', bc)
        normal = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
        normal.set_editor_property('parameter_name', 'Normal')
        normal.set_editor_property('texture', nrm)
        normal.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        packed = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 300)
        packed.set_editor_property('parameter_name', 'ORM')
        packed.set_editor_property('texture', orm)
        packed.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        MEL.connect_material_property(base, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
        MEL.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
        MEL.connect_material_property(packed, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        MEL.connect_material_property(packed, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
        MEL.connect_material_property(packed, 'B', unreal.MaterialProperty.MP_METALLIC)
        MEL.recompile_material(material)
        save(material)
    master = load(VAN_MASTER)
    if not EAL.does_asset_exist(VAN_MI):
        instance = ASSET_TOOLS.create_asset('MI_GerMechDepot_RadioVan', folder, unreal.MaterialInstanceConstant,
                                            unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(instance, master)
        MEL.set_material_instance_texture_parameter_value(instance, 'BaseColor', bc)
        MEL.set_material_instance_texture_parameter_value(instance, 'Normal', nrm)
        MEL.set_material_instance_texture_parameter_value(instance, 'ORM', orm)
        MEL.update_material_instance(instance)
        save(instance)
    print('VAN MATERIAL', VAN_MASTER, VAN_MI)


# ------------------------------------------------------------------------------------------------- holes

def hole_material(key, mesh):
    """Masked two-sided NoGrid hole instance for a slot; shared ones once for all three expansions."""
    if key == 'sandbag':
        return load(SANDBAG_HOLES)
    if key == 'module':
        return make_holes_instance(MODULE, SHARED_HOLES, mesh, grid_axis=None, two_sided=True)
    if key == 'weaponrack':
        return make_holes_instance(WEAPON_RACK, SHARED_HOLES, mesh, grid_axis=None, two_sided=True)
    if key == 'van':
        return make_holes_instance(VAN_MI, VAN_HOME + '/Materials', mesh, grid_axis=None, two_sided=True)
    if key == 'hull251':
        return make_holes_instance(HULL_251, SHARED_HOLES, mesh, grid_axis=None, two_sided=True)
    raise RuntimeError('unknown hole key ' + key)


def damaged_materials(key):
    """DamagedMaterials in slot order for the job's main mesh (None for slots without holes)."""
    job = JOBS[key]
    mesh = load(job['home'] + '/Meshes/' + job['mesh'])
    out = []
    for slot in slot_names(mesh):
        hole_key = job['holes'].get(slot)
        if hole_key is None:
            out.append(None)
            continue
        try:
            out.append(hole_material(hole_key, mesh))
        except Exception as error:   # an attribute master cannot be converted: leave that slot whole
            print('  HOLES SKIPPED', slot, error)
            out.append(None)
    print('HOLES', key, [m.get_name() if m else None for m in out])
    for m in out:
        if m is not None and m.get_path_name().startswith(SHARED_HOLES):
            parent = m.get_editor_property('parent')
            if parent is not None:
                print('  mask live', m.get_name(), check_mask_live(parent) if isinstance(parent, unreal.Material) else 'MI parent')
    return out


# ------------------------------------------------------------------------------------------------- blueprints

def bp_component(bp, name_prefix):
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        obj = SLIB.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), bp)
        if obj is not None and obj.get_name().split('_GEN_VARIABLE')[0] == name_prefix:
            return obj
    return None


def bp_component_of_class(bp, cls):
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        obj = SLIB.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), bp)
        if obj is not None and isinstance(obj, cls):
            return obj
    return None


def ensure_wall_hole_component(bp):
    existing = bp_component_of_class(bp, unreal.WallHoleDamageComponent)
    if existing is not None:
        return existing
    root = SDS.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.WallHoleDamageComponent,
                                          blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError('add WallHoleDamage failed: %s' % fail)
    SDS.rename_subobject(handle, unreal.Text('WallHoleDamage'))
    return SLIB.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), bp)


def configure_holes(component, materials, debris):
    """German-pit tuning (README: 0.85 mm radius per calibre, NoGrid, outset 0, 3600 cm2 per piece, 5 pieces)."""
    component.set_editor_property('WallMeshComponentName', 'BuildingMeshComponent')
    component.set_editor_property('DamagedMaterials', materials)
    component.set_editor_property('DebrisMeshes', debris)
    hole = unreal.WallChunkHoleSettings()
    hole.import_text('(MinCalibreMm=20,HoleRadiusPerCalibreMm=0.850000,HoleRadiusPerSqrtDamage=5.000000,'
                     'GuaranteedChunkRadius=45.000000,MaxHoleRadius=220.000000,MaxHiddenShareWhileAlive=0.650000)')
    component.set_editor_property('HoleSettings', hole)
    grid = unreal.WallHoleGridSettings()
    grid.import_text('(GridAxis=NoGrid,CellSize=150.000000,CellBreakRadiusShare=0.750000,BakeReachShare=0.350000,'
                     'GrowRadiusShare=0.700000,MinHoleRadius=12.000000,HoleCentreOutsetShare=0.000000,'
                     'AreaPerDebrisPiece=3600.000000,MaxPiecesPerHit=5,CollapsePieceSpacing=110.000000,'
                     'MaxCollapsePieces=120)')
    component.set_editor_property('GridSettings', grid)
    collapse = unreal.WallChunkCollapseSettings()
    collapse.import_text('(bCollapseIntoChunksOnDeath=False,DestroyOwnerDelaySeconds=4.500000,DeathSpeedScale=1.300000,'
                         'CollapseSound="/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue.Collapse_Cue",'
                         'CollapseAttenuation="/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse.Att_Collapse",'
                         'CollapseConcurrency="/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse.SC_Collapse")')
    component.set_editor_property('CollapseSettings', collapse)


def aim_points(key, mesh):
    """Aim points on holeable hull surfaces (mesh space = expansion space: the mesh sits at the actor origin)."""
    if key == 'RadioVan':
        # Van socket = roof centre; the box body is 4.7 m along X and 1.8 m across, its outer side at -Y.
        van = mesh.find_socket('Van').get_editor_property('relative_location')
        return [van, van + unreal.Vector(0, 0, 45), van + unreal.Vector(0, -90, -80), van + unreal.Vector(190, 0, -80),
                van + unreal.Vector(-190, 0, -80)]
    names = {'WurfrahmenShop': ('Roof', 'Rockets', 'Jig'),
             'SniperLoft': ('Aim_Parapet', 'Aim_Net', 'Aim_Trestle', 'Aim_Landing')}[key]
    return [mesh.find_socket(n).get_editor_property('relative_location') for n in names]


def place_helpers(bp, mesh):
    """Selection box, decal, progress bar and nav modifier over the mesh's own bounds (they come from the copied BP)."""
    box = mesh.get_bounding_box()
    centre = (box.min + box.max) * 0.5
    half = (box.max - box.min) * 0.5
    selection = bp_component(bp, 'bpSelectionArea')
    if selection:
        selection.set_editor_property('relative_location', unreal.Vector(centre.x, centre.y, min(half.z, 200.0)))
        selection.set_editor_property('box_extent', unreal.Vector(half.x, half.y, min(half.z, 200.0)))
    decal = bp_component(bp, 'SelectedDecal')
    if decal:
        decal.set_editor_property('relative_location', unreal.Vector(centre.x, centre.y, 0))
    bar = bp_component(bp, 'ProgressBarWidget')
    if bar:
        bar.set_editor_property('relative_location', unreal.Vector(centre.x, centre.y, box.max.z + 150))
    nav = bp_component(bp, 'NavModifier')
    if nav:
        nav.set_editor_property('failsafe_extent', unreal.Vector(half.x, half.y, half.z))
    return centre, half


def build_bp(key, turret_class=None):
    """Copy the base expansion, put the new meshes, holes, health notify levels and aim points on it.
    The graph pins (meshes, expansion type) are set afterwards through UECP set_pin_default."""
    job = JOBS[key]
    path = job['home'] + '/' + job['bp']
    if not EAL.does_asset_exist(path):
        EAL.duplicate_asset(job['base_bp'], path)
    bp = load(path)
    mesh = load(job['home'] + '/Meshes/' + job['mesh'])
    building = bp_component(bp, 'BuildingMeshComponent')
    building.set_editor_property('static_mesh', mesh)
    centre, half = place_helpers(bp, mesh)
    health = bp_component(bp, 'Health')
    # Empty: no 50 % mesh swap (procedural holes instead; a swap would reset the hole materials).
    health.set_editor_property('HealthLevelsToNotifyOn', [])
    holes = ensure_wall_hole_component(bp)
    debris = [load(job['home'] + '/Meshes/Debris/%s_Debris_%d' % (job['mesh'], i)) for i in range(job['debris'])]
    if not debris:
        debris = [load('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/Holes/'
                       'SM_GerPit_Debris_%d' % i) for i in range(8)]
    configure_holes(holes, damaged_materials(key), debris)
    if turret_class is not None:
        child = bp_component(bp, 'Howitzer')
        child.set_editor_property('child_actor_class', unreal.load_class(None, turret_class))
    cdo = unreal.get_default_object(bp.generated_class())
    bp.modify()
    cdo.set_editor_property('AimOffsetPoints', aim_points(key, mesh))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    print('BP', path, 'centre', centre, 'half', half, 'aim', len(aim_points(key, mesh)))
    report_bp(path)


def report_bp(path):
    bp = load(path)
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        obj = SLIB.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), bp)
        if obj is None:
            continue
        extra = ''
        for prop in ('static_mesh', 'child_actor_class', 'HealthLevelsToNotifyOn', 'DamagedMaterials'):
            try:
                value = obj.get_editor_property(prop)
            except Exception:
                continue
            if isinstance(value, unreal.Object):
                value = value.get_name()
            elif isinstance(value, unreal.Array):
                value = [v.get_name() if isinstance(v, unreal.Object) else str(v) for v in value]
            extra += ' %s=%s' % (prop, value)
        print('  comp', obj.get_name(), obj.get_class().get_name(), extra)
    cdo = unreal.get_default_object(bp.generated_class())
    print('  aim', [(round(v.x), round(v.y), round(v.z)) for v in cdo.get_editor_property('AimOffsetPoints')])


# ------------------------------------------------------------------------------------------------- sniper turret

SNIPER_TURRET = '/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets/German/Ger_EIT_Sniper_Kar98'


def build_sniper_turret():
    """German Kar98k sniper as an embedded infantry turret: the scavenger MP40 gunner copied, with the barracks'
    female sniper model and the Kar98k sniper rifle; limited to the loft's front half (the net poles are behind)."""
    if not EAL.does_asset_exist(SNIPER_TURRET):
        EAL.duplicate_asset('/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets/German/Ger_EIT_Scavenger_MP40',
                            SNIPER_TURRET)
    bp = load(SNIPER_TURRET)
    soldier = bp_component(bp, 'SoldierMesh')
    soldier.set_editor_property('skeletal_mesh_asset', load(
        '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/FemaleSniper/SK_GerFemaleSniper'))
    weapon = bp_component(bp, 'WeaponMesh')
    weapon.set_editor_property('static_mesh', load(
        '/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/Kar98/Mesh/SM_Ger_InfWeapon_KarSniper'))
    cdo = unreal.get_default_object(bp.generated_class())
    bp.modify()
    settings = cdo.get_editor_property('InfantryTurretSettings')
    settings.import_text('(RotationSpeed=90.000000,MaxPitch=45.000000,MinPitch=-35.000000,IdleRotationType=Idle_Animate,'
                         'bLimitTraverseArc=True,MinTraverseYaw=-90.000000,MaxTraverseYaw=90.000000)')
    cdo.set_editor_property('InfantryTurretSettings', settings)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    print('SNIPER', SNIPER_TURRET, unreal.get_default_object(bp.generated_class()).get_editor_property(
        'InfantryTurretSettings').export_text())
