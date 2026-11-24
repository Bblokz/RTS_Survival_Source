import importlib, sys, time
sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
import ore_map_build as omb
importlib.reload(omb)
omb.load_layout()
mel = unreal.MaterialEditingLibrary
mi = unreal.load_asset("/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Materials/MI_OreCliff")
print("scalars", {str(n): round(mel.get_material_instance_scalar_parameter_value(mi, n), 3) for n in mel.get_scalar_parameter_names(mi)})
print("vectors", {str(n): [round(v, 3) for v in mel.get_material_instance_vector_parameter_value(mi, n).to_tuple()] for n in mel.get_vector_parameter_names(mi)})
# 2026-11-13: in game the rock read near white and bluish against the warm ground; warmer, darker, dustier





unreal.SystemLibrary.execute_console_command(omb.world(), "t.IdleWhenNotForeground 0")
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les.editor_request_begin_play()
_s = {"n": 0, "t": time.time(), "stage": 1, "i": 0}
OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/shots/"
# (name, x, y, yaw): camera pawn position and facing for an in-game screenshot with the fog of war material off
SPOTS = [("ig3_escarp_face", -6500.0, 2500.0, 180.0), ("ig3_escarp_face_e", -8500.0, 14500.0, 180.0),
                                    ("ig3_crest_face", -22600.0, 8400.0, 180.0), ("ig3_crest_face_w", -22300.0, -6000.0, 180.0), ("ig3_escarp_lip", -11200.0, 2500.0, 0.0),
                                    ("ig3_knoll_south", 8800.0, -6000.0, 0.0), ("ig3_knoll_north", 15800.0, -6000.0, 180.0)]


def _defer():
    _s["n"] += 1
    now = time.time()
    gw = ues.get_game_world()
    if gw is None:
        return now - _s["t"] > 120
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == 1 and game_t > 10.0:
        unreal.SystemLibrary.execute_console_command(gw, "ShowFlag.PostProcessMaterial 0")
        _s["stage"], _s["mark"], _s["moved"] = 2, game_t, False
        return False
    if _s["stage"] == 2:
        i = _s["i"]
        if i >= len(SPOTS):
            les.editor_request_end_play()
            return True
        name, x, y, yaw = SPOTS[i]
        if not _s["moved"]:
            pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
            pawn.set_actor_location(unreal.Vector(x, y, omb.ground_z(x, y) + 43.0), False, False)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
            _s["moved"], _s["mark"] = True, game_t
            return False
        if game_t - _s["mark"] > 3.0 and not _s.get("shot"):
            cam = unreal.GameplayStatics.get_player_camera_manager(gw, 0)
            r = cam.get_camera_rotation()
            print(name, "cam yaw %.0f pitch %.0f" % (r.yaw, r.pitch))
            unreal.SystemLibrary.execute_console_command(gw, "HighResShot 1600x900 filename=" + OUT + name + ".png")
            _s["shot"], _s["mark"] = True, game_t
            return False
        if _s.get("shot") and game_t - _s["mark"] > 1.5:
            _s["i"] += 1
            _s["moved"], _s["shot"] = False, False
    return False
