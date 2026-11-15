"""Read-only survey of the first section of Ger_IntroMission (run inside the live editor).

Writes to Saved/IntroDetail: heights.txt (2 m line-trace grid), weights_<layer>.png, top_basecolor.png.
Never calls the UECP landscape tools (they crash the editor).
"""
import unreal

OUT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + "IntroDetail/"
X0, X1, Y0, Y1, STEP = -41500, -9000, -12000, 9500, 200


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def landscape():
    return [a for a in actors() if a.get_class().get_name() == "Landscape"][0]


def heights():
    w = world()
    ignore = [a for a in actors() if a.get_class().get_name() != "Landscape"]
    rows = []
    y = Y0
    while y <= Y1:
        row = []
        x = X0
        while x <= X1:
            hit = unreal.SystemLibrary.line_trace_single(
                w, unreal.Vector(x, y, 5000), unreal.Vector(x, y, -2000),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                unreal.DrawDebugTrace.NONE, True)
            z = -9999
            if hit:
                t = hit.to_tuple()
                z = t[4].z  # location
            row.append("%d" % round(z))
            x += STEP
        rows.append(" ".join(row))
        y += STEP
    with open(OUT + "heights.txt", "w") as f:
        f.write("%d %d %d %d %d\n" % (X0, X1, Y0, Y1, STEP))
        f.write("\n".join(rows))
    return len(rows)


def weights():
    w = world()
    ls = landscape()
    done = []
    for name in ls.get_target_layer_names():
        rt = unreal.RenderingLibrary.create_render_target2d(w, 1024, 1024, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        ok = ls.landscape_export_weightmap_to_render_target(rt, name)
        unreal.RenderingLibrary.export_render_target(w, rt, OUT, "weights_%s.png" % name)
        unreal.RenderingLibrary.release_render_target2d(rt)
        done.append((str(name), ok))
    return done


def top(name="top_basecolor", cx=-25250, cy=-1250, width=33000, res=(2048, 1344), source=None):
    w = world()
    cap = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SceneCapture2D, unreal.Vector(cx, cy, 6000), unreal.Rotator(pitch=-90, yaw=-90, roll=0))
    c = cap.capture_component2d
    c.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
    c.set_editor_property("ortho_width", width)
    c.set_editor_property("capture_source", source or unreal.SceneCaptureSource.SCS_BASE_COLOR)
    c.set_editor_property("capture_every_frame", False)
    c.set_editor_property("capture_on_movement", False)
    rt = unreal.RenderingLibrary.create_render_target2d(w, res[0], res[1], unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property("texture_target", rt)
    c.capture_scene()
    unreal.RenderingLibrary.export_render_target(w, rt, OUT, name + ".png")
    c.set_editor_property("texture_target", None)
    unreal.RenderingLibrary.release_render_target2d(rt)
    cap.destroy_actor()
    return name
