"""Configures the flare ability Niagara systems (stateless emitters from the FountainLightweight template).
Run through the UECP bridge (execute_uecp_script):
    SYSTEM = 'burn'  # or 'trail' / 'launch'
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/FlareAbility/flare_niagara.py").read())
Each system must already exist with its emitters named as below (create from FountainLightweight, duplicate the
'Fountain' emitter, rename). The flare actor attaches TRAIL while climbing and BURN from ignition; LAUNCH plays once
at the turret socket (its +X is the tube direction)."""

FLARE = "/Game/RTS_Survival/SpecialEffects/Flare/"
# The materials of the project's APC smoke discharger (NS_SmokeScreen_APC). Its glows use colours in the thousands:
# the auto exposure needs that much for a glow to read by day.
GLOW = "/Game/RTS_Survival/SpecialEffects/ArmyVFX/Materials/M_Fire_Spark_1.M_Fire_Spark_1"
SPARK = GLOW
SMOKE = "/Game/RTS_Survival/SpecialEffects/ArmyVFX/Materials/M_SmokeScreen_1.M_SmokeScreen_1"
# M_SmokeScreen_1 is a 10x10 flipbook, played once over each particle's life.
SMOKE_RENDERER = [{"property_name": "SubImageSize.X", "float_value": 10.0},
                  {"property_name": "SubImageSize.Y", "float_value": 10.0}]
SMOKE_SUBUV = [("SubUVAnimation", "NumFrames", 100)]
# Cone axis is +Z; pitch -90 turns it to +X (the launch tube), 180 points it down.
# Rotator inputs only take their components one by one (struct text and vectors are ignored without an error).
TO_X = -90.0
DOWN = 180.0
BOUNDS = 2500


def glow(rate, life, size, color, burst=None):
    return {
        "material": GLOW,
        "spawn": burst or {"spawn_rate": rate},
        "disable": ["AddVelocity", "GravityForce", "Drag"],
        "dist": [("InitializeParticle", "LifetimeDistribution", "range", life),
                 ("InitializeParticle", "SpriteSizeDistribution", "nonuniform_range", ([size[0]] * 2, [size[1]] * 2)),
                 ("InitializeParticle", "ColorDistribution", "nonuniform_range", (color, color))],
        "params": [("ShapeLocation", "SphereMax", 2.0)],
        "curves": [("ScaleColor", "ScaleDistribution", [[1, 1, 1, 1], [1, 1, 1, 0.7], [1, 1, 1, 0]])],
    }


SPECS = {
    "burn": {
        # The burning star: a hot white-yellow core that flickers through random sizes.
        "core": glow(45, (0.08, 0.16), (160, 260), [1600, 1100, 650, 1]),
        # Soft halo that makes the flare read from the RTS camera, also by day.
        "halo": glow(14, (0.25, 0.4), (600, 800), [140, 85, 40, 0.35]),
        # Magnesium sparks dripping from the cartridge.
        "sparks": {
            "material": SPARK,
            "spawn": {"spawn_rate": 75},
            "enable": ["AddVelocity", "GravityForce", "Drag"],
            "dist": [("InitializeParticle", "LifetimeDistribution", "range", (0.5, 1.1)),
                     ("InitializeParticle", "SpriteSizeDistribution", "nonuniform_range", ([5, 5], [10, 10])),
                     ("InitializeParticle", "ColorDistribution", "nonuniform_range", ([500, 260, 90, 1], [800, 420, 140, 1])),
                     ("AddVelocity", "ConeVelocityDistribution", "range", (120, 420)),
                     ("Drag", "DragDistribution", "constant", 0.8)],
            "params": [("AddVelocity", "ConeAngle", 75.0), ("AddVelocity", "ConeRotation.Pitch", DOWN),
                       ("ShapeLocation", "SphereMax", 4.0)],
            "curves": [("ScaleColor", "ScaleDistribution", [[1, 1, 1, 1], [1, 0.6, 0.3, 1], [0.6, 0.2, 0.1, 0]])],
        },
        # Thick white smoke rising from the flare and hanging above it.
        "smoke": {
            "material": SMOKE,
            "spawn": {"spawn_rate": 14},
            "enable": ["AddVelocity", "GravityForce", "Drag", "ScaleSpriteSize", "CurlNoiseForce", "SubUVAnimation"],
            "dist": [("InitializeParticle", "LifetimeDistribution", "range", (3.5, 5.5)),
                     ("InitializeParticle", "SpriteSizeDistribution", "nonuniform_range", ([70, 70], [120, 120])),
                     ("InitializeParticle", "ColorDistribution", "nonuniform_range",
                      ([0.85, 0.83, 0.8, 0.55], [1, 0.98, 0.95, 0.7])),
                     ("AddVelocity", "ConeVelocityDistribution", "range", (20, 70)),
                     ("GravityForce", "GravityDistribution", "nonuniform_range", ([0, 0, 25], [0, 0, 25])),
                     ("Drag", "DragDistribution", "constant", 0.6)],
            "params": [("AddVelocity", "ConeAngle", 60.0), ("ShapeLocation", "SphereMax", 12.0),
                       ("CurlNoiseForce", "NoiseAmplitude", 60.0), ("CurlNoiseForce", "NoiseFrequency", 0.01)],
            "curves": [("ScaleColor", "ScaleDistribution", [[1, 1, 1, 0], [1, 1, 1, 1], [1, 1, 1, 0.6], [1, 1, 1, 0]]),
                       ("ScaleSpriteSize", "ScaleDistribution", [[1, 1], [2.5, 2.5], [4.5, 4.5]])],
        },
    },
    "trail": {
        # Smoke ribbon of puffs left behind the climbing cartridge.
        "smoke": {
            "material": SMOKE,
            "spawn": {"spawn_rate": 150},
            "enable": ["AddVelocity", "Drag", "ScaleSpriteSize", "CurlNoiseForce", "SubUVAnimation"],
            "disable": ["GravityForce"],
            "dist": [("InitializeParticle", "LifetimeDistribution", "range", (1.6, 2.6)),
                     ("InitializeParticle", "SpriteSizeDistribution", "nonuniform_range", ([30, 30], [50, 50])),
                     ("InitializeParticle", "ColorDistribution", "nonuniform_range",
                      ([0.85, 0.83, 0.8, 0.55], [1, 0.98, 0.95, 0.7])),
                     ("AddVelocity", "ConeVelocityDistribution", "range", (5, 30)),
                     ("Drag", "DragDistribution", "constant", 1.0)],
            "params": [("AddVelocity", "ConeAngle", 180.0), ("ShapeLocation", "SphereMax", 4.0),
                       ("CurlNoiseForce", "NoiseAmplitude", 40.0), ("CurlNoiseForce", "NoiseFrequency", 0.02)],
            "curves": [("ScaleColor", "ScaleDistribution", [[1, 1, 1, 1], [1, 1, 1, 0.5], [1, 1, 1, 0]]),
                       ("ScaleSpriteSize", "ScaleDistribution", [[1, 1], [2.5, 2.5], [3.5, 3.5]])],
        },
        "core": glow(40, (0.06, 0.1), (60, 100), [1200, 800, 450, 1]),
        "sparks": {
            "material": SPARK,
            "spawn": {"spawn_rate": 45},
            "enable": ["AddVelocity", "GravityForce", "Drag"],
            "dist": [("InitializeParticle", "LifetimeDistribution", "range", (0.25, 0.5)),
                     ("InitializeParticle", "SpriteSizeDistribution", "nonuniform_range", ([4, 4], [7, 7])),
                     ("InitializeParticle", "ColorDistribution", "nonuniform_range", ([500, 260, 90, 1], [800, 420, 140, 1])),
                     ("AddVelocity", "ConeVelocityDistribution", "range", (60, 180)),
                     ("Drag", "DragDistribution", "constant", 1.0)],
            "params": [("AddVelocity", "ConeAngle", 180.0), ("ShapeLocation", "SphereMax", 3.0)],
            "curves": [("ScaleColor", "ScaleDistribution", [[1, 1, 1, 1], [1, 0.5, 0.2, 0]])],
        },
    },
    "launch": {
        # Muzzle flash at the tube mouth.
        "flash": glow(0, (0.06, 0.12), (140, 220), [10000, 7300, 3800, 1], burst={"spawn_rate": 0, "burst_count": 3,
                                                                        "burst_time": 0.0}),
        # Smoke pushed out of the tube along +X, slowing down and rising.
        "smoke": {
            "material": SMOKE,
            "spawn": {"spawn_rate": 0, "burst_count": 30, "burst_time": 0.0},
            "enable": ["AddVelocity", "GravityForce", "Drag", "ScaleSpriteSize", "CurlNoiseForce", "SubUVAnimation"],
            "dist": [("InitializeParticle", "LifetimeDistribution", "range", (2.0, 4.0)),
                     ("InitializeParticle", "SpriteSizeDistribution", "nonuniform_range", ([90, 90], [150, 150])),
                     ("InitializeParticle", "ColorDistribution", "nonuniform_range",
                      ([0.8, 0.78, 0.75, 0.9], [0.95, 0.93, 0.9, 1.0])),
                     ("AddVelocity", "ConeVelocityDistribution", "range", (150, 650)),
                     ("GravityForce", "GravityDistribution", "nonuniform_range", ([0, 0, 40], [0, 0, 40])),
                     ("Drag", "DragDistribution", "constant", 3.5)],
            "params": [("AddVelocity", "ConeAngle", 22.0), ("AddVelocity", "ConeRotation.Pitch", TO_X),
                       ("ShapeLocation", "SphereMax", 4.0),
                       ("CurlNoiseForce", "NoiseAmplitude", 30.0), ("CurlNoiseForce", "NoiseFrequency", 0.02)],
            "curves": [("ScaleColor", "ScaleDistribution", [[1, 1, 1, 1], [1, 1, 1, 0.6], [1, 1, 1, 0]]),
                       ("ScaleSpriteSize", "ScaleDistribution", [[1, 1], [3, 3], [5, 5]])],
        },
        "sparks": {
            "material": SPARK,
            "spawn": {"spawn_rate": 0, "burst_count": 22, "burst_time": 0.0},
            "enable": ["AddVelocity", "GravityForce", "Drag"],
            "dist": [("InitializeParticle", "LifetimeDistribution", "range", (0.2, 0.45)),
                     ("InitializeParticle", "SpriteSizeDistribution", "nonuniform_range", ([4, 4], [8, 8])),
                     ("InitializeParticle", "ColorDistribution", "nonuniform_range", ([500, 260, 90, 1], [800, 420, 140, 1])),
                     ("AddVelocity", "ConeVelocityDistribution", "range", (500, 1200)),
                     ("Drag", "DragDistribution", "constant", 1.5)],
            "params": [("AddVelocity", "ConeAngle", 18.0), ("AddVelocity", "ConeRotation.Pitch", TO_X),
                       ("ShapeLocation", "SphereMax", 2.0)],
            "curves": [("ScaleColor", "ScaleDistribution", [[1, 1, 1, 1], [1, 0.5, 0.2, 0]])],
        },
    },
}
SYSTEM_PATHS = {"burn": FLARE + "NS_Flare_Burn", "trail": FLARE + "NS_Flare_Trail", "launch": FLARE + "NS_Flare_Launch"}


def _typed(value):
    if isinstance(value, bool):
        return {"bool_value": value}
    if isinstance(value, (int, float)):
        return {"float_value": float(value)}
    if isinstance(value, str):
        # struct text, e.g. a rotator: a vector value is ignored for FRotator inputs
        return {"value": value}
    return {"vector_value": value}


def configure(system_key):
    system = SYSTEM_PATHS[system_key]
    problems = []
    for emitter, spec in SPECS[system_key].items():
        base = {"system_path": system, "emitter_name": emitter}
        for module in spec.get("enable", []):
            uecp.niagara.set_niagara_module_enabled(module_name=module, enabled=True, **base)
        for module in spec.get("disable", []):
            uecp.niagara.set_niagara_module_enabled(module_name=module, enabled=False, **base)
        distributions = []
        for module, prop, mode, value in spec["dist"]:
            entry = {"module_name": module, "property_name": prop, "mode": mode}
            if isinstance(value, tuple):
                entry["min"], entry["max"] = value
            else:
                entry["value"] = value
            distributions.append(entry)
        result = uecp.niagara.set_niagara_distribution_mode(distributions=distributions, **base)
        if not result.get("ok", True):
            problems.append((emitter, "dist", result))
        parameters = [dict(module_name=m, parameter_name=p, **_typed(v)) for m, p, v in spec.get("params", [])]
        if parameters:
            result = uecp.niagara.set_niagara_module_parameters_bulk(parameters=parameters, **base)
            if not result.get("ok", True):
                problems.append((emitter, "params", result))
        curves = [{"module_name": m, "property_name": p, "curve_values": c} for m, p, c in spec.get("curves", [])]
        if curves:
            result = uecp.niagara.set_niagara_distribution_curve(curves=curves, **base)
            if not result.get("ok", True):
                problems.append((emitter, "curves", result))
        uecp.niagara.set_emitter_spawn_rate(**spec["spawn"], **base)
        uecp.niagara.set_niagara_renderer_material(material_path=spec["material"], **base)
        renderer = [{"property_name": "bCastShadows", "bool_value": False}]
        if spec["material"] == SMOKE:
            renderer += SMOKE_RENDERER
            uecp.niagara.set_niagara_module_parameters_bulk(parameters=[
                dict(module_name=m, parameter_name=p, float_value=float(v)) for m, p, v in SMOKE_SUBUV], **base)
        result = uecp.niagara.set_niagara_renderer_properties_bulk(properties=renderer, **base)
        if not result.get("ok", True):
            problems.append((emitter, "renderer", result))
        bounds = {"FixedBounds.Min.X": -BOUNDS, "FixedBounds.Min.Y": -BOUNDS, "FixedBounds.Min.Z": -BOUNDS,
                  "FixedBounds.Max.X": BOUNDS, "FixedBounds.Max.Y": BOUNDS, "FixedBounds.Max.Z": BOUNDS}
        if system_key == "launch":
            # one shot; the dotted EmitterState.* names are refused, the struct text is not
            bounds["EmitterState"] = ("(InactiveResponse=Complete,LoopBehavior=Once,LoopCount=1,LoopDurationMode=Fixed,"
                                      "LoopDuration=(Min=3.000000,Max=3.000000,Mode=UniformConstant))")
        result = uecp.niagara.set_emitter_properties(properties=bounds, **base)
        if result.get("unknown_properties"):
            problems.append((emitter, "emitter_props", result.get("unknown_properties")))
    # read back what silently-ignored writes would leave at the template default
    for emitter, spec in SPECS[system_key].items():
        detail = uecp.niagara.get_niagara_detailed_summary(system_path=system, emitter_name=emitter)["emitters"][0]
        values = {m["module"] + "." + p["name"]: p["value"] for m in detail["module_parameters"] for p in m["properties"]}
        for module, prop, value in spec.get("params", []):
            got = values.get(module + "." + prop)
            difference = None if got is None else got - value
            if difference is not None and prop.startswith("ConeRotation"):
                difference = (difference + 180.0) % 360.0 - 180.0     # 180 may read back as -180
            if isinstance(value, float) and (difference is None or abs(difference) > 1e-3):
                problems.append((emitter, prop, "wanted", value, "got", got))
        renderer = {p["name"]: p["value"] for r in detail["renderers"] for p in r["properties"]}
        if spec["material"] == SMOKE and renderer.get("SubImageSize.X") != 10:
            problems.append((emitter, "SubImageSize", renderer.get("SubImageSize.X")))
    print("CONFIGURED", system_key, "problems:", problems)
    print(uecp.niagara.validate_niagara_system(system_path=system))


configure(SYSTEM)
