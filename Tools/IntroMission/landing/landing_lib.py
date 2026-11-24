"""Helpers for the Ger_IntroMission rear landing field (exec'd in the throwaway editor through landing_cmd.py).
Re-uses rear_lib.py (spawn/trace/shots) with its own root, folder and prefix. Local frame (u, v) = depot slab frame:
centre (18300, -15700), yaw 18; u along the slab's 72 m axis, v along its 48 m axis (+v = road D side)."""
import math
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/rear/rear_lib.py').read())
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroLanding/'
SHOTS = ROOT + 'shots/'
FOLDER = 'IntroRear/LandingField/'
PREFIX = 'LAND_'
os.makedirs(SHOTS, exist_ok=True)
CX, CY, YAW = 18300.0, -15700.0, 18.0
_A = math.radians(YAW)


def W(u, v):
    return CX + u * math.cos(_A) - v * math.sin(_A), CY + u * math.sin(_A) + v * math.cos(_A)


def L(x, y):
    dx, dy = x - CX, y - CY
    return dx * math.cos(_A) + dy * math.sin(_A), -dx * math.sin(_A) + dy * math.cos(_A)


def ground_hit(x, y, ignore=None, top=20000.0):
    """Trace straight down; returns (z, actor) of the first blocking hit, ignoring the given actors."""
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, top), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, list(ignore or []),
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None, None
    t = hit.to_tuple()
    return t[4].z, t[9]


def landscape_z(x, y):
    """Z of the landscape only (every other actor that blocks is skipped by re-tracing)."""
    ignore = []
    for _ in range(8):
        z, a = ground_hit(x, y, ignore)
        if a is None:
            return None
        if isinstance(a, unreal.Landscape) or isinstance(a, unreal.LandscapeProxy):
            return z
        ignore.append(a)
    return None
