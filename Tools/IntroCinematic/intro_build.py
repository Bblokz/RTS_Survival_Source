"""Builds the data side of the 1976 intro cinematic inside the throwaway editor (intro_editor_server.py):
voice waves, transcript and cue tables, the master sequence with one subsequence per shot.

Every function can be re-run. Nothing here saves outside the cinematic folders (intro_lib.safe_save).
The shots themselves (cameras, set actors) come from intro_manifest.py.
"""
import importlib
import json
import os

import unreal

import intro_lib as lib
import intro_narration as narration

importlib.reload(lib)
importlib.reload(narration)

FPS = 30
SEQ_DIR = lib.ROOT + "/Sequences"
MASTER = lib.ROOT + "/LS_Intro1976_Master"
DATA_DIR = lib.ROOT + "/Data"
CUES_TABLE = DATA_DIR + "/DT_IntroCues"
TRANSCRIPTS_TABLE = DATA_DIR + "/DT_IntroTranscripts"
VO_JSON = narration.vo_json()
TIMELINE_JSON = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/timeline.json"

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def frames(seconds):
    return int(round(seconds * FPS))


def load_vo():
    with open(VO_JSON, "r", encoding="utf-8") as handle:
        return json.load(handle)


# ---------------------------------------------------------------------------------------------------------------------
# Voice
# ---------------------------------------------------------------------------------------------------------------------
def import_vo():
    """Imports the scratch waves as SoundWaves (replacing older imports)."""
    vo = load_vo()
    tasks = []
    for row_id, info in vo.items():
        task = unreal.AssetImportTask()
        task.filename = info["wave"]
        task.destination_path = narration.VO_ASSET_DIR
        task.destination_name = narration.wave_name(row_id)
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    packages = []
    for row_id in vo:
        path = "%s/%s" % (narration.VO_ASSET_DIR, narration.wave_name(row_id))
        wave = unreal.load_asset(path)
        if wave is None:
            raise RuntimeError("voice import failed: " + path)
        packages.append(wave.get_outermost())
    lib.safe_save(packages)
    print("import_vo", len(packages))


def vo_asset_path(row_id):
    name = narration.wave_name(row_id)
    return "%s/%s.%s" % (narration.VO_ASSET_DIR, name, name)


def _table(path, struct_path):
    table = unreal.load_asset(path)
    if table is None:
        factory = unreal.DataTableFactory()
        factory.struct = unreal.load_object(None, struct_path)
        if factory.struct is None:
            raise RuntimeError("row struct missing (DLL too old?): " + struct_path)
        folder, name = path.rsplit("/", 1)
        table = tools.create_asset(name, folder, unreal.DataTable, factory)
    return table


def build_transcripts():
    """DT_IntroTranscripts: word-timed subtitles of the scratch waves (rows: FCinematicTranscriptRow)."""
    vo = load_vo()
    rows = []
    for row_id, info in vo.items():
        sentences = [{"Words": [{"Text": w["text"], "StartSeconds": w["start"], "EndSeconds": w["end"]} for w in s]}
                     for s in info["sentences"]]
        rows.append({"Name": narration.wave_name(row_id), "VoiceLine": vo_asset_path(row_id),
                     "SpeakerOverride": "", "Sentences": sentences})
    table = _table(TRANSCRIPTS_TABLE, "/Script/RTS_Survival.CinematicTranscriptRow")
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows))
    names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    print("build_transcripts fill", ok, "rows", len(names))
    lib.safe_save([table.get_outermost()])


def build_cues(cues):
    """DT_IntroCues from a list of dicts: type, start, end, l1, l2, pos, angle, param."""
    rows = []
    for index, cue in enumerate(cues):
        pos = cue.get("pos", (0.5, 0.5))
        rows.append({"Name": "Cue_%03d_%s" % (index, cue["type"]), "Type": cue["type"],
                     "StartSeconds": round(cue["start"], 3), "EndSeconds": round(cue["end"], 3),
                     "Line1": cue.get("l1", ""), "Line2": cue.get("l2", ""),
                     "Position": {"X": pos[0], "Y": pos[1]}, "AngleDegrees": cue.get("angle", 0.0),
                     "Param": cue.get("param", 0.0)})
    table = _table(CUES_TABLE, "/Script/RTS_Survival.IntroCinematicCueRow")
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows))
    names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    print("build_cues fill", ok, "rows", len(names))
    lib.safe_save([table.get_outermost()])


# ---------------------------------------------------------------------------------------------------------------------
# UI material: the archival notice burning away from the centre
# ---------------------------------------------------------------------------------------------------------------------
BURN_CODE = r"""
float2 p = (UV - 0.5) * float2(1.7778, 1.0);
float2 q = p * 7.0;
float2 i = floor(q); float2 f = frac(q); f = f * f * (3.0 - 2.0 * f);
#define H(v) frac(sin(dot(v, float2(127.1, 311.7))) * 43758.5453)
float n = lerp(lerp(H(i), H(i + float2(1, 0)), f.x), lerp(H(i + float2(0, 1)), H(i + float2(1, 1)), f.x), f.y);
float2 q2 = p * 23.0; float2 i2 = floor(q2); float2 f2 = frac(q2); f2 = f2 * f2 * (3.0 - 2.0 * f2);
float n2 = lerp(lerp(H(i2), H(i2 + float2(1, 0)), f2.x), lerp(H(i2 + float2(0, 1)), H(i2 + float2(1, 1)), f2.x), f2.y);
float edge = length(p) + (n - 0.5) * 0.30 + (n2 - 0.5) * 0.07;
float radius = Burn * 1.45 - 0.16;
float hole = smoothstep(radius - 0.012, radius + 0.012, edge);
float rim = saturate(1.0 - abs(edge - radius) / 0.055) * step(0.001, Burn);
float char = saturate(1.0 - (edge - radius) / 0.16) * hole;
float3 glow = float3(1.0, 0.42, 0.08) * rim * 3.0 + float3(0.10, 0.045, 0.02) * char;
return float4(glow, saturate(max(hole, rim)));
"""


def build_burn_material():
    path = lib.ROOT + "/UI/M_Intro_NoticeBurn"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mel = unreal.MaterialEditingLibrary
    material = tools.create_asset("M_Intro_NoticeBurn", lib.ROOT + "/UI", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    burn = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 200)
    burn.set_editor_property("parameter_name", "Burn")
    burn.set_editor_property("default_value", 0.0)
    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -550, 0)
    custom.set_editor_property("code", BURN_CODE)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    in_uv = unreal.CustomInput()
    in_uv.set_editor_property("input_name", "UV")
    in_burn = unreal.CustomInput()
    in_burn.set_editor_property("input_name", "Burn")
    custom.set_editor_property("inputs", [in_uv, in_burn])
    mel.connect_material_expressions(uv, "", custom, "UV")
    mel.connect_material_expressions(burn, "", custom, "Burn")
    rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -250, -60)
    for channel, on in (("r", True), ("g", True), ("b", True), ("a", False)):
        rgb.set_editor_property(channel, on)
    alpha = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -250, 120)
    for channel, on in (("r", False), ("g", False), ("b", False), ("a", True)):
        alpha.set_editor_property(channel, on)
    mel.connect_material_expressions(custom, "", rgb, "")
    mel.connect_material_expressions(custom, "", alpha, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    lib.safe_save([material.get_outermost()])
    print("build_burn_material ok")


# ---------------------------------------------------------------------------------------------------------------------
# Sequences
# ---------------------------------------------------------------------------------------------------------------------
def new_sequence(path, seconds):
    """A cleared level sequence at this path. An existing asset is emptied and reused: deleting a sequence that the
    map's sequence actor or the master still references fails and hangs the editor."""
    folder, name = path.rsplit("/", 1)
    sequence = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if sequence is None:
        sequence = tools.create_asset(name, folder, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    else:
        for track in sequence.get_tracks():
            sequence.remove_track(track)
        for binding in list(sequence.get_spawnables()) + list(sequence.get_possessables()):
            binding.remove()
    sequence.set_display_rate(unreal.FrameRate(FPS, 1))
    sequence.set_playback_start(0)
    sequence.set_playback_end(frames(seconds))
    return sequence


def add_camera(sequence, keys, seconds, focal_length=30.0, aperture=4.0, linear=False):
    """Spawnable cine camera with a camera cut over the whole sequence.
    keys: list of (seconds, (x, y, z), (pitch, yaw, roll)). Focus is off (everything sharp) unless a shot sets it."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # A spawnable's template has no reachable components from python, so configure a real actor and copy it in.
    instance = actors.spawn_actor_from_class(unreal.CineCameraActor, unreal.Vector(0, 0, -200000))
    component = instance.get_cine_camera_component()
    focus = component.get_editor_property("focus_settings")
    focus.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE)
    component.set_editor_property("focus_settings", focus)
    component.set_editor_property("current_focal_length", focal_length)
    component.set_editor_property("current_aperture", aperture)
    camera = sequence.add_spawnable_from_instance(instance)
    actors.destroy_actor(instance)
    camera.set_display_name("Cam")
    # Only the transform track goes: the spawn track must stay, or the camera never exists.
    for old in camera.get_tracks():
        if isinstance(old, unreal.MovieScene3DTransformTrack):
            camera.remove_track(old)
    track = camera.add_track(unreal.MovieScene3DTransformTrack)
    section = track.add_section()
    section.set_range(0, frames(seconds))
    channels = section.get_all_channels()
    for at, location, rotation in keys:
        frame = unreal.FrameNumber(frames(at))
        values = [location[0], location[1], location[2], rotation[2], rotation[0], rotation[1]]
        for channel, value in zip(channels[:6], values):
            channel.add_key(frame, value, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR if linear
                            else unreal.MovieSceneKeyInterpolation.AUTO)
    cut_track = sequence.add_track(unreal.MovieSceneCameraCutTrack)
    cut = cut_track.add_section()
    cut.set_range(0, frames(seconds))
    # The engine builds the binding ID: a hand-made one (guid only) resolves in the editor but not in a game run.
    cut.set_camera_binding_id(sequence.get_binding_id(camera))
    return camera


def add_level_visibility(sequence, level_names, seconds):
    if not level_names:
        return
    track = sequence.add_track(unreal.MovieSceneLevelVisibilityTrack)
    section = track.add_section()
    section.set_range(0, frames(seconds))
    section.set_visibility(unreal.LevelVisibility.VISIBLE)
    section.set_level_names([unreal.Name(n) for n in level_names])


def layout(shots):
    """Places the shots and their voice rows on the master clock.
    A shot lasts pre + voice rows (with gap between) + post, or at least 'min' seconds."""
    vo = load_vo()
    cursor = 0.0
    timeline = []
    for shot in shots:
        pre, post, gap = shot.get("pre", 0.4), shot.get("post", 0.6), shot.get("gap", 0.45)
        voice_cursor = pre
        rows = []
        for row_id in shot.get("vo", []):
            rows.append({"row": row_id, "start": round(cursor + voice_cursor, 3), "duration": vo[row_id]["duration"]})
            voice_cursor += vo[row_id]["duration"] + gap
        length = (voice_cursor - gap + post) if rows else 0.0
        if rows and shot.get("cut_vo"):
            # The shot ends part-way through its voice; the line runs on over the next shot.
            length = pre + (voice_cursor - gap - pre) * shot["cut_vo"]
        length = max(length, shot.get("min", 0.0))
        # Whole frames, so shot ends land on frame boundaries.
        length = frames(length) / float(FPS)
        timeline.append({"id": shot["id"], "start": round(cursor, 4), "duration": length, "vo": rows})
        cursor += length
    return timeline, cursor


def build_master(shots, shot_builder):
    """One subsequence per shot (built by shot_builder(sequence, shot, seconds)) plus the master with the shot
    track, the voice tracks and the fades. Returns the timeline (also written to Saved/IntroCinematic/timeline.json)."""
    timeline, total = layout(shots)
    master = new_sequence(MASTER, total)
    shot_track = master.add_track(unreal.MovieSceneCinematicShotTrack)
    voice_track = master.add_track(unreal.MovieSceneAudioTrack)
    voice_track.set_display_name("Narrator")
    packages = [master.get_outermost()]
    for shot, placed in zip(shots, timeline):
        seconds = placed["duration"]
        sub_path = "%s/LS_Intro_%s" % (SEQ_DIR, shot["id"].replace(".", "_"))
        sub = new_sequence(sub_path, seconds)
        add_level_visibility(sub, shot.get("levels", []), seconds)
        shot_builder(sub, shot, seconds)
        packages.append(sub.get_outermost())
        section = shot_track.add_section()
        section.set_sequence(sub)
        section.set_range(frames(placed["start"]), frames(placed["start"] + seconds))
        section.set_shot_display_name(shot["id"])
        for index, row in enumerate(placed["vo"]):
            audio = voice_track.add_section()
            audio.set_sound(unreal.load_asset(vo_asset_path(row["row"])))
            audio.set_range(frames(row["start"]), frames(row["start"] + row["duration"]))
            audio.set_row_index(index % 2)
    fades = [(placed["start"], shot.get("fade_in", 0.0), placed["start"] + placed["duration"], shot.get("fade_out", 0.0))
             for shot, placed in zip(shots, timeline)]
    add_fades(master, fades, total)
    lib.safe_save(packages)
    with open(TIMELINE_JSON, "w", encoding="utf-8") as handle:
        json.dump({"total": total, "fps": FPS, "shots": timeline}, handle, indent=1)
    print("build_master %d shots, %.1f s" % (len(shots), total))
    return timeline, total


def add_fades(master, fades, total):
    """Fade track: 1 = black. fades: (shot start, fade-in seconds, shot end, fade-out seconds)."""
    keys = []
    for start, fade_in, end, fade_out in fades:
        if fade_in > 0.0:
            keys += [(start, 1.0), (start + fade_in, 0.0)]
        if fade_out > 0.0:
            keys += [(end - fade_out, 0.0), (end - 1.0 / FPS, 1.0)]
    if not keys:
        return
    track = master.add_track(unreal.MovieSceneFadeTrack)
    section = track.add_section()
    section.set_range(0, frames(total))
    channel = section.get_all_channels()[0]
    for at, value in sorted(keys):
        channel.add_key(unreal.FrameNumber(frames(at)), value, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)


# ---------------------------------------------------------------------------------------------------------------------
# Map
# ---------------------------------------------------------------------------------------------------------------------
def setup_map():
    """Game mode override and the autoplay sequence actor of the cinematic map (the map must be the open level)."""
    world = lib.editor_world()
    if world.get_outermost().get_name() != lib.MAP:
        raise RuntimeError("open the cinematic map first, world is " + world.get_outermost().get_name())
    lib.make_persistent_current()
    game_mode = unreal.load_class(None, "/Script/RTS_Survival.IntroCinematicGameMode")
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    persistent_prefix = lib.MAP + "."
    sequence_actor = None
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.LevelSequenceActor) and actor.get_path_name().startswith(persistent_prefix):
            sequence_actor = actor
    if sequence_actor is None:
        sequence_actor = actors.spawn_actor_from_class(unreal.LevelSequenceActor, unreal.Vector(0, 0, 0))
        if not sequence_actor.get_path_name().startswith(persistent_prefix):
            actors.destroy_actor(sequence_actor)
            raise RuntimeError("sequence actor did not land in the cinematic map")
    sequence_actor.set_actor_label("IntroMasterSequence")
    sequence_actor.set_sequence(unreal.load_asset(MASTER))
    settings = sequence_actor.get_editor_property("playback_settings")
    settings.set_editor_property("auto_play", True)
    sequence_actor.set_editor_property("playback_settings", settings)
    # The two grades: one unbound post process volume with M_Intro_Grade, steered by MPC_Intro.Archive.
    volume = None
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.PostProcessVolume) and actor.get_path_name().startswith(persistent_prefix):
            volume = actor
    if volume is None:
        volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    volume.set_actor_label("IntroGrade")
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    grade = unreal.load_asset(lib.ROOT + "/Materials/M_Intro_Grade")
    blendables = unreal.WeightedBlendables()
    blendables.set_editor_property("array", [unreal.WeightedBlendable(1.0, grade)])
    settings.set_editor_property("weighted_blendables", blendables)
    # Exposure follows each set at once (no pumping after a cut) and may go as dark as a lamp-lit bunker.
    settings.set_editor_property("override_auto_exposure_method", False)
    settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM)
    # No brightness range override: each map keeps its own exposure (the game look), the sets are lit to match.
    settings.set_editor_property("override_auto_exposure_min_brightness", False)
    settings.set_editor_property("override_auto_exposure_max_brightness", False)
    settings.set_editor_property("override_auto_exposure_speed_up", True)
    settings.set_editor_property("auto_exposure_speed_up", 20.0)
    settings.set_editor_property("override_auto_exposure_speed_down", True)
    settings.set_editor_property("auto_exposure_speed_down", 20.0)
    volume.set_editor_property("priority", -1.0)
    settings.set_editor_property("override_motion_blur_amount", True)
    settings.set_editor_property("motion_blur_amount", 0.25)
    volume.set_editor_property("settings", settings)
    print("setup_map game mode", world.get_world_settings().get_editor_property("default_game_mode"),
          "actor", sequence_actor.get_path_name())
    lib.save_map()
    print("foreign dirty (never saved):", lib.foreign_dirty())


def build_render_preset(name, width, height, game_mode=True, fps=None, frame_range=None):
    path = lib.ROOT + "/Render/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    config = tools.create_asset(name, lib.ROOT + "/Render", unreal.MoviePipelinePrimaryConfig, None)
    output = config.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    output.output_directory = unreal.DirectoryPath("D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/Render/" + name)
    output.file_name_format = "f.{frame_number}"
    output.output_resolution = unreal.IntPoint(width, height)
    output.override_existing_output = True
    output.zero_pad_frame_numbers = 5
    if frame_range:
        # Re-render of a few shots only; frame numbers stay those of the whole film.
        output.use_custom_playback_range = True
        output.custom_start_frame = frame_range[0]
        output.custom_end_frame = frame_range[1]
    if fps:
        # Previews: fewer frames of the same film (the sequence itself stays at 30 fps).
        output.use_custom_frame_rate = True
        output.output_frame_rate = unreal.FrameRate(fps, 1)
    config.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    config.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_JPG)
    widgets = config.find_or_add_setting_by_class(unreal.MoviePipelineWidgetRenderer)
    widgets.composite_onto_final_image = True
    anti_aliasing = config.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
    anti_aliasing.engine_warm_up_count = 30
    anti_aliasing.render_warm_up_count = 30
    anti_aliasing.render_warm_up_frames = True
    if game_mode:
        override = config.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
        override.game_mode_override = unreal.load_class(None, "/Script/RTS_Survival.IntroCinematicGameMode")
    lib.safe_save([config.get_outermost()])
    print("build_render_preset", path)
