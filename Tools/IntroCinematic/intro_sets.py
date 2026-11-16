"""Sets made for the 1976 intro cinematic. Each set is its own small sublevel under
/Game/RTS_Survival/Maps/Cinematics/Intro1976/, far away from the mission maps, switched on per shot by a Level
Visibility track. Every builder clears and rebuilds its level, so it can be re-run.

Run inside the throwaway editor with the cinematic map open.
"""
import importlib
import math

import unreal

import intro_fire as fire
import intro_lib as lib
import intro_materials as mats

importlib.reload(lib)
importlib.reload(fire)

actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
CUBE = "/Engine/BasicShapes/Cube"
PLANE = "/Engine/BasicShapes/Plane"
CYLINDER = "/Engine/BasicShapes/Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere"
M = mats.MAT_DIR + "/"

# Where each set stands in the world (cm). Far from the mission landscapes (they span about +-60,000).
ORIGINS = {
    "Intro_Bunker": (600000.0, 0.0, 0.0),
    "Intro_Draft": (620000.0, 0.0, 0.0),
    "Intro_Space": (640000.0, 0.0, 200000.0),
    "Intro_Skyline": (680000.0, 0.0, 0.0),
    "Intro_SkylineRuin": (680000.0, 0.0, 0.0),
    "Intro_Crater": (840000.0, 0.0, 0.0),            # a strike crater at night (section 8)      # the same place after the strike
    "Intro_Clouds": (720000.0, 0.0, 100000.0),
    "Intro_Trench": (760000.0, 0.0, 0.0),
    "Intro_Hall": (800000.0, 0.0, 0.0),
    # Overlays that stand on a mission map (shown together with it).
    "Intro_BerlinSky": (0.0, 0.0, 0.0),
    # The Ural Shield stands on a plain of its own: Ger_2ndOreMountains is 720 m across and terraced, far too
    # small and steep for a wall of 60 m modules that has to run to both horizons.
    "Intro_Ural": (0.0, 1000000.0, 0.0),
    "Intro_DesertSet": (0.0, 0.0, 0.0),
}

# --- Map sheet -------------------------------------------------------------------------------------------------------
# EarthWorldMap country meshes: one shared frame, 1 unit = 1 km, X = Mercator north, Y = east (lon/180 * 20038).
MAP_BOUNDS = (2600.0, 11800.0, -13900.0, 8900.0)   # x min, x max, y min, y max (km)
MAP_SCALE = 0.0149                                  # cm per km: the sheet is about 340 x 137 cm
TABLE_TOP_Z = 95.0
SHEET_Z = TABLE_TOP_Z + 0.4
MAP_LIFT = 0.5                                      # countries above the sea sheet
SHEET_CENTRE_OFFSET_X = 38.0                        # sheet pushed to the far side of the table (props in front)
EARTH_RADIUS = 6371.0


def map_km(lat, lon):
    x = EARTH_RADIUS * math.log(math.tan(math.pi / 4.0 + math.radians(lat) / 2.0))
    y = lon / 180.0 * 20038.0
    return x, y


def map_local(lat, lon):
    """Place on the table, relative to the set origin (cm): +X = north (away from the officer), +Y = east."""
    x, y = map_km(lat, lon)
    centre_x = (MAP_BOUNDS[0] + MAP_BOUNDS[1]) / 2.0
    centre_y = (MAP_BOUNDS[2] + MAP_BOUNDS[3]) / 2.0
    return ((x - centre_x) * MAP_SCALE + SHEET_CENTRE_OFFSET_X, (y - centre_y) * MAP_SCALE)


def map_uv(lat, lon):
    """Sheet UV of a place: u east, v north (for the stain material)."""
    x, y = map_km(lat, lon)
    return ((y - MAP_BOUNDS[2]) / (MAP_BOUNDS[3] - MAP_BOUNDS[2]), (x - MAP_BOUNDS[0]) / (MAP_BOUNDS[1] - MAP_BOUNDS[0]))


# Places (lat, lon).
PLACES = {
    "Berlin": (52.5, 13.4), "Moscow": (55.75, 37.6), "London": (51.5, -0.1),
    "LosAlamos": (35.9, -106.3), "OakRidge": (36.0, -84.3), "Hanford": (46.6, -119.5),
    "Peenemunde": (54.1, 13.8),
}
# Shot 3.6: German cities that follow Berlin (pins without labels).
GERMAN_CITIES = [(53.55, 10.0), (48.14, 11.58), (50.94, 6.96), (51.05, 13.74), (50.11, 8.68), (48.78, 9.18),
                 (52.37, 9.73), (49.45, 11.08)]
# Shot 6.2b: strikes scattered over Soviet-held Europe. Deliberately irregular: no line, no arc.
SVAROG_STRIKES = [(52.2, 21.0), (50.45, 30.5), (47.5, 19.05), (44.4, 26.1), (56.95, 24.1), (49.2, 16.6),
                  (53.9, 27.55), (46.5, 30.7), (54.7, 20.5), (48.2, 16.4), (51.75, 19.45), (45.8, 15.97),
                  (58.4, 26.7), (49.8, 24.0), (55.2, 30.2), (43.2, 23.3),
                  # No belt, no front line: strikes west, north, south and east of the rest, all on land the Soviets
                  # held (Germany and eastward; nothing on Britain, France, Scandinavia or Turkey).
                  (48.9, 8.4), (50.1, 8.7), (59.9, 30.3), (46.0, 14.5), (55.3, 39.0), (48.1, 11.6), (54.3, 10.1)]
# Shot 5.4: the stain starts in many places at once, west to east, so no start point reads as its target.
STAIN_SEEDS = [(50.8, 4.4), (48.9, 2.3), (52.5, 13.4), (45.5, 9.2), (50.1, 19.9), (59.3, 18.1), (44.8, 20.5),
               (53.9, 27.5), (41.0, 29.0)]


# The ammunition prop on the map table (local cm, yaw): past the mug on the officer's edge, clear of the date line
# and inside the 1.1 push-in.
AMMO_AT = (-104.0, 22.0, TABLE_TOP_Z + 0.1)
AMMO_YAW = 95.0


# --- Level and actor helpers -----------------------------------------------------------------------------------------
def set_level(name):
    """Makes the set's sublevel (created on first use) the current level and returns its streaming level."""
    world = lib.editor_world()
    streaming = unreal.GameplayStatics.get_streaming_level(world, name)
    if streaming is None:
        streaming = unreal.EditorLevelUtils.create_new_streaming_level(
            unreal.LevelStreamingDynamic, lib.SUBMAP_ROOT + "/" + name, False)
    streaming.set_editor_property("initially_loaded", True)
    streaming.set_editor_property("initially_visible", False)
    les.set_current_level_by_name(name)
    current = les.get_current_level().get_outermost().get_name()
    if current != lib.SUBMAP_ROOT + "/" + name:
        raise RuntimeError("current level is %s, wanted %s" % (current, name))
    return streaming


def clear_level(name):
    package = lib.SUBMAP_ROOT + "/" + name + "."
    count = 0
    for actor in actors_api.get_all_level_actors():
        if actor.get_path_name().startswith(package) and not isinstance(
                actor, (unreal.WorldSettings, unreal.Brush, unreal.NavigationData)):
            actors_api.destroy_actor(actor)
            count += 1
    return count


def finish_level(name):
    lib.make_persistent_current()
    lib.safe_save([unreal.find_package(lib.SUBMAP_ROOT + "/" + name)])
    lib.save_map()


class Set(object):
    """Spawns actors of one set relative to its origin."""

    def __init__(self, name):
        self.name = name
        self.origin = unreal.Vector(*ORIGINS[name])
        set_level(name)
        print(name, "cleared", clear_level(name))

    def at(self, local):
        return unreal.Vector(self.origin.x + local[0], self.origin.y + local[1], self.origin.z + local[2])

    def mesh(self, label, mesh_path, local, rotation=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0), materials=None,
             shadow=True, mobile=False):
        """rotation = (pitch, yaw, roll). materials: one path for every slot, or a list per slot."""
        actor = actors_api.spawn_actor_from_class(unreal.StaticMeshActor, self.at(local),
                                                  unreal.Rotator(rotation[2], rotation[0], rotation[1]))
        actor.set_actor_label(label)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        component = actor.static_mesh_component
        mesh = unreal.load_asset(mesh_path)
        if mesh is None:
            raise RuntimeError("mesh missing: " + mesh_path)
        component.set_static_mesh(mesh)
        if mobile:
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
        if materials is not None:
            if isinstance(materials, str):
                materials = [materials] * max(1, component.get_num_materials())
            for index, path in enumerate(materials):
                if path:
                    component.set_material(index, unreal.load_asset(path))
        component.set_cast_shadow(shadow)
        component.set_editor_property("can_ever_affect_navigation", False)
        return actor

    def box(self, label, local_centre, size, material, **kwargs):
        """A box of size (cm) centred at local_centre."""
        return self.mesh(label, CUBE, local_centre, scale=(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0),
                         materials=material, **kwargs)

    def plane(self, label, local_centre, size, material, yaw=0.0, **kwargs):
        """A flat plane facing up, size (x, y) in cm."""
        return self.mesh(label, PLANE, local_centre, rotation=(0.0, yaw, 0.0),
                         scale=(size[0] / 100.0, size[1] / 100.0, 1.0), materials=material, shadow=False, **kwargs)

    def rod(self, label, start, end, thickness, material):
        """A thin box from start to end (local cm): string, pencil lines."""
        dx, dy, dz = end[0] - start[0], end[1] - start[1], end[2] - start[2]
        length = math.sqrt(dx * dx + dy * dy + dz * dz)
        yaw = math.degrees(math.atan2(dy, dx))
        pitch = math.degrees(math.atan2(dz, math.sqrt(dx * dx + dy * dy)))
        centre = ((start[0] + end[0]) / 2.0, (start[1] + end[1]) / 2.0, (start[2] + end[2]) / 2.0)
        return self.mesh(label, CUBE, centre, rotation=(pitch, yaw, 0.0),
                         scale=(length / 100.0, thickness / 100.0, thickness / 100.0), materials=material, shadow=False)

    def point_light(self, label, local, intensity, color, radius, source_radius=2.0, shadows=True, temperature=None):
        actor = actors_api.spawn_actor_from_class(unreal.PointLight, self.at(local))
        actor.set_actor_label(label)
        component = actor.point_light_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_intensity_units(unreal.LightUnits.CANDELAS)
        component.set_intensity(intensity)
        component.set_light_color(unreal.LinearColor(*color))
        component.set_attenuation_radius(radius)
        component.set_source_radius(source_radius)
        component.set_cast_shadows(shadows)
        if temperature:
            component.set_use_temperature(True)
            component.set_temperature(temperature)
        return actor

    def niagara(self, label, system_path, local, scale=1.0):
        system = unreal.load_asset(system_path)
        if system is None:
            print("niagara missing", system_path)
            return None
        actor = actors_api.spawn_actor_from_object(system, self.at(local))
        actor.set_actor_label(label)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        return actor


# --- Bunker and map table --------------------------------------------------------------------------------------------
def _first_existing(paths):
    for path in paths:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return path
    return None


def build_bunker():
    """Resistance command bunker, 1976: concrete room, one oil lamp, the map table.
    Layout (cm, set origin = floor under the table centre): officer's side is -X, north on the map is +X."""
    s = Set("Intro_Bunker")
    concrete = M + "MI_Intro_Concrete"
    half_x, half_y, height, wall = 420.0, 520.0, 270.0, 40.0
    s.box("Bunker_Floor", (0, 0, -10), (2 * half_x + 2 * wall, 2 * half_y + 2 * wall, 20), concrete)
    s.box("Bunker_Ceiling", (0, 0, height + 15), (2 * half_x + 2 * wall, 2 * half_y + 2 * wall, 30), concrete)
    s.box("Bunker_Wall_N", (half_x + wall / 2, 0, height / 2), (wall, 2 * half_y + 2 * wall, height), concrete)
    s.box("Bunker_Wall_S", (-half_x - wall / 2, 0, height / 2), (wall, 2 * half_y + 2 * wall, height), concrete)
    s.box("Bunker_Wall_E", (0, half_y + wall / 2, height / 2), (2 * half_x, wall, height), concrete)
    s.box("Bunker_Wall_W", (0, -half_y - wall / 2, height / 2), (2 * half_x, wall, height), concrete)
    for index, y in enumerate((-340.0, 0.0, 340.0)):
        s.box("Bunker_Beam_%d" % index, (0, y, height - 14), (2 * half_x, 34, 28), concrete)

    # Table: the modelled one when it has been imported, otherwise a greybox slab on trestles.
    table_mesh = _first_existing([lib.ROOT + "/Props/SM_Intro_MapTable"])
    if table_mesh:
        s.mesh("MapTable", table_mesh, (0, 0, 0), rotation=(0, 90, 0))
    else:
        wood = M + "MI_Intro_DarkWood"
        s.box("MapTable_Top", (0, 0, TABLE_TOP_Z - 4), (240, 360, 8), wood)
        for index, (x, y) in enumerate(((-95, -150), (95, -150), (-95, 150), (95, 150))):
            s.box("MapTable_Leg_%d" % index, (x, y, (TABLE_TOP_Z - 8) / 2), (10, 10, TABLE_TOP_Z - 8), wood)

    build_map(s)

    # Props on the officer's side of the table.
    s.mesh("Prop_MP40", "/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/MP40/Mesh/SM_Ger_InfWeapon_MP40",
           (-96, -128, TABLE_TOP_Z + 4.5), rotation=(0, 64, 90))
    s.mesh("Prop_Radio", "/Game/UVB80/Assets/UVB80/Items/SM_Radio", (-88, -52, TABLE_TOP_Z + 0.3), rotation=(0, -168, 0))
    s.mesh("Prop_Mug", "/Game/UVB80/Assets/UVB80/Items/SM_Cup", (-98, -8, TABLE_TOP_Z + 0.3), rotation=(0, 40, 0))
    # (Phone past the ammunition, MP40 clear of the radio: they cut through each other before.)
    s.mesh("Prop_Phone", "/Game/UVB80/Assets/UVB80/Items/SM_OldPhone", (-106, 70, TABLE_TOP_Z + 0.3), rotation=(0, -25, 0))
    # Ammunition for the MP40: three stick magazines, an opened 9 mm box and loose rounds (Blender prop AmmoSpread),
    # on the officer's side of the date line.
    ammo_mesh = _first_existing([lib.ROOT + "/Props/SM_Intro_AmmoSpread"])
    if ammo_mesh:
        s.mesh("Prop_Ammo", ammo_mesh, AMMO_AT, rotation=(0, AMMO_YAW, 0))

    # The oil lamp over the table: the only real light. A weak cool fill keeps the room from going fully black.
    lamp_height = 205.0
    lamp_mesh = _first_existing([lib.ROOT + "/Props/SM_Intro_OilLamp"])
    if lamp_mesh:
        s.mesh("OilLamp", lamp_mesh, (-10, 20, lamp_height + 30), mobile=True, shadow=False)
    else:
        s.mesh("OilLamp", SPHERE, (-10, 20, lamp_height), scale=(0.1, 0.1, 0.14), materials=M + "MI_Intro_Flame",
               shadow=False, mobile=True)
    # Intensities sit in the project's own range (the mission maps' suns are about 3 lux at a fixed exposure).
    s.point_light("OilLampLight", (-10, 20, lamp_height), 2.2, (1.0, 0.62, 0.30, 1.0), 1400.0, source_radius=3.0)
    s.point_light("BunkerFill", (-300, -200, 180), 0.05, (0.55, 0.65, 0.8, 1.0), 1600.0, shadows=False)
    s.niagara("BunkerDust", "/Game/DustAndSandStorm/SandStorm/Niagara/NS_FallingDust_01", (0, 0, 230), 0.4)
    finish_level("Intro_Bunker")
    print("build_bunker done")


def build_map(s):
    """The paper map on the table: sea sheet, every country mesh cut to the sheet, pins, string, rings, the stain."""
    width_y = (MAP_BOUNDS[3] - MAP_BOUNDS[2]) * MAP_SCALE
    depth_x = (MAP_BOUNDS[1] - MAP_BOUNDS[0]) * MAP_SCALE
    centre = (SHEET_CENTRE_OFFSET_X, 0.0)
    s.plane("Map_Sea", (centre[0], centre[1], SHEET_Z), (depth_x + 6, width_y + 6), M + "MI_Intro_Sea")
    # Countries: one shared transform puts all of them in place.
    centre_x_km = (MAP_BOUNDS[0] + MAP_BOUNDS[1]) / 2.0
    centre_y_km = (MAP_BOUNDS[2] + MAP_BOUNDS[3]) / 2.0
    local = (-centre_x_km * MAP_SCALE + SHEET_CENTRE_OFFSET_X, -centre_y_km * MAP_SCALE, SHEET_Z + MAP_LIFT)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    countries = sorted(str(a.package_name) for a in registry.get_assets_by_path("/Game/EarthWorldMap/Meshes/Countries/8k", False))
    placed = 0
    for index, path in enumerate(countries):
        mesh = unreal.load_asset(path)
        bounds = mesh.get_bounds()
        low_x, high_x = bounds.origin.x - bounds.box_extent.x, bounds.origin.x + bounds.box_extent.x
        low_y, high_y = bounds.origin.y - bounds.box_extent.y, bounds.origin.y + bounds.box_extent.y
        if high_x < MAP_BOUNDS[0] or low_x > MAP_BOUNDS[1] or high_y < MAP_BOUNDS[2] or low_y > MAP_BOUNDS[3]:
            continue
        name = path.rsplit("/", 1)[1]
        material = M + ("MI_Intro_Map_Reich" if name in ("SM_Germany", "SM_Austria") else "MI_Intro_Map_Land%d" % (index % 5))
        s.mesh("Map_" + name[3:], path, local, scale=(MAP_SCALE, MAP_SCALE, MAP_SCALE), materials=material, shadow=False)
        placed += 1
    print("countries placed", placed, "sheet %.0f x %.0f cm" % (width_y, depth_x))

    top = SHEET_Z + MAP_LIFT

    def place(name_or_latlon, lift=0.0):
        lat, lon = PLACES[name_or_latlon] if isinstance(name_or_latlon, str) else name_or_latlon
        x, y = map_local(lat, lon)
        return (x, y, top + lift)

    pin_mesh = _first_existing([lib.ROOT + "/Props/SM_Intro_MapPin"])
    star_mesh = _first_existing([lib.ROOT + "/Props/SM_Intro_StarPin"])
    ring_mesh = _first_existing([lib.ROOT + "/Props/SM_Intro_BurnRing"])

    def pin(label, where, scale=1.0, star=False, burn=None):
        """burn = (group, delay, duration): the pin of a struck place glows, chars and leans over in the fire."""
        position = place(where)
        if star and star_mesh:
            return s.mesh(label, star_mesh, position, scale=(scale, scale, scale))
        if pin_mesh:
            material = None
            if burn:
                material = fire.pin_instance(label, burn[0], burn[1], burn[2], (len(label) * 0.137 + burn[1] * 0.31) % 1.0)
            return s.mesh(label, pin_mesh, position, scale=(scale, scale, scale), materials=material, mobile=bool(burn))
        s.mesh(label + "_Needle", CYLINDER, (position[0], position[1], position[2] + 1.4 * scale),
               scale=(0.0018 * scale, 0.0018 * scale, 0.028 * scale), materials=M + "MI_Intro_PinBrass")
        return s.mesh(label, SPHERE, (position[0], position[1], position[2] + 3.0 * scale),
                      scale=(0.011 * scale, 0.011 * scale, 0.011 * scale),
                      materials=M + ("MI_Intro_PinBrass" if star else "MI_Intro_PinRed"))

    ring_parent = unreal.load_asset(M + "M_Intro_BurnRing")

    def ring(label, where, size, group, delay, duration=1.6):
        """A scorch at a place; it starts burning 'delay' seconds into its group's clock (MPC Rings36/42/62)."""
        position = place(where, 0.12)
        mats.instance("MI_Intro_" + label, ring_parent, scalars={"Group": group, "Delay": delay, "Duration": duration})
        material = M + "MI_Intro_" + label
        # The strike burns: flames, sparks and smoke on an upright card, and their light on the paper.
        seed = (delay * 0.731 + size * 0.389 + group * 0.17) % 1.0
        flame_mi, glow_mi = fire.ring_instances(label, group, delay, duration, size, seed)
        width, height = 2.4 * size, 5.0 * size          # as in fire.ring_instances
        for suffix, rotation in (("_FireA", (0.0, 90.0, 90.0)), ("_FireB", (0.0, 0.0, 90.0))):
            card = s.mesh(label + suffix, PLANE, (position[0], position[1], position[2] + height / 2.0 - 0.1),
                          rotation=rotation, scale=(width / 100.0, height / 100.0, 1.0), materials=flame_mi,
                          shadow=False, mobile=True)
            card.static_mesh_component.set_translucent_sort_priority(3)
        pool = s.plane(label + "_FireLight", (position[0], position[1], position[2] + 0.08), (4.0 * size, 4.0 * size),
                       glow_mi, mobile=True)
        pool.static_mesh_component.set_translucent_sort_priority(2)
        if ring_mesh:
            return s.mesh(label, ring_mesh, position, scale=(size / 6.0, size / 6.0, 1.0),
                          materials=material, shadow=False, mobile=True)
        return s.plane(label, position, (size, size), material, mobile=True)

    # Berlin and the cities that followed (3.5 / 3.6).
    pin("Pin_Berlin", "Berlin", 0.55, burn=(0, -10.0, 1.6))
    ring("Ring_Berlin", "Berlin", 2.6, 0, -10.0)
    for index, city in enumerate(GERMAN_CITIES):
        pin("Pin_City_%d" % index, city, 0.45, burn=(0, 0.9 + index * 0.6, 1.6))
        ring("Ring_City_%d" % index, city, 2.0, 0, 0.9 + index * 0.6)
    # Manhattan Project sites (4.x): three pins with paper tags, a red string from the east.
    for index, site in enumerate(("LosAlamos", "OakRidge", "Hanford")):
        pin("Pin_" + site, site, 1.0, burn=(1, 3.6 + index * 1.7, 2.6))
        ring("Ring_" + site, site, 7.0, 1, 3.6 + index * 1.7, 2.6)
        x, y, z = place(site)
        tag_mesh = _first_existing([lib.ROOT + "/Props/SM_Intro_PaperTag"])
        tag_material = M + "MI_Intro_Tag_" + site
        if tag_mesh:
            s.mesh("Tag_" + site, tag_mesh, (x - 9.0, y + 3.0, z + 0.5), rotation=(0, 96 + index * 5, 0),
                   scale=(1.8, 1.8, 1.8), materials=tag_material, mobile=True, shadow=False)
        else:
            s.plane("Tag_" + site, (x - 6.0, y + 1.5, z + 0.5), (4.5, 9.0), tag_material, yaw=8.0 * (index - 1), mobile=True)
    hub = place("Moscow", 1.6)
    pin("Pin_Moscow", "Moscow", 0.8)
    for site in ("LosAlamos", "OakRidge", "Hanford"):
        end = place(site, 2.6)
        s.rod("String_" + site, hub, end, 0.09, M + "MI_Intro_String")
    pin("Pin_SovietStar", (50.2, 20.5), 1.5, star=True)
    # Operation Svarog (6.2b): scattered strikes on Soviet-held Europe.
    for index, strike in enumerate(SVAROG_STRIKES):
        ring("Ring_Svarog_%d" % index, strike, 2.3 + 0.9 * ((index * 7) % 3), 2, 0.5 + ((index * 5) % 16) * 0.4)
    # The stain (5.4) and its pencil label.
    s.plane("Map_Stain", (centre[0], centre[1], top + 0.25), (depth_x, width_y), M + "M_Intro_Stain", mobile=True)
    label_x, label_y = map_local(63.5, 28.0)
    label_x, label_y = map_local(41.0, 12.0)
    s.plane("Map_PencilLabel", (label_x, label_y, top + 0.3), (58.0, 7.3), M + "MI_Intro_PencilPurpleDeath", yaw=90.0, mobile=True)
    # No Ural Shield line on the map: a line next to the stain would hint at why the Soviets withdrew (doc table).
    # The hand-drawn date line along the officer's edge of the table (1.2 / 9.2): 1944 at the west end.
    s.plane("Map_DateLine", (-64.0, 0.0, TABLE_TOP_Z + 0.5), (320.0, 20.0), M + "MI_Intro_DateLine", yaw=90.0)
