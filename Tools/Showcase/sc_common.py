"""Shared building blocks of the showcase screenshot map (RTS_Showcase): generated stage landscapes, ground-traced
dressing, per-stage lighting and camera presets. Every actor made here is labelled SC_<Stage>_... and sits in the
Showcase/<Stage> outliner folder, so a stage rebuild removes exactly its own actors.

Runs inside the editor (DebugGame build: ARTSShowcaseCamera lives in the game module).
"""
import math
import os
import random
import struct

import unreal

PREFIX = "SC_"
SAVE_DIR = "D:/UE5Projects/RTS_Survival/Saved/Showcase/Terrain"
LANDSCAPE_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/"
BRUSHIFY = "/Game/Brushify/"
ENV = "/Game/RTS_Survival/Blueprints/Environment/"
MESHES = "/Game/RTS_Survival/Meshes/"
LIGHT_TAG_PREFIX = "ShowcaseLight_"
SPAWN_TAG_PREFIX = "ShowcaseSpawn="

GROUND_MATERIALS = {
    "DryCastle": dict(material=LANDSCAPE_ROOT + "CastleGrassDesertDry/MI_RTS_Landscape_DryCastle_Inst",
                      layer_root=LANDSCAPE_ROOT + "CastleGrassDesertDry/DATA/",
                      layers=["GrassCastle", "DryGrass", "Desert", "GraySand", "RockDesert"]),
    "Brushify": dict(material=BRUSHIFY + "Materials/Landscape/MI_Landscape",
                     layer_root=BRUSHIFY + "Materials/Landscape/LayerInfos/",
                     layers=["Grass", "GrassDry", "Forest", "ForestDry", "Desert", "Snow"]),
    "BrushifySnow": dict(material="/Game/RTS_Survival/Maps/Showcase/MI_Showcase_Landscape_CleanSnow",
                         layer_root=BRUSHIFY + "Materials/Landscape/LayerInfos/",
                         layers=["Grass", "GrassDry", "Forest", "ForestDry", "Desert", "Snow"]),
    "Desert": dict(material=LANDSCAPE_ROOT + "DesertPaintable/MI_RTS_Landscape_DesertPaintable",
                   layer_root=LANDSCAPE_ROOT + "DesertPaintable/",
                   layers=["Sand", "DesertRockDark", "Weedy", "DesertGravel", "ScorchSandyStone", "ScorchConcrete"]),
}


def log(text):
    unreal.log_warning("SHOWCASE " + str(text))


def smoothstep(edge0, edge1, value):
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


class Terrain:
    """Height field of one stage in its own frame: u along the view axis (+u = away from the cameras, towards the
    backdrop), v sideways. The stage pad around the units stays gently rolling; past the rim the ground climbs into
    ridges, tallest behind the units so a low camera sees hills instead of the next stage."""

    def __init__(self, stage):
        self.stage = stage
        self.name = stage["name"]
        self.centre = stage["centre"]
        self.yaw = math.radians(stage.get("yaw", 0.0))
        self.components = stage.get("components", 10)
        self.quads = 63
        self.quad_cm = stage.get("quad_cm", 200.0)
        self.verts = self.components * self.quads + 1
        self.half = (self.verts - 1) * self.quad_cm / 2.0
        self.pad = stage.get("pad", (9000.0, 9000.0))
        self.rim = stage.get("rim", 5000.0)
        self.back_rim = stage.get("back_rim", self.rim * 1.6)
        # In front of the units (the camera side) the ground stays open and low for a longer stretch.
        self.front_rim = stage.get("front_rim", self.rim * 0.25)
        self.front_stretch = stage.get("front_stretch", 1.8)
        self.bumps = stage.get("bumps", 80.0)
        self.base = stage.get("base", 400.0)
        self.flats = stage.get("flats", [])
        self.custom_height = stage.get("height_fn")
        rng = random.Random(stage.get("seed", 1))
        self.phases = [rng.uniform(0, math.tau) for _ in range(16)]

    def local(self, x, y):
        dx, dy = x - self.centre[0], y - self.centre[1]
        c, s = math.cos(self.yaw), math.sin(self.yaw)
        return dx * c + dy * s, -dx * s + dy * c

    def world(self, u, v):
        c, s = math.cos(self.yaw), math.sin(self.yaw)
        return self.centre[0] + u * c - v * s, self.centre[1] + u * s + v * c

    def contains(self, x, y):
        return abs(x - self.centre[0]) <= self.half and abs(y - self.centre[1]) <= self.half

    @staticmethod
    def value_noise(x, y, salt):
        x0, y0 = math.floor(x), math.floor(y)
        fx, fy = x - x0, y - y0
        sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)

        def corner(cx, cy):
            h = (cx * 73856093) ^ (cy * 19349663) ^ (salt * 83492791)
            h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
            return (h & 0xFFFF) / 32767.5 - 1.0

        top = corner(x0, y0) + (corner(x0 + 1, y0) - corner(x0, y0)) * sx
        bottom = corner(x0, y0 + 1) + (corner(x0 + 1, y0 + 1) - corner(x0, y0 + 1)) * sx
        return top + (bottom - top) * sy

    def noise(self, u, v, wavelength, index):
        # Value noise, two octaves; sines gave straight diagonal stripes in the paint layers.
        salt = index * 7919 + int(self.phases[index] * 1000)
        return (self.value_noise(u / wavelength, v / wavelength, salt)
                + 0.5 * self.value_noise(u / (wavelength * 0.45), v / (wavelength * 0.45), salt + 1))

    def rim_amount(self, u, v):
        pad_u = self.pad[0] * (self.front_stretch if u < 0.0 else 1.0)
        radius = math.sqrt((u / pad_u) ** 2 + (v / self.pad[1]) ** 2)
        return smoothstep(1.0, 1.9, radius)

    def rim_height(self, u, v):
        """Ridge height: front_rim in front, rim at the sides, back_rim behind."""
        angle = math.atan2(v, u)
        along = math.cos(angle)
        if along >= 0.0:
            return self.rim + (self.back_rim - self.rim) * along ** 1.5
        return self.rim + (self.front_rim - self.rim) * (-along) ** 0.8

    def back_amount(self, u, v):
        """1 behind the stage (away from the cameras), 0 in front."""
        angle = math.atan2(v, u)
        return max(0.0, math.cos(angle)) ** 1.5

    def height(self, x, y):
        u, v = self.local(x, y)
        if self.custom_height is not None:
            return self.custom_height(self, u, v)
        rim = self.rim_amount(u, v)
        crest = max(0.55, 0.75 + 0.3 * self.noise(u, v, 6500.0, 0)) + 0.08 * self.noise(u, v, 1300.0, 7)
        rim_height = self.rim_height(u, v)
        field = self.bumps * (self.noise(u, v, 2200.0, 3) + 0.35 * self.noise(u, v, 700.0, 6))
        raw = self.base + field + rim * rim_height * crest
        return self.flatten(u, v, raw)

    def flatten(self, u, v, raw):
        for flat in self.flats:
            fu, fv, half_u, half_v, flat_height, blend = flat
            outside_u = max(0.0, abs(u - fu) - half_u)
            outside_v = max(0.0, abs(v - fv) - half_v)
            amount = 1.0 - smoothstep(0.0, blend, math.hypot(outside_u, outside_v))
            raw = raw * (1.0 - amount) + flat_height * amount
        return raw

    def slope(self, x, y):
        step = self.quad_cm * 2.0
        dx = self.height(x + step, y) - self.height(x - step, y)
        dy = self.height(x, y + step) - self.height(x, y - step)
        return math.hypot(dx, dy) / (2.0 * step)


def build_landscape(stage, terrain, weights_fn, source_body_instance):
    """Writes the height and weight raws, then creates the painted landscape through the C++ helper."""
    ground = GROUND_MATERIALS[stage["ground"]]
    layers = ground["layers"]
    corner_x = terrain.centre[0] - terrain.half
    corner_y = terrain.centre[1] - terrain.half
    heights = []
    layer_bytes = [bytearray() for _ in layers]
    for row in range(terrain.verts):
        y = corner_y + row * terrain.quad_cm
        for column in range(terrain.verts):
            x = corner_x + column * terrain.quad_cm
            heights.append(max(0, min(65535, 32768 + round(terrain.height(x, y) * 128.0 / 100.0))))
            u, v = terrain.local(x, y)
            values = weights_fn(terrain, u, v, x, y)
            values = [max(0.0, values.get(layer, 0.0)) for layer in layers]
            total = sum(values) or 1.0
            normalized = [int(value * 255.0 / total) for value in values]
            strongest = max(range(len(normalized)), key=lambda index: normalized[index])
            normalized[strongest] += 255 - sum(normalized)
            for buffer, value in zip(layer_bytes, normalized):
                buffer.append(value)
    os.makedirs(SAVE_DIR, exist_ok=True)
    height_path = os.path.join(SAVE_DIR, stage["name"] + "_height.raw")
    with open(height_path, "wb") as handle:
        handle.write(struct.pack("<%dH" % len(heights), *heights))
    layer_paths = []
    for layer_name, buffer in zip(layers, layer_bytes):
        path = os.path.join(SAVE_DIR, "%s_%s.raw" % (stage["name"], layer_name))
        with open(path, "wb") as handle:
            handle.write(bytes(buffer))
        layer_paths.append(path)
    layer_infos = [unreal.load_asset(ground["layer_root"] + layer + "_LayerInfo") for layer in layers]
    missing = [layer for layer, info in zip(layers, layer_infos) if info is None]
    if missing:
        raise RuntimeError("missing layer infos %s for %s" % (missing, stage["name"]))
    label = PREFIX + stage["name"] + "_Landscape"
    landscape = unreal.RTSShortsLandscapeLibrary.create_painted_landscape(
        editor_world(), unreal.Vector(corner_x, corner_y, 0.0), unreal.Vector(terrain.quad_cm, terrain.quad_cm, 100.0),
        terrain.components, terrain.components, terrain.quads, unreal.load_asset(ground["material"]), height_path,
        layer_infos, layer_paths, label)
    if landscape is None:
        raise RuntimeError("landscape for %s was not created" % stage["name"])
    landscape.set_actor_label(label)
    landscape.set_folder_path("Showcase/" + stage["name"])
    if source_body_instance is not None:
        landscape.set_editor_property("body_instance", source_body_instance)
    return landscape


class Stage:
    """Placement helpers for one stage; positions are given in the stage frame (u, v) in cm."""

    def __init__(self, spec, terrain):
        self.spec = spec
        self.name = spec["name"]
        self.terrain = terrain
        self.rng = random.Random(spec.get("seed", 1) * 31 + 7)
        self.count = 0

    # -- bookkeeping -------------------------------------------------------------------------------------------------
    def finish(self, actor, label, folder=""):
        self.count += 1
        actor.set_actor_label("%s%s_%s" % (PREFIX, self.name, label))
        actor.set_folder_path("Showcase/%s%s" % (self.name, "/" + folder if folder else ""))
        return actor

    def ground(self, u, v):
        x, y = self.terrain.world(u, v)
        return self.terrain.height(x, y)

    def yaw(self, local_yaw):
        return math.degrees(self.terrain.yaw) + local_yaw

    # -- spawning ----------------------------------------------------------------------------------------------------
    def blueprint(self, path, label, u, v, yaw=0.0, folder="", z=0.0, scale=1.0, absolute_z=None):
        if not path.startswith("/"):
            resolved = find_asset(path)
            if resolved is None:
                log("MISSING blueprint name " + path)
                return None
            path = resolved
        actor_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
        if actor_class is None:
            log("MISSING blueprint " + path)
            return None
        x, y = self.terrain.world(u, v)
        height = absolute_z if absolute_z is not None else self.terrain.height(x, y) + z
        actor = actor_subsystem().spawn_actor_from_class(actor_class, unreal.Vector(x, y, height),
                                                         unreal.Rotator(roll=0.0, pitch=0.0, yaw=self.yaw(yaw)))
        if actor is None:
            log("SPAWN FAILED " + path)
            return None
        if scale != 1.0:
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        if child_actors_have_timelines(actor):
            actor = replace_with_spawn_marker(actor)
        return self.finish(actor, label, folder)

    def mesh(self, path, label, u, v, yaw=0.0, folder="", scale=1.0, sink=15.0, pitch=0.0, roll=0.0,
             scale3d=None, z=None, material=None):
        if not path.startswith("/"):
            path = find_asset(path) or path
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("MISSING mesh " + path)
            return None
        scale3d = scale3d or (scale, scale, scale)
        bounds = mesh.get_bounds()
        bottom = (bounds.origin.z - bounds.box_extent.z) * scale3d[2]
        x, y = self.terrain.world(u, v)
        height = z if z is not None else self.terrain.height(x, y) - bottom - sink
        actor = actor_subsystem().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, height),
                                                         unreal.Rotator(roll=roll, pitch=pitch, yaw=self.yaw(yaw)))
        actor.static_mesh_component.set_static_mesh(mesh)
        if material is not None:
            for index in range(actor.static_mesh_component.get_num_materials()):
                actor.static_mesh_component.set_material(index, unreal.load_asset(material))
        actor.set_actor_scale3d(unreal.Vector(*scale3d))
        return self.finish(actor, label, folder)

    def decal(self, material_path, label, u, v, size, yaw=0.0, folder="Decals", depth=400.0):
        material = unreal.load_asset(material_path)
        if material is None:
            log("MISSING decal " + material_path)
            return None
        x, y = self.terrain.world(u, v)
        actor = actor_subsystem().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, self.terrain.height(x, y)),
                                                         unreal.Rotator(roll=0.0, pitch=-90.0, yaw=self.yaw(yaw)))
        component = actor.get_component_by_class(unreal.DecalComponent)
        component.set_decal_material(material)
        size_x, size_y = (size, size) if not isinstance(size, (tuple, list)) else size
        component.set_editor_property("decal_size", unreal.Vector(depth, size_y, size_x))
        return self.finish(actor, label, folder)

    def scatter(self, paths, label, count, u_range, v_range, folder, scale=(0.8, 1.2), avoid=None, blueprint=False,
                sink=15.0):
        placed = 0
        attempts = 0
        while placed < count and attempts < count * 20:
            attempts += 1
            u = self.rng.uniform(*u_range)
            v = self.rng.uniform(*v_range)
            if avoid is not None and avoid(u, v):
                continue
            path = self.rng.choice(paths)
            size = self.rng.uniform(*scale)
            yaw = self.rng.uniform(0, 360)
            if blueprint:
                actor = self.blueprint(path, "%s_%03d" % (label, placed), u, v, yaw, folder, z=-sink, scale=size)
            else:
                actor = self.mesh(path, "%s_%03d" % (label, placed), u, v, yaw, folder, scale=size, sink=sink)
            placed += 1 if actor is not None else 0
        return placed

    def skirt(self, material_path, depth=150.0):
        """A huge plane just under the landscape's base height: past the landscape's edge a low camera sees ground in
        the mountains' colour instead of the sky atmosphere's bare planet."""
        x, y = self.terrain.centre
        actor = actor_subsystem().spawn_actor_from_class(unreal.StaticMeshActor,
                                                         unreal.Vector(x, y, self.terrain.base - depth), unreal.Rotator(0, 0, 0))
        actor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
        actor.static_mesh_component.set_material(0, unreal.load_asset(material_path))
        actor.static_mesh_component.set_editor_property("cast_shadow", False)
        # A 12 km plane in the distance field scene produced NaN matrix ensures.
        actor.static_mesh_component.set_editor_property("affect_distance_field_lighting", False)
        actor.set_actor_scale3d(unreal.Vector(12000.0, 12000.0, 1.0))
        return self.finish(actor, "Skirt", "Backdrop")

    def row_by_bounds(self, names, label, front_u, gap, yaw=180.0, folder="Buildings", centre_v=0.0, place=None,
                      mesh=False):
        """Lines actors up along v at their measured footprints: each one's near edge at front_u, gap cm between them,
        the row centred on centre_v. Returns the actors."""
        placed = []
        for index, name in enumerate(names):
            if mesh:
                actor = self.mesh(name, "%s_%02d" % (label, index), front_u, 0.0, yaw, folder, sink=5.0)
            elif place is not None:
                actor = place(self, name, "%s_%02d" % (label, index), front_u, 0.0, yaw, folder)
            else:
                actor = self.blueprint(name, "%s_%02d" % (label, index), front_u, 0.0, yaw, folder)
            if actor is not None:
                placed.append(actor)
        widths = []
        for actor in placed:
            origin, extent = local_bounds(actor)
            widths.append((actor, origin, extent))
        total = sum(2.0 * extent.y for _, _, extent in widths) + gap * max(0, len(widths) - 1)
        v = centre_v - total / 2.0
        for actor, origin, extent in widths:
            location = actor.get_actor_location()
            v_target = v + extent.y
            x, y = self.terrain.world(front_u + extent.x, v_target)
            ground = self.terrain.height(x, y)
            dx, dy = x - origin.x, y - origin.y
            actor.set_actor_location(unreal.Vector(location.x + dx, location.y + dy,
                                                   location.z + (ground - self.terrain.height(location.x, location.y))),
                                     False, False)
            v += 2.0 * extent.y + gap
        return placed

    # -- lighting ----------------------------------------------------------------------------------------------------
    def lighting(self, sun_pitch, sun_yaw, sun_lux=10.0, sun_color=(1.0, 0.96, 0.9), sun_temperature=None,
                 sky_intensity=1.0, fog_density=0.02, fog_falloff=0.2, fog_color=None, fog_start=0.0,
                 exposure_bias=0.0, saturation=1.0, contrast=1.0, temperature=6500.0, tint=0.0, vignette=0.35,
                 bloom=0.4, clouds=False, rayleigh_scale=1.0, mie_scale=1.0, sky_tint=None):
        """Sun, sky atmosphere, captured sky light, height fog and an unbound grading volume, all tagged
        ShowcaseLight_<Stage> so ARTSShowcaseCamera / RTS.Showcase.Shot switch them per stage."""
        tag = unreal.Name(LIGHT_TAG_PREFIX + self.name)
        x, y = self.terrain.centre
        actors = actor_subsystem()

        sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(x, y, 3000),
                                            unreal.Rotator(roll=0.0, pitch=sun_pitch, yaw=self.yaw(sun_yaw)))
        component = sun.get_component_by_class(unreal.DirectionalLightComponent)
        component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("intensity", sun_lux)
        component.set_editor_property("light_color", unreal.Color(*[int(255 * c) for c in sun_color], 255))
        if sun_temperature is not None:
            component.set_editor_property("use_temperature", True)
            component.set_editor_property("temperature", sun_temperature)
        component.set_editor_property("atmosphere_sun_light", True)
        component.set_editor_property("cast_shadows", True)
        component.set_editor_property("light_source_angle", 0.8)
        # Screen-space contact shadows ground the tracks; without them hulls read as hovering.
        component.set_editor_property("contact_shadow_length", 0.06)
        sun.tags = [tag]
        self.finish(sun, "Sun", "Lighting")

        sky = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, 0))
        atmosphere = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
        atmosphere.set_editor_property("rayleigh_scattering_scale", 0.0331 * rayleigh_scale)
        atmosphere.set_editor_property("mie_scattering_scale", 0.003996 * mie_scale)
        if sky_tint is not None:
            atmosphere.set_editor_property("sky_luminance_factor", unreal.LinearColor(*sky_tint, 1.0))
        sky.tags = [tag]
        self.finish(sky, "SkyAtmosphere", "Lighting")

        sky_light = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(x, y, 2000), unreal.Rotator(0, 0, 0))
        light = sky_light.get_component_by_class(unreal.SkyLightComponent)
        light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        light.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
        light.set_editor_property("real_time_capture", True)
        light.set_editor_property("lower_hemisphere_is_black", False)
        light.set_editor_property("intensity", sky_intensity)
        sky_light.tags = [tag]
        self.finish(sky_light, "SkyLight", "Lighting")

        fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, 0))
        fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        fog_component.set_editor_property("fog_density", fog_density)
        fog_component.set_editor_property("fog_height_falloff", fog_falloff)
        fog_component.set_editor_property("start_distance", fog_start)
        if fog_color is not None:
            fog_component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(*fog_color, 1.0))
        fog.tags = [tag]
        self.finish(fog, "HeightFog", "Lighting")

        if clouds:
            cloud = actors.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, 0))
            cloud.tags = [tag]
            self.finish(cloud, "Clouds", "Lighting")

        volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(x, y, 500), unreal.Rotator(0, 0, 0))
        volume.set_editor_property("unbound", True)
        volume.set_editor_property("priority", 50.0)
        settings = volume.settings
        settings.set_editor_property("override_auto_exposure_bias", True)
        settings.set_editor_property("auto_exposure_bias", exposure_bias)
        settings.set_editor_property("override_color_saturation", True)
        settings.set_editor_property("color_saturation", unreal.Vector4(saturation, saturation, saturation, 1.0))
        settings.set_editor_property("override_color_contrast", True)
        settings.set_editor_property("color_contrast", unreal.Vector4(contrast, contrast, contrast, 1.0))
        settings.set_editor_property("override_white_temp", True)
        settings.set_editor_property("white_temp", temperature)
        settings.set_editor_property("override_white_tint", True)
        settings.set_editor_property("white_tint", tint)
        settings.set_editor_property("override_vignette_intensity", True)
        settings.set_editor_property("vignette_intensity", vignette)
        settings.set_editor_property("override_bloom_intensity", True)
        settings.set_editor_property("bloom_intensity", bloom)
        settings.set_editor_property("override_motion_blur_amount", True)
        settings.set_editor_property("motion_blur_amount", 0.0)
        volume.set_editor_property("settings", settings)
        volume.tags = [tag]
        self.finish(volume, "Grading", "Lighting")

    # -- cameras -----------------------------------------------------------------------------------------------------
    def orbit_camera(self, shot, target, radius, azimuth, elevation, fmt="Horizontal16x9", focal=35.0, notes="",
                     margin=1.1, aperture=11.0, look_height=None):
        """Camera on a sphere around target (u, v, height): azimuth 0 looks along +u (from the front), positive
        azimuth swings the camera to the right (+v side). Distance fits a circle of `radius` cm across the frame
        width; a vertical frame is narrower, so it stands further back."""
        sensor_width = 18.0 if fmt == "Vertical1x2" else 36.0
        half_fov = math.atan(sensor_width / (2.0 * focal))
        distance = radius * margin / math.tan(half_fov)
        tu, tv, th = target
        az, el = math.radians(azimuth), math.radians(elevation)
        eu = tu - distance * math.cos(el) * math.cos(az)
        ev = tv + distance * math.cos(el) * math.sin(az)
        eh = th + distance * math.sin(el) + (self.ground(tu, tv) - self.ground(eu, ev))
        eye_height = max(eh, 150.0)
        return self.camera(shot, (eu, ev, eye_height), (tu, tv, th if look_height is None else look_height), fmt, focal,
                           notes, aperture)

    def camera(self, shot, eye, target, fmt="Horizontal16x9", focal=35.0, notes="", aperture=11.0, long_edge=7680):
        """eye/target: (u, v, height above ground at that point) in the stage frame."""
        eu, ev, eh = eye
        tu, tv, th = target
        ex, ey = self.terrain.world(eu, ev)
        tx, ty = self.terrain.world(tu, tv)
        ez = self.terrain.height(ex, ey) + eh
        tz = self.terrain.height(tx, ty) + th
        rotation = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(ex, ey, ez), unreal.Vector(tx, ty, tz))
        # Cameras live in the persistent level: the console filter must find them while their stage is unloaded.
        set_current_level(PERSISTENT_LEVEL)
        camera = actor_subsystem().spawn_actor_from_class(unreal.RTSShowcaseCamera, unreal.Vector(ex, ey, ez), rotation)
        set_current_level(stage_level_name(self.name))
        camera.set_editor_property("stage_name", self.name)
        camera.set_editor_property("shot_name", shot)
        camera.set_editor_property("shot_format", getattr(unreal.RTSShowcaseShotFormat, {
            "Horizontal16x9": "HORIZONTAL16X9", "Vertical1x2": "VERTICAL1X2", "Panorama2x1": "PANORAMA2X1"}[fmt]))
        camera.set_editor_property("long_edge_pixels", long_edge)
        camera.set_editor_property("notes", notes)
        component = camera.get_cine_camera_component()
        component.set_editor_property("current_focal_length", focal)
        component.set_editor_property("current_aperture", aperture)
        focus = component.get_editor_property("focus_settings")
        focus.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE)
        component.set_editor_property("focus_settings", focus)
        return self.finish(camera, "Cam_" + shot, "Cameras")


PERSISTENT_LEVEL = "RTS_Showcase"
MAP_FOLDER = "/Game/RTS_Survival/Maps/Showcase/"


def stage_level_name(stage_name):
    """Stage actors live in the streaming sublevel RTS_Showcase_<Stage>; URTSShowcaseCaptureSubsystem streams in
    only the stage it shoots."""
    return "%s_%s" % (PERSISTENT_LEVEL, stage_name)


def set_current_level(level_name):
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_current_level_by_name(level_name):
        raise RuntimeError("cannot make %s the current level" % level_name)


def ensure_stage_level(stage_name):
    level_name = stage_level_name(stage_name)
    world = editor_world()
    existing = [level for level in unreal.EditorLevelUtils.get_levels(world)
                if level.get_outermost().get_name().endswith("/" + level_name)]
    if not existing:
        set_current_level(PERSISTENT_LEVEL)
        streaming = unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingDynamic,
                                                                       MAP_FOLDER + level_name, False)
        if streaming is None:
            raise RuntimeError("could not create the sublevel " + level_name)
        log("created sublevel " + level_name)
    set_current_level(level_name)


def remove_stage(name):
    prefix = "%s%s_" % (PREFIX, name)
    removed = 0
    for actor in actor_subsystem().get_all_level_actors():
        is_stage_camera = isinstance(actor, unreal.RTSShowcaseCamera) and str(actor.get_editor_property("stage_name")) == name
        if actor.get_actor_label().startswith(prefix) or is_stage_camera:
            actor_subsystem().destroy_actor(actor)
            removed += 1
    return removed


def local_bounds(actor, max_offset=8000.0, max_extent=8000.0):
    """Bounds of the actor's visible meshes near its pivot. Some building Blueprints carry a component parked at the
    world origin, which makes get_actor_bounds span kilometres."""
    location = actor.get_actor_location()
    low, high = None, None
    for component in actor.get_components_by_class(unreal.MeshComponent):
        origin, extent, _ = unreal.SystemLibrary.get_component_bounds(component)
        if (origin - location).length() > max_offset or max(extent.x, extent.y, extent.z) > max_extent:
            continue
        if extent.x + extent.y + extent.z <= 1.0:
            continue
        lo, hi = origin - extent, origin + extent
        low = lo if low is None else unreal.Vector(min(low.x, lo.x), min(low.y, lo.y), min(low.z, lo.z))
        high = hi if high is None else unreal.Vector(max(high.x, hi.x), max(high.y, hi.y), max(high.z, hi.z))
    if low is None:
        return location, unreal.Vector(500.0, 500.0, 500.0)
    return (low + high) * 0.5, (high - low) * 0.5


def child_actors_have_timelines(actor):
    """A child turret actor with a timeline references its class's private curve: the level cannot be saved with it
    (\"Illegal reference to private object\")."""
    for child in actor.get_components_by_class(unreal.ChildActorComponent):
        child_actor = child.get_editor_property("child_actor")
        if child_actor is not None and child_actor.get_components_by_class(unreal.TimelineComponent):
            return True
    return False


def replace_with_spawn_marker(actor):
    """Swaps a unit for a TargetPoint tagged ShowcaseSpawn=<class>; URTSShowcaseCaptureSubsystem spawns it at play."""
    transform = actor.get_actor_transform()
    class_path = actor.get_class().get_path_name()
    actor_subsystem().destroy_actor(actor)
    marker = actor_subsystem().spawn_actor_from_class(unreal.TargetPoint, transform.translation, transform.rotation.rotator())
    marker.tags = [unreal.Name(SPAWN_TAG_PREFIX + class_path)]
    return marker


_ASSET_INDEX = {}


def find_asset(name, roots=("/Game/RTS_Survival",)):
    """Object path of an asset by its name (variants live in assorted sub folders); cached per run."""
    if not _ASSET_INDEX:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        for root in roots:
            for data in registry.get_assets_by_path(root, recursive=True):
                _ASSET_INDEX.setdefault(str(data.asset_name), str(data.package_name))
    return _ASSET_INDEX.get(name)
