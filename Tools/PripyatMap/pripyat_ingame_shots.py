"""In-game screenshots of Ger_PripyatMe410 from the RTS camera (play session in the throwaway editor, fog of war
material off). Set SPOTS = [(name, x, y, yaw), ...] and TAG before this body runs through pripyat_cmd.py; the shots go
to Saved/Pripyat/shots/<TAG>/<name>.png. Scene captures of this map come out near black, so judge looks from these."""
import importlib, os, sys, time
import unreal
sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap")
import pripyat_map_build as pmb
importlib.reload(pmb)
pmb.load_layout()
_TAG = globals().get("TAG", "ig")
_SPOTS = list(globals().get("SPOTS", [("start", -12000.0, -31500.0, 0.0)]))
_OUT = "D:/UE5Projects/RTS_Survival/Saved/Pripyat/shots/" + _TAG + "/"
os.makedirs(_OUT, exist_ok=True)
unreal.SystemLibrary.execute_console_command(pmb.world(), "t.IdleWhenNotForeground 0")
_les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
_les.editor_request_begin_play()
_s = {"n": 0, "t": time.time(), "stage": 1, "i": 0}


def _defer():
    _s["n"] += 1
    now = time.time()
    gw = _ues.get_game_world()
    if gw is None:
        return now - _s["t"] > 180
    if unreal.GameplayStatics.is_game_paused(gw) and _s["n"] % 50 == 0:
        unreal.GameplayStatics.set_game_paused(gw, False)
    game_t = unreal.GameplayStatics.get_time_seconds(gw)
    if _s["stage"] == 1 and game_t > 10.0:
        unreal.SystemLibrary.execute_console_command(gw, "ShowFlag.PostProcessMaterial 0")
        _s["stage"], _s["mark"], _s["moved"] = 2, game_t, False
        return False
    if _s["stage"] == 2:
        i = _s["i"]
        if i >= len(_SPOTS):
            _les.editor_request_end_play()
            return True
        name, x, y, yaw = _SPOTS[i]
        if not _s["moved"]:
            pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
            pawn.set_actor_location(unreal.Vector(x, y, pmb.ground_z(x, y) + 43.0), False, False)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
            _s["moved"], _s["mark"] = True, game_t
            return False
        if game_t - _s["mark"] > 3.0 and not _s.get("shot"):
            unreal.SystemLibrary.execute_console_command(gw, "HighResShot 1600x900 filename=" + _OUT + name + ".png")
            _s["shot"], _s["mark"] = True, game_t
            return False
        if _s.get("shot") and game_t - _s["mark"] > 1.5:
            _s["i"] += 1
            _s["moved"], _s["shot"] = False, False
    return False
