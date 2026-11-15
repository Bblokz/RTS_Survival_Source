import sys, importlib, unreal
sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/IntroCinematic")
import intro_lib as lib, intro_materials as mats
importlib.reload(lib); importlib.reload(mats)
import intro_sets as sets
importlib.reload(sets)
import intro_sets2 as sets2
importlib.reload(sets2)
import intro_build as b
importlib.reload(b)
import intro_shots as shots
importlib.reload(shots)
import intro_manifest as manifest
importlib.reload(manifest)
STEPS = "__STEPS__".split(",")
if "mats" in STEPS:
    mats.import_textures()
    mats.build_grade()
    mats.build_sky_materials()
    mats.build_notice_material()
    mats.extras()
    mats.build_stain_material([sets.map_uv(*p) for p in sets.STAIN_SEEDS])
if "mats2" in STEPS:
    import intro_import as imp
    importlib.reload(imp)
    mats.build_earth_view()
    mats.build_ground_material()
    mats.build_cloud_material()
    mats.build_mpc()
    mats.build_grade()
if "kits" in STEPS:
    # Rebuilding M_Intro_PBR drops the texture overrides of its instances: always re-import the kits with it.
    import intro_import as imp
    importlib.reload(imp)
    imp.import_all()
    imp.import_sets()
    imp.tune_set_materials()
if "kit" in STEPS:
    import intro_import as imp
    importlib.reload(imp)
    imp.import_sets()
if "sets" in STEPS:
    sets2.build_draft(); sets2.build_skyline(); sets2.build_skyline_ruin(); sets2.build_space(); sets2.build_clouds(); sets2.build_trench(); sets2.build_crater(); sets2.build_hall()
if "bunker" in STEPS:
    sets.build_bunker()
if "clouds" in STEPS:
    sets2.build_clouds(); sets2.build_crater()
if "trench" in STEPS:
    sets2.build_trench()
if "space" in STEPS:
    sets2.build_space(); sets2.build_draft()
if "hall" in STEPS:
    sets2.build_hall()
if "ural" in STEPS:
    sets2.build_ural()
if "film" in STEPS:
    all_shots = manifest.shots()
    ONLY = "__ONLY__"
    if ONLY:
        all_shots = [s for s in all_shots if any(s["id"].startswith(p) for p in ONLY.split("+"))]
    timeline, _total = b.layout(all_shots)
    cues = manifest.global_cues(timeline) if not ONLY else [c for c in manifest.global_cues(b.layout(manifest.shots())[0]) if c["type"] == "HideActors"]
    shots.build_film(all_shots, cues)
    if "sound" in STEPS:
        import json
        import intro_sound as sound
        importlib.reload(sound)
        data = json.load(open(b.TIMELINE_JSON, encoding="utf-8"))
        sound.add_sounds(data["shots"], data["cues"])
if "map" in STEPS:
    b.setup_map()
if "presets" in STEPS:
    b.build_render_preset("MRQ_Preview", 1920, 1080, fps=5)
    b.build_render_preset("MRQ_Final", 1920, 1080)
print("foreign dirty", lib.foreign_dirty())
