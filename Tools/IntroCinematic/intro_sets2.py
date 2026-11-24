"""More sets of the intro cinematic (see intro_sets.py for the helpers and the bunker):
drafting room (2.2), space (3.2), Berlin skyline (3.3/3.4), night clouds (5.1), trench (5.2/5.3), hall (7),
and the sky overlay that Ger_Berlin needs (its own sky is never seen in the game's top-down view).
"""
import importlib
import random

import unreal

import intro_lib as lib
import intro_materials as mats
import intro_sets as base

importlib.reload(base)

Set = base.Set
M = base.M
actors_api = base.actors_api
CUBE, PLANE, SPHERE, CYLINDER = base.CUBE, base.PLANE, base.SPHERE, base.CYLINDER
CONE = "/Engine/BasicShapes/Cone"
_registry = unreal.AssetRegistryHelpers.get_asset_registry()


def find_assets(folder, contains="", class_name="StaticMesh"):
    """Package paths of assets in a folder whose name contains a string."""
    found = []
    for data in _registry.get_assets_by_path(folder, True):
        if str(data.asset_class_path.asset_name) == class_name and contains in str(data.asset_name):
            found.append(str(data.package_name))
    return sorted(found)


def _spawn(cls, s, local, rotation=(0.0, 0.0, 0.0), label=None):
    actor = actors_api.spawn_actor_from_class(cls, s.at(local), unreal.Rotator(rotation[2], rotation[0], rotation[1]))
    if label:
        actor.set_actor_label(label)
    return actor


def sun(s, label, pitch, yaw, intensity, color=(1.0, 0.95, 0.88, 1.0)):
    actor = _spawn(unreal.DirectionalLight, s, (0, 0, 2000), (pitch, yaw, 0), label)
    component = actor.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_intensity(intensity)
    component.set_light_color(unreal.LinearColor(*color))
    # Screen-space contact shadows: these sets stand far from the world origin, where the shadow bias alone left
    # a lit strip under the tanks at the Ural gate (they looked as if they hovered).
    component.set_editor_property("contact_shadow_length", 0.06)
    return actor


def sky_light(s, label, intensity, color=(0.5, 0.6, 0.8, 1.0)):
    actor = _spawn(unreal.SkyLight, s, (0, 0, 3000), label=label)
    component = actor.get_component_by_class(unreal.SkyLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("real_time_capture", False)
    component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    component.set_editor_property("cubemap", unreal.load_asset("/Engine/EngineResources/GrayLightTextureCube"))
    component.set_intensity(intensity)
    component.set_light_color(unreal.LinearColor(*color))
    return actor


def fog(s, label, density, color, height_falloff=0.02, volumetric=False, local=(0, 0, 0)):
    actor = _spawn(unreal.ExponentialHeightFog, s, local, label=label)
    component = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
    component.set_fog_density(density)
    component.set_fog_height_falloff(height_falloff)
    component.set_fog_inscattering_color(unreal.LinearColor(*color))
    component.set_volumetric_fog(volumetric)
    return actor


def spot(s, label, local, rotation, intensity, color, cone=30.0, radius=3000.0, shadows=True):
    actor = _spawn(unreal.SpotLight, s, local, rotation, label)
    component = actor.get_component_by_class(unreal.SpotLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_intensity_units(unreal.LightUnits.CANDELAS)
    component.set_intensity(intensity)
    component.set_light_color(unreal.LinearColor(*color))
    component.set_outer_cone_angle(cone)
    component.set_inner_cone_angle(cone * 0.5)
    component.set_attenuation_radius(radius)
    component.set_cast_shadows(shadows)
    return actor


def skeletal(s, label, mesh_path, anim_path, local, yaw=0.0):
    actor = _spawn(unreal.SkeletalMeshActor, s, local, (0, yaw, 0), label)
    component = actor.skeletal_mesh_component
    component.set_skinned_asset_and_update(unreal.load_asset(mesh_path))
    animation = unreal.load_asset(anim_path) if anim_path else None
    if animation:
        component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        component.override_animation_data(animation, True, True, 0.0, 1.0)
    return actor


SOLDIER_RUS = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier"
SOLDIER_GER = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/GermanSoldier/Meshes/SK_WWII_GermanSoldier_varA"
SOLDIER_SCAV = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Mesh/SK_PostApoSoldier"
ANIMS = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/"
V2 = "/Game/RTS_Survival/Meshes/Weapons/V2/SM_V2Rocket"
V2_GREEN = "/Game/RTS_Survival/Cinematics/V2Scene/V2materials/MI_V2_RussianGreen"


def fixed_exposure(s, label, brightness):
    """Locks the eye adaptation of a set (unbound volume above the film's grade volume)."""
    volume = _spawn(unreal.PostProcessVolume, s, (0, 0, 0), label=label)
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 2.0)
    settings = volume.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", brightness)
    settings.set_editor_property("auto_exposure_max_brightness", brightness)
    volume.set_editor_property("settings", settings)


def build_draft():
    """Shot 2.2: a drafting room at night; the blueprint on the board, a figure bent over it. The torch is the
    shot's own light (a spawnable), so the room itself is nearly dark."""
    s = Set("Intro_Draft")
    stone = M + "MI_Intro_DarkStone"
    s.box("Draft_Floor", (0, 0, -10), (900, 900, 20), stone)
    s.box("Draft_WallBack", (320, 0, 150), (20, 900, 320), stone)
    s.box("Draft_WallSide", (0, 330, 150), (900, 20, 320), stone)
    s.box("Draft_Desk", (200, 0, 40), (130, 230, 80), M + "MI_Intro_DarkWood")
    # Tilted drafting board with the cutaway drawing.
    s.box("Draft_Board", (215, 0, 96), (112, 170, 4), M + "MI_Intro_DarkWood", rotation=(-18, 0, 0))
    # Lies on the board: the board tilts front-to-back (pitch -18), so with its long side along Y the sheet takes that
    # slope as a roll (pitch -18 with yaw 90 tilted it sideways and half of it sank into the board); centred 3.5 mm
    # above the board's top face (board centre + up * (2 cm half thickness + 0.35 cm)).
    s.mesh("Draft_Blueprint", PLANE, (215.73, 0, 98.23), rotation=(0, 90, -18), scale=(1.56, 1.04, 1.0),
           materials=M + "MI_Intro_Blueprint", shadow=False)
    s.mesh("Draft_Typewriter", "/Game/UVB80/Assets/UVB80/Items/SM_Typewriter", (180, 150, 80.5), rotation=(0, 200, 0))
    s.mesh("Draft_Lamp", "/Game/UVB80/Assets/UVB80/Items/SM_TableLamp", (240, -140, 80.5), rotation=(0, 30, 0))
    skeletal(s, "Draft_Spy", SOLDIER_RUS, ANIMS + "TeamWeaponAnims/Pak/AN_Infantry_SpotPAK_Crouched", (158, -112, 0), yaw=62)
    s.point_light("Draft_Moon", (-200, -250, 260), 0.06, (0.45, 0.55, 0.9, 1.0), 1500.0, shadows=False)
    base.finish_level("Intro_Draft")


def build_space():
    """Shot 3.2: high altitude; the curve of the earth below, black sky, hard sun."""
    s = Set("Intro_Space")
    radius = 600000.0   # a 6 km ball reads as a planet from a few hundred metres above it
    # The planet is drawn by the sky dome's material (ray against a sphere): no facets on the limb.
    centre = s.at((0, 0, -radius - 30000))
    mats.instance("MI_Intro_EarthView", unreal.load_asset(M + "M_Intro_EarthView"),
                  vectors={"Centre": (centre.x, centre.y, centre.z, 0.0)}, scalars={"Radius": radius})
    s.mesh("Space_Sky", SPHERE, (0, 0, 0), scale=(40000.0,) * 3, materials=M + "MI_Intro_EarthView", shadow=False)
    sun(s, "Space_Sun", -18, -40, 3.0)
    base.finish_level("Intro_Space")


def build_skyline():
    """Shots 3.3 / 3.4a: Berlin standing, January 1945, in 3D from the Berlin mission kits (intro_berlin)."""
    import intro_berlin
    importlib.reload(intro_berlin)
    intro_berlin.build_level(base, importlib.import_module("intro_sets2"), ruined=False)


def build_skyline_ruin():
    """Shot 3.4b: the same plots after the strike; the near shells fall in the shot itself (intro_berlin)."""
    import intro_berlin
    importlib.reload(intro_berlin)
    intro_berlin.build_level(base, importlib.import_module("intro_sets2"), ruined=True)


def build_clouds():
    """Shot 5.1: night, high cloud lit purple from inside. Volumetric fog with purple lights buried in it."""
    s = Set("Intro_Clouds")
    s.mesh("Cloud_Sky", SPHERE, (0, 0, 0), scale=(3000.0,) * 3, materials=M + "MI_Intro_Sky_Night", shadow=False)
    # Four sheets of cloud at different depths in front of the camera (it looks along +X): the far ones glow from
    # inside, the near one is dark wisps that slide past faster.
    for index, (distance, size, drop) in enumerate(((16000, 520, -1800), (10500, 360, -900), (6000, 230, 300), (2600, 120, 900))):
        s.mesh("Cloud_Sheet_%d" % index, PLANE, (distance, 0, drop), rotation=(0, 90, 90), scale=(size, size * 0.6, 1.0),
               materials=M + "MI_Intro_Cloud_%d" % index, shadow=False)
    fixed_exposure(s, "Cloud_Exposure", 1.1)
    base.finish_level("Intro_Clouds")


KIT = lib.ROOT + "/Sets/"
# Mould meshes of the trench kit: (mesh, offset from the trench pivot in Unreal axes). Their pivot is where they grow from.
TRENCH_MOULD = (
    ("SM_Intro_Mould_Mat_A", (-91.0, -440.0, 32.3)), ("SM_Intro_Mould_Mat_B", (102.0, -120.0, 44.0)),
    ("SM_Intro_Mould_Mat_C", (69.3, -760.0, 7.0)), ("SM_Intro_Mould_Veins", (-60.0, -362.0, 1.3)),
    ("SM_Intro_Mould_Pustules_A", (68.5, -1117.5, 7.2)), ("SM_Intro_Mould_Pustules_B", (-74.5, -710.0, 1.2)))


def build_trench():
    """Shots 5.2 / 5.3: a 1945 trench at dawn (Blender kit: earth, revetment, duckboards, sandbags, props), the dead.
    Floor at z = 0, ground at +190. The purple growth is added by the shots (it grows), not here."""
    s = Set("Intro_Trench")
    s.mesh("Trench_Earth", KIT + "SM_Intro_Trench", (0, 0, 0))
    s.mesh("Trench_Props", KIT + "SM_Intro_TrenchProps", (0, 0, 0))
    poses = find_assets("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/CorpsePoses/Poses", "GerSoldierVarA")
    print("corpse poses", len(poses))
    for k, (x, y, yaw) in enumerate(((24, -330, 75), (-26, -640, 200), (30, -930, 120))):
        if poses:
            # the dead are caked in the Purple Death (MI_Intro_PD_Encrusted): the game's soldier material, and then a
            # matte field-grey, read as grey clay under this set's fixed exposure
            s.mesh("Trench_Body_%d" % k, poses[(k * 5) % len(poses)], (x, y, 9), rotation=(0, yaw, 0),
                   materials=M + "MI_Intro_PD_Encrusted")
    # Low sun from behind the camera side, raking down the trench; the far end stays in shade.
    trench_sun = sun(s, "Trench_Sun", -19, -62, 2.4, (1.0, 0.7, 0.44, 1.0))
    # a wider sun disc: the growing mould's shadow on the trench floor had a ruler-straight edge (5.3)
    trench_sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("light_source_angle", 2.5)
    sky_light(s, "Trench_Sky", 0.22, (0.45, 0.52, 0.72, 1.0))
    fixed_exposure(s, "Trench_Exposure", 0.13)
    s.mesh("Trench_SkyDome", SPHERE, (0, 0, 0), scale=(900.0,) * 3, materials=M + "MI_Intro_Sky_Ash", shadow=False)
    fog(s, "Trench_Fog", 0.012, (0.2, 0.18, 0.16, 1.0), 0.1)
    base.finish_level("Intro_Trench")


def build_crater():
    """Shots 8.1 - 8.3: a strike crater at night (Blender terrain with real cracks). Floor at z = 0."""
    s = Set("Intro_Crater")
    s.mesh("Crater_Terrain", KIT + "SM_Intro_CraterTerrain", (0, 0, 0))
    sun(s, "Crater_Moon", -26, 215, 1.1, (0.62, 0.8, 0.86, 1.0))
    sky_light(s, "Crater_Sky", 0.3, (0.34, 0.5, 0.56, 1.0))
    s.mesh("Crater_SkyDome", SPHERE, (0, 0, 0), scale=(3000.0,) * 3, materials=M + "MI_Intro_Sky_Night", shadow=False)
    fog(s, "Crater_Fog", 0.002, (0.012, 0.015, 0.03, 1.0), 0.05)
    # Fixed exposure: at night the eye adaptation would lift the dark floor to grey and burn the glow to white.
    volume = _spawn(unreal.PostProcessVolume, s, (0, 0, 0), label="Crater_Exposure")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 2.0)
    settings = volume.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 0.35)
    settings.set_editor_property("auto_exposure_max_brightness", 0.35)
    volume.set_editor_property("settings", settings)
    base.finish_level("Intro_Crater")


def build_hall():
    """Section 7: a dim hall behind the wall. The banners and the old portrait frame are moved by the shots."""
    s = Set("Intro_Hall")
    stone = M + "MI_Intro_DarkStone"
    s.box("Hall_Floor", (0, 0, -20), (3600, 2600, 40), stone)
    s.box("Hall_Back", (1500, 0, 800), (60, 2600, 1700), stone)
    s.box("Hall_Left", (0, -1300, 800), (3600, 60, 1700), stone)
    s.box("Hall_Right", (0, 1300, 800), (3600, 60, 1700), stone)
    s.box("Hall_Ceiling", (0, 0, 1650), (3600, 2600, 60), stone)
    for k in range(4):
        for side in (-1, 1):
            s.box("Hall_Pillar_%d_%d" % (k, side), (-900 + k * 650, side * 900, 800), (110, 110, 1600), stone)
    s.box("Hall_Dais", (1300, 0, 30), (360, 1700, 60), stone)
    s.point_light("Hall_Fill", (-300, 0, 900), 2.0, (0.9, 0.75, 0.6, 1.0), 5000.0, source_radius=80.0, shadows=False)
    # Depth: a lamp on every pillar (pools of light down the nave), steps to the dais, beams under the ceiling.
    for k in range(4):
        for side in (-1, 1):
            s.box("Hall_Sconce_%d_%d" % (k, side), (-900 + k * 650, side * 838, 520), (10, 10, 22), M + "MI_Intro_Flame", shadow=False)
            s.point_light("Hall_Lamp_%d_%d" % (k, side), (-900 + k * 650, side * 790, 520), 9.0, (1.0, 0.72, 0.42, 1.0), 900.0,
                          source_radius=12.0, shadows=True)
        s.box("Hall_Beam_%d" % k, (-900 + k * 650, 0, 1570), (90, 2600, 110), stone)
    for k in range(3):
        s.box("Hall_Step_%d" % k, (1090 - k * 70, 0, 45 - k * 15), (70, 1900, 30), stone)
    base.finish_level("Intro_Hall")


def build_berlin_sky():
    """Overlay shown with Ger_Berlin: the map has no sky of its own (the game looks straight down)."""
    s = Set("Intro_BerlinSky")
    s.mesh("Berlin_SkyDome", SPHERE, (0, 0, 0), scale=(2600.0,) * 3, materials=M + "MI_Intro_Sky_Ash", shadow=False)
    fog(s, "Berlin_Fog", 0.022, (0.42, 0.40, 0.37, 1.0), 0.08)
    base.finish_level("Intro_BerlinSky")


def build_ural_greybox():
    """Greybox of the Ural Shield on Ger_2ndOreMountains (replaced by the modelled kit): a wall of boxes along Y
    with a gate gap at y = 0, and a huge ground sheet so the land runs on past the map edge."""
    import intro_scout as scout
    scout.set_levels_visible(["Ger_2ndOreMountains"])
    s = Set("Intro_Ural")
    ground = scout.ground_z(base.ORIGINS["Intro_Ural"][0] - 1000.0, 0.0) or 4000.0
    print("ural ground", ground)
    concrete = M + "MI_Intro_Concrete"
    height, thick, length = 4600.0, 1400.0, 6000.0
    for k in range(-40, 41):
        if k == 0:
            for side in (-1, 1):
                s.box("Ural_GatePier_%d" % side, (0, side * 1900, ground + height / 2 - 300), (thick * 1.3, 2200, height + 900), concrete)
            s.box("Ural_GateLintel", (0, 0, ground + 1300 + (height - 1300) / 2), (thick, 1600, height - 1300), concrete)
            continue
        s.box("Ural_Seg_%d" % k, (0, k * length, ground + height / 2 - 300), (thick, length, height), concrete)
        s.box("Ural_Buttress_%d" % k, (-thick * 0.7, k * length, ground + 1400), (700, 900, 3400), concrete)
    s.box("Ural_Iron", (-thick / 2 - 12, 0, ground + height - 900), (24, 80 * length, 900), M + "MI_Intro_Steel")
    s.box("Ural_FarGround", (0, 0, ground - 900), (2400000, 2400000, 100), M + "MI_Intro_Dirt")
    base.finish_level("Intro_Ural")
    scout.set_levels_visible([])


# Behind the Ural Shield (+X, east): the Soviet hinterland, owner 2026-11-21. Tall works (stacks, cooling towers,
# refinery) stand near the wall where they rise over its 45 m parapet in 6.4; housing blocks lie further back, where
# the climbing camera sees over the wall. In front (-X, west): a cratered no man's land of dead trees. Both are
# seeded plans (the shots read them too: smoke on the stacks). Clear: the 6.3 road and camera (|y| < 2600 near the gate).
_IZ = "/Game/RTS_Survival/Meshes/Industrial/Zone/Meshes/"
_RUSB = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/"
# (mesh, stack top as (x, y, z) in the mesh's own frame or None, scale range)
URAL_WORKS = (
    (_IZ + "MegaFactoryPipeVerticals/Atlassed_SM_Mega_factory_01", (-194.0, 318.0, 13300.0), (1.0, 1.25)),
    (_IZ + "MegaFactoryPipeVerticals/Atlassed_SM_Mega_factory_04", (59.0, 3.0, 8600.0), (1.0, 1.2)),
    (_IZ + "MegaFactoryPipeVerticals/Atlassed_SM_Mega_factory_02", None, (1.0, 1.3)),
    (_IZ + "Cooling_tower/Atlassed_SM_Cooling_tower_01", (0.0, 0.0, 4900.0), (1.1, 1.5)),
    ("/Game/RTS_Survival/Meshes/Buildings/Industrial/SM_IndustrialRefinery", None, (1.0, 1.2)),
    (_IZ + "Warehouses/Atlassed_SM_Warehouse_01", None, (1.0, 1.0)),
    (_RUSB + "FactoryMeshes/SM_Rus_WarFactory", None, (1.6, 2.0)),
    (_RUSB + "CoolingTowersMeshes/SM_Rus_IndustrialCooling", None, (1.8, 2.4)),
)
URAL_HOUSING = (
    ("/Game/ApocalypticCity/StaticMeshs/Classic_house_01_sm", (0.9, 1.1)),
    ("/Game/SM_CityHouse_Grey_1", (1.3, 1.6)),
    ("/Game/SM_CityHouse_Yellow_1", (1.3, 1.6)),
    ("/Game/SM_CityHouse_Blue_1", (1.3, 1.6)),
)
URAL_TREES = tuple("/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/SM_RTS_DeadTree_%s" % n for n in ("1", "2", "04", "05", "06"))
URAL_STUMP = "/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/SM_RTS_DeadTree_Stump"
URAL_CRATER = "/Game/RTS_Survival/Meshes/ground/Craters/SM_Crater_1_TerrainNode_001"


def ural_hinterland():
    """[(mesh, x, y, yaw, scale, stack top world point or None)] behind the wall, set-local cm."""
    import math
    rng = random.Random(1946)
    plan = []
    y = -36000.0
    while y < 420000.0:
        mesh, stack, (lo, hi) = URAL_WORKS[rng.randrange(len(URAL_WORKS))]
        x = rng.uniform(9000.0, 26000.0) + max(0.0, y) * 0.04
        yaw = rng.choice((0.0, 90.0, 180.0, 270.0)) + rng.uniform(-4.0, 4.0)
        scale = rng.uniform(lo, hi)
        top = None
        if stack:
            c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            top = (x + (stack[0] * c - stack[1] * s_) * scale, y + (stack[0] * s_ + stack[1] * c) * scale, stack[2] * scale)
        plan.append((mesh, x, y, yaw, scale, top))
        y += rng.uniform(5500.0, 11000.0) * (1.0 + max(0.0, y) / 300000.0)
    # housing: rows of blocks along streets parallel to the wall, further back
    for row in range(6):
        x_row = 34000.0 + row * 7000.0
        y = -30000.0 + rng.uniform(0.0, 3000.0)
        while y < 300000.0:
            mesh, (lo, hi) = URAL_HOUSING[rng.randrange(len(URAL_HOUSING))]
            plan.append((mesh, x_row + rng.uniform(-600.0, 600.0), y, rng.choice((0.0, 180.0)), rng.uniform(lo, hi), None))
            y += rng.uniform(3400.0, 6200.0) * (1.0 + max(0.0, y) / 200000.0)
    return plan


def ural_front():
    """[(kind, mesh, x, y, yaw, scale)] in front of the wall: craters (flattened, bowl floor on the ground) and dead
    trees in groves and alone; nothing on the 6.3 road / camera corridor."""
    rng = random.Random(1947)
    plan = []

    def clear(x, y):
        return x > -16000.0 and abs(y) < 2600.0

    for _ in range(360):
        x, y = -rng.uniform(1600.0, 70000.0), rng.uniform(-70000.0, 320000.0)
        if clear(x, y):
            continue
        plan.append(("crater", URAL_CRATER, x, y, rng.uniform(0.0, 360.0), rng.uniform(1.3, 3.4)))
    groves = [(-rng.uniform(3000.0, 60000.0), rng.uniform(-60000.0, 300000.0)) for _ in range(34)]
    for gx, gy in groves:
        for _ in range(rng.randrange(4, 12)):
            x, y = gx + rng.gauss(0.0, 2200.0), gy + rng.gauss(0.0, 3200.0)
            if x > -1500.0 or clear(x, y):
                continue
            mesh = URAL_STUMP if rng.random() < 0.25 else URAL_TREES[rng.randrange(len(URAL_TREES))]
            plan.append(("tree", mesh, x, y, rng.uniform(0.0, 360.0), rng.uniform(1.8, 2.8)))
    for _ in range(140):
        x, y = -rng.uniform(1800.0, 60000.0), rng.uniform(-60000.0, 300000.0)
        if clear(x, y):
            continue
        plan.append(("tree", URAL_TREES[rng.randrange(len(URAL_TREES))], x, y, rng.uniform(0.0, 360.0), rng.uniform(1.8, 3.0)))
    return plan


def ural_smoke(at):
    """Shot spawnables: smoke from the hinterland's stacks (NS_SmokePillar1 rendered in 3.3)."""
    return [{"kind": "niagara", "asset": "NS_SmokePillar1", "label": "UralStack_%d" % index,
             "keys": [(0.0, at("Intro_Ural", *top), (0, 0, 0), 6.0)]}
            for index, top in enumerate(t for *_, t in ural_hinterland() if t is not None) if index < 24]


def build_ural():
    """The Ural Shield (6.3 / 6.4) from the modelled kit, on its own plain: gate frame at the set origin, hero
    segments beside it, cheap far pieces (stretched along the wall with distance) to about 40 km each way, hills
    behind and beside it, overcast sky and fog so it fades into both horizons."""
    s = Set("Intro_Ural")
    kit = lib.ROOT + "/UralShield/"
    dirt = M + "MI_Intro_Ground_Ural"
    s.box("Ural_Ground", (0, 0, -50), (16000000, 16000000, 100), dirt)
    # Behind the gate: the yard and its sheds, so the open leaves do not show an empty sky.
    stone = M + "MI_Intro_DarkStone"
    for k, (x, y, sx, sy, sz) in enumerate(((7000, -1800, 2600, 5200, 1500), (9000, 3600, 3200, 4200, 2100), (15000, 0, 5000, 16000, 1200))):
        s.box("Ural_Yard_%d" % k, (x, y, sz / 2.0 - 40), (sx, sy, sz), M + "MI_Intro_Silhouette")
    s.mesh("Ural_GateFrame", kit + "SM_UralShield_GateFrame", (0, 0, -40))
    for side in (1, -1):
        y = 6000.0
        index = 1
        while y < 4000000.0:
            stretch = 1.0 if index <= 16 else (4.0 if index <= 34 else 12.0)
            length = 6000.0 * stretch
            centre = y + length / 2.0 - 3000.0
            mesh = "SM_UralShield_Segment" if index <= 5 else "SM_UralShield_Far"
            s.mesh("Ural_%s_%03d" % ("N" if side > 0 else "S", index), kit + mesh, (0, side * centre, -40),
                   scale=(1.0, stretch, 1.0), shadow=index <= 16)
            y += length
            index += 1
    # Low hills the wall runs through and mountains behind it (the east), all the same dry earth.
    rng = random.Random(1947)
    for index in range(46):
        behind = index % 3 != 0
        x = rng.uniform(60000, 520000) if behind else rng.uniform(-420000, -90000)
        y = rng.uniform(-1500000, 1500000)
        radius = rng.uniform(60000, 190000) if behind else rng.uniform(30000, 80000)
        height = radius * rng.uniform(0.16, 0.34)
        s.mesh("Ural_Hill_%02d" % index, SPHERE, (x, y, -height * 0.35), scale=(radius / 50.0, radius * rng.uniform(1.0, 2.2) / 50.0, height / 50.0),
               materials=dirt, shadow=False)
    for index, (mesh, x, y, yaw, scale, _top) in enumerate(ural_hinterland()):
        s.mesh("Ural_Works_%03d" % index, mesh, (x, y, 0), rotation=(0, yaw, 0), scale=(scale,) * 3, shadow=x < 40000.0)
    for index, (kind, mesh, x, y, yaw, scale) in enumerate(ural_front()):
        if kind == "crater":
            # flattened so its bowl floor sits on the ground (the ground box would cover a sunken bowl): a low rim
            # (the mesh's own landscape-style material drew pale discs: scorched earth instead)
            s.mesh("Ural_Crater_%03d" % index, mesh, (x, y, 310.0 * scale * 0.45), rotation=(0, yaw, 0),
                   scale=(scale, scale, scale * 0.45), shadow=False, materials=M + "MI_Intro_Ground_Crater")
        else:
            s.mesh("Ural_DeadTree_%03d" % index, mesh, (x, y, -10.0), rotation=(0, yaw, 0), scale=(scale,) * 3,
                   shadow=x > -20000.0)
    sun(s, "Ural_Sun", -24, 18, 3.6, (1.0, 0.9, 0.78, 1.0))
    sky_light(s, "Ural_Sky", 0.9, (0.62, 0.66, 0.76, 1.0))
    s.mesh("Ural_SkyDome", SPHERE, (0, 0, 0), scale=(150000.0,) * 3, materials=M + "MI_Intro_Sky_Ash", shadow=False)
    fog(s, "Ural_Fog", 0.0022, (0.55, 0.52, 0.47, 1.0), 0.008)
    base.finish_level("Intro_Ural")


def build_all():
    build_draft()
    build_space()
    build_skyline()
    build_skyline_ruin()
    build_clouds()
    build_trench()
    build_crater()
    build_hall()
    build_berlin_sky()
