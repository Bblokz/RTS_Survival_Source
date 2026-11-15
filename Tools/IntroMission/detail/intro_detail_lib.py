"""Helpers for detailing Ger_IntroMission inside the user's LIVE editor (no second editor, no UECP landscape tools).

Landscape heights and paint are changed only through the engine's ALandscapeProxy.editor_apply_spline with
temporary spline components, so every edit is local (nothing the user sculpts elsewhere is touched).
"""
import math
import unreal

MAP = "Ger_IntroMission"
OUT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + "IntroDetail/"
LAYER_ROOT = "/Game/RTS_Survival/Materials/ground/Landscape/DesertPaintable/"
PREFIX = "IDT_"            # every actor this tool spawns
FOLDER = "IntroDetail"


def world():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert w.get_name() == MAP, "editor is on %s, not %s" % (w.get_name(), MAP)
    return w


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def landscape():
    return [a for a in actors() if a.get_class().get_name() == "Landscape"][0]


def layer(name):
    info = unreal.load_asset(LAYER_ROOT + name + "_LayerInfo")
    assert info, name
    return info


_ignore = None


def ground_z(x, y, refresh=False):
    """Landscape height by line trace (everything except the landscape is ignored)."""
    global _ignore
    if _ignore is None or refresh:
        _ignore = [a for a in actors() if a.get_class().get_name() != "Landscape"]
    hit = unreal.SystemLibrary.line_trace_single(
        world(), unreal.Vector(x, y, 6000), unreal.Vector(x, y, -3000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, _ignore, unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    return hit.to_tuple()[4].z


def make_spline(points):
    """Transient world-space spline through points [(x, y, z), ...]."""
    sp = unreal.new_object(unreal.SplineComponent)
    sp.clear_spline_points(False)
    for p in points:
        sp.add_spline_point(unreal.Vector(p[0], p[1], p[2]), unreal.SplineCoordinateSpace.LOCAL, False)
    sp.update_spline()
    return sp


def apply(points, width, falloff, lower=True, raise_=False, paint=None, subdiv=None, end_width=None,
          end_falloff=None):
    """Rasterise a spline strip into the landscape. width/falloff in cm (full width of the flat part, falloff per side)."""
    sp = make_spline(points)
    n = subdiv or max(8, int(sp.get_spline_length() / 100.0))
    landscape().editor_apply_spline(
        sp, width, width if end_width is None else end_width, falloff, falloff if end_falloff is None else end_falloff,
        0.0, 0.0, n, raise_, lower, layer(paint) if paint else None)


def dip(x, y, radius, depth, yaw=0.0, paint=None):
    """Shallow bowl: a strip across the hollow whose ends sit at ground level and whose middle is `depth` lower."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    pts = []
    for t in (-1.0, -0.5, 0.0, 0.5, 1.0):
        px, py = x + c * radius * t, y + s * radius * t
        g = ground_z(px, py)
        if g is None:
            return False
        pts.append((px, py, g - depth * (math.cos(t * math.pi) * 0.5 + 0.5)))
    apply(pts, radius * 0.9, radius * 0.9, lower=True, raise_=False, paint=paint, subdiv=12)
    return True


def shot(name, cx, cy, width=4000, res=(1600, 1000), lit=True, pitch=-90.0, yaw=-90.0, dist=6000.0, fov=None, ev=None,
         warm=1):
    """Scene capture to Saved/IntroDetail/<name>.png; does not touch the user's viewport."""
    w = world()
    rad_p, rad_y = math.radians(pitch), math.radians(yaw)
    fwd = (math.cos(rad_p) * math.cos(rad_y), math.cos(rad_p) * math.sin(rad_y), math.sin(rad_p))
    gz = ground_z(cx, cy) or 144.0
    loc = unreal.Vector(cx - fwd[0] * dist, cy - fwd[1] * dist, gz - fwd[2] * dist)
    cap = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SceneCapture2D, loc, unreal.Rotator(pitch=pitch, yaw=yaw, roll=0))
    c = cap.capture_component2d
    if fov:
        c.set_editor_property("fov_angle", fov)
    else:
        c.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
        c.set_editor_property("ortho_width", width)
    c.set_editor_property("capture_source",
                          unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR if lit else unreal.SceneCaptureSource.SCS_BASE_COLOR)
    c.set_editor_property("capture_every_frame", False)
    c.set_editor_property("capture_on_movement", False)
    if ev is not None:
        pp = c.get_editor_property("post_process_settings")
        pp.set_editor_property("override_auto_exposure_bias", True)
        pp.set_editor_property("auto_exposure_bias", ev)
        c.set_editor_property("post_process_settings", pp)
        c.set_editor_property("post_process_blend_weight", 1.0)
    rt = unreal.RenderingLibrary.create_render_target2d(w, res[0], res[1], unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property("texture_target", rt)
    for _ in range(max(1, warm)):
        c.capture_scene()
    unreal.RenderingLibrary.export_render_target(w, rt, OUT, name + ".png")
    c.set_editor_property("texture_target", None)
    unreal.RenderingLibrary.release_render_target2d(rt)
    cap.destroy_actor()
    return OUT + name + ".png"


_backup = None


def backup_z(x, y):
    """Bilinear height from the 2 m survey grid taken before any edit (Saved/IntroDetail/heights_backup.txt)."""
    global _backup
    if _backup is None:
        lines = open(OUT + "heights_backup.txt").read().split("\n")
        hdr = [int(v) for v in lines[0].split()]
        _backup = (hdr, [[int(v) for v in r.split()] for r in lines[1:]])
    (x0, x1, y0, y1, st), g = _backup
    fx, fy = (x - x0) / st, (y - y0) / st
    ix, iy = min(max(int(fx), 0), len(g[0]) - 2), min(max(int(fy), 0), len(g) - 2)
    tx, ty = fx - ix, fy - iy
    v = [g[iy][ix], g[iy][ix + 1], g[iy + 1][ix], g[iy + 1][ix + 1]]
    if min(v) < -5000:
        return None
    return (v[0] * (1 - tx) + v[1] * tx) * (1 - ty) + (v[2] * (1 - tx) + v[3] * tx) * ty


def restore(x0, x1, y0, y1, paint=None):
    """Put a rectangle back to the surveyed heights (strips along Y every metre); optional repaint."""
    x = x0
    while x <= x1:
        pts = []
        y = y0
        while y <= y1:
            z = backup_z(x, y)
            if z is not None:
                pts.append((x, y, z))
            y += 200
        if len(pts) >= 2:
            apply(pts, 60, 60, lower=True, raise_=True, paint=paint, subdiv=len(pts) * 2)
        x += 100


def log(name, text):
    with open(OUT + name, "w") as f:
        f.write(text)
