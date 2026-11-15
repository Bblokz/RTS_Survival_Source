"""Scout stills in the throwaway editor: a scene capture looks through candidate camera positions so shots can be
framed without a full render. Nothing is saved; the capture actor is destroyed afterwards.

Usage (a request file sent with intro_cmd.py):
    import intro_scout as scout
    _defer = scout.start("berlin_a", ["Ger_Berlin"], [("gate", (x, y, z), (pitch, yaw, roll), 60.0), ...])
Stills: Saved/IntroCinematic/scout/<tag>/<name>.png, sheet: Saved/IntroCinematic/scout/<tag>.jpg (build with
scout_sheet.py after the request returns).
"""
import os

import unreal

import intro_lib as lib

OUT = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/scout"
WIDTH, HEIGHT = 960, 540
SETTLE_TICKS = 10


def set_levels_visible(names):
    """Shows exactly these streamed levels in the editor world (all others hidden)."""
    world = lib.editor_world()
    for level in unreal.EditorLevelUtils.get_levels(world):
        package = level.get_outermost().get_name()
        if package == lib.MAP:
            continue
        short = package.rsplit("/", 1)[1]
        unreal.EditorLevelUtils.set_level_visibility(level, short in names, False)
    lib.make_persistent_current()


def start(tag, levels, views, exposure_bias=None):
    """Returns the _defer callable that captures every view over the next ticks."""
    set_levels_visible(levels)
    world = lib.editor_world()
    folder = "%s/%s" % (OUT, tag)
    os.makedirs(folder, exist_ok=True)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    capture_actor = actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 0))
    capture_actor.set_actor_label("IntroScoutCapture")
    component = capture_actor.capture_component2d
    target = unreal.RenderingLibrary.create_render_target2d(world, WIDTH, HEIGHT, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    component.texture_target = target
    component.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    component.capture_every_frame = False
    component.capture_on_movement = False
    component.always_persist_rendering_state = True
    if exposure_bias is not None:
        settings = component.post_process_settings
        settings.override_auto_exposure_bias = True
        settings.auto_exposure_bias = exposure_bias
        component.post_process_settings = settings
        component.post_process_blend_weight = 1.0
    state = {"index": 0, "ticks": 0, "moved": False}

    def _defer():
        if state["index"] >= len(views):
            actors.destroy_actor(capture_actor)
            print("scout", tag, len(views), "views ->", folder)
            return True
        name, location, rotation, fov = views[state["index"]]
        if not state["moved"]:
            capture_actor.set_actor_location_and_rotation(
                unreal.Vector(*location), unreal.Rotator(rotation[2], rotation[0], rotation[1]), False, False)
            component.fov_angle = fov
            state["moved"], state["ticks"] = True, 0
            return False
        state["ticks"] += 1
        component.capture_scene()
        if state["ticks"] < SETTLE_TICKS:
            return False
        unreal.RenderingLibrary.export_render_target(world, target, folder, name + ".png")
        state["index"] += 1
        state["moved"] = False
        return False

    return _defer


def ground_z(x, y, z_from=50000.0, z_to=-50000.0):
    """Height of the first blocking surface under (x, y), or None."""
    world = lib.editor_world()
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, z_from), unreal.Vector(x, y, z_to), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
        unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    return hit.to_tuple()[4].z
