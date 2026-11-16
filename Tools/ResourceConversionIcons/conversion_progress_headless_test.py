"""Throwaway-editor check of the converter progress bar on the health widget (never the shared editor):
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
(run_progress_test.ps1 does that.)
No map has a BUILT player converter, so the test gives a player unit whose health widget is W_BpHealthBar (the widget
both converter BXPs use) a UResourceConverterComponent and drives it like the card buttons do
(SetResourceConversionEnabled). The bar's show/hide keys on the same bM_Enabled flag the BXP gating feeds.
Checks: on -> visible; off -> collapsed + 0; on again -> visible; a unit without a converter and the level-placed
(never built) Ger_Ref_Converter -> collapsed; unpaused -> the fill follows the conversion timer.
The mission pauses itself at start; show/hide runs paused, the fill is sampled unpaused at the very end (unpaused,
this map's tank path following can assert in DebugGame).
Writes Saved/ResourceConversionIcons/progress_test_result.json + progress_on/off.png (health widget captured in world
space) and quits.
"""
import json
import os

import unreal

OUT_DIR = "D:/UE5Projects/RTS_Survival/Saved/ResourceConversionIcons"
OUT = OUT_DIR + "/progress_test_result.json"
HEALTH_WIDGET_CLASS = "W_BpHealthBar_C"
TICK_RATE = 3.0
PIE_SETTLE_SECONDS = 40.0
S = {"phase": "wait_load", "t": 0.0, "mark": 0.0, "log": [], "reads": [], "unit": None, "control": None,
     "comp": None}


def save_results():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump({"log": S["log"], "reads": S["reads"]}, open(OUT, "w"), indent=1)


def log(message):
    S["log"].append(message)
    unreal.log("CONVPROG " + message)


def finish():
    save_results()
    unreal.SystemLibrary.quit_editor()
    S["phase"] = "done"


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def health_widget_component(actor):
    for widget_comp in actor.get_components_by_class(unreal.WidgetComponent):
        if isinstance(widget_comp.get_user_widget_object(), unreal.W_HealthBar):
            return widget_comp
    return None


def health_widget(actor):
    widget_comp = health_widget_component(actor)
    return widget_comp.get_user_widget_object() if widget_comp else None


def bar_of(actor):
    widget = health_widget(actor)
    if widget is None:
        return None, None
    try:
        return widget, widget.get_editor_property("ConversionProgressBar")
    except Exception as error:  # noqa: BLE001 - widget class without the property
        log("no ConversionProgressBar on %s: %r" % (widget.get_class().get_name(), error))
        return widget, None


def read(label, actor, expect_visible=None):
    widget, bar = bar_of(actor)
    entry = {"label": label, "actor": actor.get_name(),
             "widget": widget.get_class().get_name() if widget else None}
    if bar is None:
        entry["bar"] = None
    else:
        entry["visibility"] = str(bar.get_visibility())
        entry["percent"] = round(bar.get_editor_property("percent"), 3)
    if expect_visible is not None and bar is not None:
        shown = "COLLAPSED" not in entry["visibility"] and "HIDDEN" not in entry["visibility"]
        entry["ok"] = shown == expect_visible
    S["reads"].append(entry)
    log("%s: %s" % (label, entry))
    save_results()
    return entry


def export_health_bar_shape():
    """Exports the health bar's shape texture (TGA, default exporter) to see where the visible band sits in the
    1024x256 health image, so the conversion bar can be placed relative to it."""
    for texture_path in ["/Game/RTS_HB/Texture/Base_Texture/T_Basic_Shape_HB"]:
        if not unreal.EditorAssetLibrary.does_asset_exist(texture_path):
            continue
        task = unreal.AssetExportTask()
        task.object = unreal.load_asset(texture_path)
        task.filename = OUT_DIR + "/T_Basic_Shape_HB.tga"
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        log("export %s: %s" % (texture_path, unreal.Exporter.run_asset_export_task(task)))
        return
    found = unreal.EditorAssetLibrary.list_assets("/Game/RTS_HB", True, False)
    log("T_Basic_Shape_HB not at the guessed paths; RTS_HB textures: %s"
        % [a for a in found if "Shape" in a or "Healthbar" in a][:10])


def spawn_capture_in_editor_world():
    """PIE duplicates editor-world actors; a SceneCapture2D placed now is found again in the PIE world."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 5000), unreal.Rotator())
    capture.set_actor_label("CONVPROG_CAM")


def screenshot(name, actor):
    """Screen-space UMG is not in HighResShot, so the unit's health widget is switched to world space (full
    1920x1080 designer canvas, render scale 1) and captured by a show-only SceneCapture2D facing it."""
    if os.environ.get("CONVPROG_NO_CAPTURE") == "1":
        log("screenshot %s skipped (CONVPROG_NO_CAPTURE)" % name)
        return
    world = game_world()
    widget_comp = health_widget_component(actor)
    all_captures = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)
    captures = [c for c in all_captures if c.get_actor_label() == "CONVPROG_CAM"]
    log("scene captures in PIE: %d, ours: %d" % (len(all_captures), len(captures)))
    if widget_comp is None or not captures:
        log("screenshot %s skipped: widget comp %s" % (name, widget_comp))
        return
    widget_comp.get_user_widget_object().set_render_scale(unreal.Vector2D(1.0, 1.0))
    widget_comp.set_tickable_when_paused(True)
    # Without a view looking at it, a world-space widget component does not redraw its render target.
    widget_comp.set_editor_property("tick_when_offscreen", True)
    widget_comp.set_widget_space(unreal.WidgetSpace.WORLD)
    widget_comp.set_editor_property("draw_at_desired_size", False)
    widget_comp.set_draw_size(unreal.Vector2D(1920, 1080))
    widget_comp.set_world_rotation(unreal.Rotator(0, 0, 0), False, False)
    widget_location = widget_comp.get_world_location()
    capture = captures[0]
    capture.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    capture.set_actor_location_and_rotation(widget_location + unreal.Vector(1000, 0, 0), unreal.Rotator(0, 0, 180),
                                            False, False)
    capture_comp = capture.capture_component2d
    target = unreal.RenderingLibrary.create_render_target2d(world, 1920, 1080,
                                                            unreal.TextureRenderTargetFormat.RTF_RGBA8)
    capture_comp.set_editor_property("texture_target", target)
    capture_comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    capture_comp.set_editor_property("primitive_render_mode",
                                     unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    capture_comp.clear_show_only_components()
    capture_comp.show_only_component(widget_comp)
    widget_comp.request_redraw()
    S["pending_capture"] = (capture_comp, target, name)
    S["capture_ticks"] = 0


def tick_pending_capture():
    """Captures a few frames after setup so the world-space widget has drawn into its own render target."""
    if "pending_capture" not in S:
        return
    S["capture_ticks"] += 1
    if S["capture_ticks"] < 3:
        return
    capture_comp, target, name = S.pop("pending_capture")
    capture_comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(game_world(), target, OUT_DIR, name + ".png")
    log("captured %s" % name)


def pick_units(world):
    candidates = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        rts = actor.get_component_by_class(unreal.RTSComponent)
        if rts is None or rts.get_editor_property("owning_player") != 1:
            continue
        if actor.get_component_by_class(unreal.ResourceConverterComponent) is not None:
            continue
        widget = health_widget(actor)
        if widget is None:
            continue
        candidates.append((actor, widget.get_class().get_name()))
    return [actor for actor, widget_class in candidates if widget_class == HEALTH_WIDGET_CLASS]


def start_converter_on(unit):
    # AddComponentByClass is not script-exposed; an unregistered component outered to the unit still resolves its
    # owner, world timers and the owner's health widget, which is all the bar needs.
    comp = unreal.new_object(unreal.ResourceConverterComponent, unit)
    settings = unreal.ResourceConverterSettings()
    tick_settings = settings.get_editor_property("tick")
    tick_settings.set_editor_property("start_enabled", True)
    tick_settings.set_editor_property("tick_rate", TICK_RATE)
    settings.set_editor_property("tick", tick_settings)
    comp.init_resource_converter(settings)
    return comp


def run_phase(phase, elapsed):
    if phase == "wait_load" and S["t"] > 15.0:
        export_health_bar_shape()
        if os.environ.get("CONVPROG_NO_CAPTURE") != "1":
            spawn_capture_in_editor_world()
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S["phase"], S["mark"] = "wait_pie", S["t"]
    elif phase == "wait_pie":
        world = game_world()
        if world is not None and elapsed > PIE_SETTLE_SECONDS:
            units = pick_units(world)
            log("player units with %s: %s" % (HEALTH_WIDGET_CLASS, [u.get_name() for u in units]))
            if not units:
                finish()
                return
            S["unit"], S["control"] = units[0], (units[1] if len(units) > 1 else None)
            log("paused at start: %s" % unreal.GameplayStatics.is_game_paused(world))
            read("before converter", S["unit"], expect_visible=False)
            S["comp"] = start_converter_on(S["unit"])
            unreal.GameplayStatics.get_player_controller(world, 0).select_single_unit_from_ui(S["unit"])
            S["phase"], S["mark"] = "on_a", S["t"]
        elif elapsed > 240.0:
            log("pie never started")
            finish()
    elif phase == "on_a" and elapsed > 0.6:
        S["phase"] = "on_shot"
        read("on +0.6s (paused)", S["unit"], expect_visible=True)
        # Paused, the refresh timer is frozen, so a forced fill stays for the picture.
        bar_of(S["unit"])[1].set_percent(0.6)
        screenshot("progress_on", S["unit"])
    elif phase == "on_shot" and elapsed > 2.0:
        S["comp"].set_resource_conversion_enabled(False)
        S["phase"], S["mark"] = "off", S["t"]
    elif phase == "off" and elapsed > 0.5:
        S["phase"] = "off_b"
        read("off +0.5s", S["unit"], expect_visible=False)
        screenshot("progress_off", S["unit"])
    elif phase == "off_b" and elapsed > 1.5:
        if S["control"] is not None:
            read("control unit without converter", S["control"], expect_visible=False)
        for converter in unreal.GameplayStatics.get_all_actors_of_class(game_world(), unreal.Actor):
            if converter.get_class().get_name() == "Ger_Ref_Converter_C":
                read("level-placed (not built) Ger_Ref_Converter", converter, expect_visible=False)
        S["comp"].set_resource_conversion_enabled(True)
        S["phase"], S["mark"] = "on_again", S["t"]
    elif phase == "on_again" and elapsed > 0.5:
        read("on again +0.5s (paused, restarts at 0)", S["unit"], expect_visible=True)
        unreal.GameplayStatics.set_game_paused(game_world(), False)
        log("paused after unpause: %s" % unreal.GameplayStatics.is_game_paused(game_world()))
        S["phase"], S["mark"] = "fill_a", S["t"]
    elif phase == "fill_a" and elapsed > 0.6:
        read("fill unpaused +0.6s", S["unit"], expect_visible=True)
        S["phase"] = "fill_b"
    elif phase == "fill_b" and elapsed > 1.6:
        read("fill unpaused +1.6s", S["unit"], expect_visible=True)
        S["phase"] = "fill_c"
    elif phase == "fill_c" and elapsed > 2.7:
        read("fill unpaused +2.7s (near full)", S["unit"], expect_visible=True)
        S["phase"] = "fill_d"
    elif phase == "fill_d" and elapsed > 3.5:
        read("fill unpaused +3.5s (reset after the 3 s tick)", S["unit"], expect_visible=True)
        finish()


def tick(delta):
    S["t"] += delta
    phase = S["phase"]
    try:
        tick_pending_capture()
        run_phase(phase, S["t"] - S["mark"])
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log("ERROR %s: %r" % (phase, error))
        finish()


unreal.register_slate_post_tick_callback(tick)
log("script loaded")
