"""Read-only: how the two wall splines are set up (Blueprint variables, spline points, piece lengths) and map facts."""
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldMapWall")
LINES = []


def log(text):
    LINES.append(str(text))
    unreal.log("URALWALL " + str(text))


try:
    for name in ("T_UralShield_BaseColor", "T_UralShield_Normal", "T_UralShield_ORM"):
        t = unreal.load_asset("/Game/RTS_Survival/Cinematics/Intro1976/UralShield/Textures/" + name)
        log("TEX %s %dx%d srgb=%s comp=%s" % (name, t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.srgb,
                                           t.compression_settings))
    master = unreal.load_asset("/Game/RTS_Survival/Cinematics/Intro1976/Materials/M_Intro_PBR")
    log("M_Intro_PBR used_with_spline_meshes=%s" % master.get_editor_property("used_with_spline_meshes"))
    for path in ("/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Wall/WallOnly/MI_Global_SovietWallOnly",
                 "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Wall/NewWallgate/M_Global_SovietWallGate"):
        m = unreal.load_asset(path)
        base = m.get_base_material()
        log("OLDMAT %s base %s spline=%s" % (path, base.get_path_name(), base.get_editor_property("used_with_spline_meshes")))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for pkg in ("/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Wall/WallOnly/SM_Global_SovietWall",
                "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Wall/NewWallgate/SM_Global_SovietWallWithGate2"):
        log("REFS %s <- %s" % (pkg, [str(r) for r in registry.get_referencers(pkg, unreal.AssetRegistryDependencyOptions())]))

    bp = unreal.load_asset("/Game/RTS_Survival/Blueprints/Environment/Splines/BP_RoadSpline")
    cdo = unreal.get_default_object(bp.generated_class())
    names = [n for n in dir(cdo) if not n.startswith("_")]
    log("BP dir sample: %s" % [n for n in names if any(k in n.lower() for k in ("mesh", "spline", "axis", "length", "scale", "mat", "road", "width"))])

    unreal.EditorLoadingAndSavingUtils.load_map("/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    log("actors %d, landscapes %d, navmesh %d" % (
        len(actors), len([a for a in actors if isinstance(a, unreal.LandscapeProxy)]),
        len([a for a in actors if isinstance(a, unreal.RecastNavMesh)])))
    for actor in actors:
        if actor.get_actor_label() == "SM_Global_SovietWallWithGate2":
            o, e = actor.get_actor_bounds(False)
            log("GATE loc %s rot %s scale %s bounds o %s e %s" % (actor.get_actor_location(), actor.get_actor_rotation(),
                                                                   actor.get_actor_scale3d(), o, e))
        if actor.get_class().get_name() != "BP_RoadSpline_C":
            continue
        meshes = actor.get_components_by_class(unreal.SplineMeshComponent)
        if not meshes or meshes[0].get_editor_property("static_mesh") is None or \
                "Wall" not in meshes[0].get_editor_property("static_mesh").get_name():
            continue
        log("SPLINE ACTOR %s loc %s rot %s scale %s" % (actor.get_actor_label(), actor.get_actor_location(),
                                                        actor.get_actor_rotation(), actor.get_actor_scale3d()))
        for n in dir(actor):
            if n.startswith("_"):
                continue
            low = n.lower()
            if not any(k in low for k in ("mesh", "axis", "length", "road", "width", "material", "segment", "spacing")):
                continue
            try:
                value = actor.get_editor_property(n)
            except Exception:
                continue
            log("  prop %s = %s" % (n, value))
        for spline in actor.get_components_by_class(unreal.SplineComponent):
            count = spline.get_number_of_spline_points()
            log("  spline %s points %d length %.1f" % (spline.get_name(), count, spline.get_spline_length()))
            for i in range(count):
                p = spline.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)
                log("    p%d (%.1f %.1f %.1f) dist %.1f" % (i, p.x, p.y, p.z, spline.get_distance_along_spline_at_spline_point(i)))
        for c in meshes:
            s = c.get_start_position(); e = c.get_end_position()
            log("  piece %s fwd=%s start (%.1f %.1f %.1f) end (%.1f %.1f %.1f) len %.1f startscale %s collision=%s" % (
                c.get_name(), c.get_editor_property("forward_axis"), s.x, s.y, s.z, e.x, e.y, e.z, (e - s).length(),
                c.get_start_scale(), c.get_collision_enabled()))
except Exception:
    import traceback
    log(traceback.format_exc())
os.makedirs(OUT, exist_ok=True)
open(os.path.join(OUT, "dump2.txt"), "w", encoding="utf-8").write("\n".join(LINES))
