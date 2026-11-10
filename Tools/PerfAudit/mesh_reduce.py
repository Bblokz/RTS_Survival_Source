"""Static mesh LOD0 reduction with before/after captures (run headless, see run_mesh_reduce.sh).

Usage: mesh_reduce.py <action> <tag> <mesh>=<value> [<mesh>=<value> ...]
  capture <tag> <mesh>=_ ...        render each mesh (close-up 3/4 view + RTS-distance view) to
                                    Saved/PerfAudit/MeshShots/<mesh name>_<tag>_{close,rts}.png
  reduce  <tag> <mesh>=<pct>[!] ... set LOD0 percent_triangles (0..1), rebuild, save; "!" also removes LODs 1..N
  restore <tag> <mesh>=_ ...        percent_triangles back to 1.0, rebuild, save
  info    <tag> <mesh>=_ ...        log LOD0 reduction settings and triangle counts
  screens <tag> <mesh>=a,b,c ...    set LOD screen sizes (LOD0 first)
The source model is never changed: LOD0 reduction is applied at build time, so restore undoes it completely.
"""
import os
import sys
import unreal

TAG = "RTS_MESH_REDUCE"
SHOT_DIR = unreal.Paths.project_saved_dir() + "PerfAudit/MeshShots/"
SMES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
EXPOSURE_BIAS = 0.0
CAPTURES_PER_VIEW = 4


def log(msg):
    unreal.log_warning("%s %s" % (TAG, msg))


def setup_stage():
    # A fresh level renders in the commandlet (the default commandlet world captures black). Its world init raises
    # a few game-subsystem error dialogs, which unattended mode answers.
    world = unreal.EditorLevelLibrary.new_level("/Temp/PerfMeshStage")
    if not world:
        world = unreal.EditorLevelLibrary.get_editor_world()
    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(0, -50, 35))
    sun.light_component.set_intensity(8.0)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, 0, 0))
    sky.light_component.set_intensity(1.0)
    sky.light_component.set_editor_property("real_time_capture", True)
    floor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(400, 400, 1))
    floor.static_mesh_component.set_material(0, unreal.load_asset("/Engine/EngineMaterials/DefaultMaterial"))
    return world


def capture(mesh, tag):
    os.makedirs(SHOT_DIR, exist_ok=True)
    name = mesh.get_name()
    b = mesh.get_bounds()
    radius = max(b.sphere_radius, 10.0)
    centre = b.origin
    log("bounds %s origin=(%.0f,%.0f,%.0f) extent=(%.0f,%.0f,%.0f) radius=%.0f" % (name, b.origin.x, b.origin.y, b.origin.z,
        b.box_extent.x, b.box_extent.y, b.box_extent.z, b.sphere_radius))
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.static_mesh_component.set_static_mesh(mesh)
    # seat the mesh on the floor
    low = b.origin.z - b.box_extent.z
    actor.set_actor_location(unreal.Vector(-centre.x, -centre.y, -low + 1.0), False, False)
    target = unreal.Vector(0, 0, b.box_extent.z)
    rt = unreal.RenderingLibrary.create_render_target2d(unreal.EditorLevelLibrary.get_editor_world(), 1024, 1024,
                                                        unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    cap = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    comp = cap.capture_component2d
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property("capture_every_frame", False)
    comp.set_editor_property("capture_on_movement", False)
    for view, dist_mult, fov in (("close", 2.6, 40.0), ("rts", None, 90.0)):
        if dist_mult:
            dist = radius * dist_mult
        else:
            dist = 3140.0
        pitch, yaw = -55.0 if view == "rts" else -30.0, 35.0
        rot = unreal.Rotator(0, pitch, yaw)
        fwd = rot.get_forward_vector()
        loc = target - fwd * dist
        cap.set_actor_location_and_rotation(loc, rot, False, False)
        comp.set_editor_property("fov_angle", fov)
        if view == "rts":
            # crop: narrow FOV that keeps the same pixel density as 90 deg at 1920 wide, centred on the mesh
            px = max(64.0, radius * 2.0 / (2.0 * dist * 1.0) * 1920.0)  # mesh diameter in 1080p pixels
            fov_crop = 90.0 * 1024.0 / 1920.0
            comp.set_editor_property("fov_angle", fov_crop)
        for _ in range(CAPTURES_PER_VIEW):
            comp.capture_scene()
        path = SHOT_DIR + "%s_%s_%s.png" % (name, tag, view)
        unreal.RenderingLibrary.export_render_target(unreal.EditorLevelLibrary.get_editor_world(), rt, SHOT_DIR,
                                                     "%s_%s_%s.png" % (name, tag, view))
        log("shot %s" % path)
    unreal.EditorLevelLibrary.destroy_actor(cap)
    unreal.EditorLevelLibrary.destroy_actor(actor)


def info(mesh):
    rs = SMES.get_lod_reduction_settings(mesh, 0)
    log("info %s lods=%d tris0=%d pct=%.3f" % (mesh.get_path_name(), mesh.get_num_lods(), mesh.get_num_triangles(0),
                                               rs.percent_triangles))


def set_pct(mesh, pct):
    before = mesh.get_num_triangles(0)
    rs = SMES.get_lod_reduction_settings(mesh, 0)
    rs.percent_triangles = pct
    rs.percent_vertices = 1.0
    try:
        rs.termination_criterion = unreal.StaticMeshReductionTerimationCriterion.TRIANGLES
    except Exception:
        pass
    SMES.set_lod_reduction_settings(mesh, 0, rs)
    after = mesh.get_num_triangles(0)
    ok = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    log("reduce %s pct=%.3f tris %d -> %d saved=%s" % (mesh.get_path_name(), pct, before, after, ok))


def main(argv):
    action, tag = argv[0], argv[1]
    pairs = [a.split("=", 1) for a in argv[2:]]
    unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
    if action == "capture":
        setup_stage()
    for path, value in pairs:
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            log("ERROR not a static mesh: %s" % path)
            continue
        if action == "info":
            info(mesh)
        elif action == "capture":
            capture(mesh, tag)
        elif action == "reduce":
            # "<pct>!" also removes the extra LODs (imported LODs that barely reduce anything); revert via git.
            drop_lods = value.endswith("!")
            set_pct(mesh, float(value.rstrip("!")))
            if drop_lods and mesh.get_num_lods() > 1:
                SMES.remove_lods(mesh)
                ok = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
                log("removed extra LODs %s lods=%d tris0=%d saved=%s" % (mesh.get_path_name(), mesh.get_num_lods(),
                                                                      mesh.get_num_triangles(0), ok))
        elif action == "restore":
            set_pct(mesh, 1.0)
        elif action == "scale":
            # value: factor applied to the current LOD0 percent_triangles (e.g. 0.5 halves a baked 8k corpse to 4k)
            current = SMES.get_lod_reduction_settings(mesh, 0).percent_triangles
            set_pct(mesh, current * float(value))
        elif action == "cap":
            # value: max LOD0 triangles; meshes above it get their percent scaled down to land on it (baker-style cap)
            cap = float(value)
            tris0 = mesh.get_num_triangles(0)
            if tris0 > cap:
                current = SMES.get_lod_reduction_settings(mesh, 0).percent_triangles
                set_pct(mesh, current * cap / tris0)
            else:
                log("cap %s tris0=%d already <= %d" % (mesh.get_path_name(), tris0, cap))
        elif action == "addlod1":
            # value: "<pct of LOD0>,<screen size>" -> exactly one generated LOD1 below the (kept) LOD0 reduction
            pct_of_lod0, screen = [float(v) for v in value.split(",")]
            lod0_pct = SMES.get_lod_reduction_settings(mesh, 0).percent_triangles
            options = unreal.EditorScriptingMeshReductionOptions()
            options.auto_compute_lod_screen_size = False
            settings = [unreal.EditorScriptingMeshReductionSettings(lod0_pct, 1.0),
                        # LOD1's percent is applied to the already reduced LOD0 (measured: 0.14 gave 14% of LOD0)
                        unreal.EditorScriptingMeshReductionSettings(pct_of_lod0, screen)]
            options.reduction_settings = settings
            SMES.set_lods(mesh, options)
            ok = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            log("addlod1 %s lods=%d tris=%s ss=%s saved=%s" % (
                mesh.get_path_name(), mesh.get_num_lods(),
                [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
                [round(v, 3) for v in SMES.get_lod_screen_sizes(mesh)], ok))
        elif action == "screens":
            # value: comma separated LOD screen sizes, e.g. 1.0,0.5,0.25,0.1 (auto compute is turned off)
            sizes = [float(v) for v in value.split(",")]
            before = [round(v, 3) for v in SMES.get_lod_screen_sizes(mesh)]
            SMES.set_lod_screen_sizes(mesh, sizes)
            ok = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            log("screens %s %s -> %s saved=%s" % (mesh.get_path_name(), before,
                                                  [round(v, 3) for v in SMES.get_lod_screen_sizes(mesh)], ok))
    log("DONE")


main(sys.argv[1:])
