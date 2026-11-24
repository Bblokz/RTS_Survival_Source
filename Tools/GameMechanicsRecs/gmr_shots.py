"""Camera grammar and shared presets for GameMechanicsRecs jobs.

Every scene in gmr_jobs.py is built from these helpers, so a camera rule the critic asks for (slower orbits,
lower hero angles, longer holds...) is changed once here and reaches every recording on the next generation.

Conventions (see README.md): positions are in the job's anchor frame (X forward along AnchorYaw, Y right, Z up,
centimetres). A key's Yaw is the compass direction from its pivot to the camera (0 = camera in front looking back
along -X, 180 = camera behind looking forward), Pitch is the elevation above the pivot (positive looks down).
"""

# --- Style constants: the house look of every recording -------------------------------------------------------------
STYLE = {
    "ease": "InOut",
    "pivot_lag": 0.7,          # s, spring half-life on tracked pivots (units dying or turning never jerk the frame)
    "aperture_hero": 4.0,      # f-stop on close hero shots (mild depth of field)
    "aperture_wide": 0.0,      # wide shots are sharp front to back
    "clearance": 350.0,        # cm the camera keeps above the ground / roofs
    "look_height": 150.0,      # cm above the pivot the camera looks at (hull centre, not the tracks)
}


def key(t, pivot="", yaw=180.0, pitch=22.0, dist=3000.0, focal=35.0, off=(0, 0, 0), look=None, roll=0.0,
        vyaw=0.0, vpitch=0.0, vdist=0.0, vfocal=0.0, vlook=None):
    """One camera key. look = (x, y, z) LookOffset in the anchor frame; default looks at hull height."""
    if look is None:
        look = (0, 0, STYLE["look_height"])
    return {
        "Time": float(t), "Pivot": pivot, "Offset": vec(off), "Yaw": float(yaw), "Pitch": float(pitch),
        "Distance": float(dist), "Focal": float(focal), "Roll": float(roll), "LookOffset": vec(look),
        "VYaw": float(vyaw), "VPitch": float(vpitch), "VDistance": float(vdist), "VFocal": float(vfocal),
        "VLookOffset": vec(vlook if vlook is not None else (0, 0, 0)),
    }


def vec(v):
    return {"X": float(v[0]), "Y": float(v[1]), "Z": float(v[2])}


def shot(name, start, end, rig, keys, ease=None, lag=None, aperture=None, shake=0.0, clearance=None, occlusion=True):
    return {
        "Name": name, "Start": float(start), "End": float(end), "Rig": rig, "Keys": keys,
        "Ease": ease or STYLE["ease"],
        "PivotLag": float(STYLE["pivot_lag"] if lag is None else lag),
        "Aperture": float(STYLE["aperture_wide"] if aperture is None else aperture),
        "Shake": float(shake), "bAvoidOcclusion": bool(occlusion),
        "MinGroundClearance": float(STYLE["clearance"] if clearance is None else clearance),
    }


# --- Shot templates ------------------------------------------------------------------------------------------------

def orbit(name, start, end, pivot, yaw0, yaw1, pitch0=18, pitch1=None, dist0=2600, dist1=None, focal0=32,
          focal1=None, off=(0, 0, 0), look=None, aperture=None, shake=0.0, occlusion=True, clearance=None, **kw):
    """Slow arc around a subject: the classic hero move."""
    d = end - start
    return shot(name, start, end, "Orbit", [
        key(0, pivot, yaw0, pitch0, dist0, focal0, off, look, **_v(kw, 0)),
        key(d, pivot, yaw1, pitch0 if pitch1 is None else pitch1, dist0 if dist1 is None else dist1,
            focal0 if focal1 is None else focal1, off, look, **_v(kw, 1)),
    ], aperture=aperture, shake=shake, occlusion=occlusion, clearance=clearance)


def track(name, start, end, pivot0, pivot1, yaw, pitch=20, dist=3200, focal=30, off0=(0, 0, 0), off1=(0, 0, 0),
          look=None, aperture=None, shake=0.0, yaw1=None, **kw):
    """Lateral dolly: the camera travels with its pivot from one subject to the next, angle kept."""
    d = end - start
    return shot(name, start, end, "Orbit", [
        key(0, pivot0, yaw, pitch, dist, focal, off0, look, **_v(kw, 0)),
        key(d, pivot1, yaw if yaw1 is None else yaw1, pitch, dist, focal, off1, look, **_v(kw, 1)),
    ], aperture=aperture, shake=shake)


def pan(name, start, end, pivot0, pivot1, yaw, pitch=20, dist=3500, focal=30, off0=(0, 0, 0), off1=(0, 0, 0),
        look=None, shake=0.0, **kw):
    """Locked-off camera that pans from one subject to another (camera placed by the first key)."""
    d = end - start
    return shot(name, start, end, "Fixed", [
        key(0, pivot0, yaw, pitch, dist, focal, off0, look, **_v(kw, 0)),
        key(d, pivot1, yaw, pitch, dist, focal, off1, look, **_v(kw, 1)),
    ], shake=shake)


def sweep(name, start, end, pivot0, pivot1, camera_offset, focal=35, camera_pivot="", look=None, vcamera_offset=None,
          vfocal=0.0, off0=(0, 0, 0), off1=(0, 0, 0), extra_pivots=(), ease=None, lag=None):
    """The camera stands at one spot (camera_offset from camera_pivot, z = height above ground) and pans from one
    subject over others to the last: the 'camera pans over the base' move. extra_pivots = intermediate subjects."""
    d = end - start
    pivots = [pivot0] + list(extra_pivots) + [pivot1]
    keys = []
    for index, pivot in enumerate(pivots):
        t = d * index / (len(pivots) - 1)
        off = off0 if index == 0 else off1 if index == len(pivots) - 1 else (0, 0, 0)
        keys.append(key(t, pivot, 180, 20, 3000, focal, off, look, vfocal=vfocal))
    s = shot(name, start, end, "Fixed", keys, ease=ease, lag=lag)
    s["CameraPivot"] = camera_pivot
    s["CameraOffset"] = vec(camera_offset)
    if vcamera_offset is not None:
        s["VCameraOffset"] = vec(vcamera_offset)
    return s


def crane(name, start, end, pivot, yaw0, pitch0, pitch1, dist0, dist1, focal0=28, focal1=None, yaw1=None,
          off=(0, 0, 0), look=None, **kw):
    """Rise or descend over the subject, the establishing / reveal move."""
    d = end - start
    return shot(name, start, end, "Orbit", [
        key(0, pivot, yaw0, pitch0, dist0, focal0, off, look, **_v(kw, 0)),
        key(d, pivot, yaw0 if yaw1 is None else yaw1, pitch1, dist1, focal0 if focal1 is None else focal1, off, look,
            **_v(kw, 1)),
    ])


def follow(name, start, end, pivot, yaw_rel0, yaw_rel1=None, pitch=14, dist=2600, focal=32, look=None, lag=1.0,
           aperture=None, shake=0.0, **kw):
    """Chase / tracking shot; yaw is relative to the subject's heading (180 = behind, 90 = right side)."""
    d = end - start
    return shot(name, start, end, "Follow", [
        key(0, pivot, yaw_rel0, pitch, dist, focal, (0, 0, 0), look, **_v(kw, 0)),
        key(d, pivot, yaw_rel0 if yaw_rel1 is None else yaw_rel1, pitch, dist, focal, (0, 0, 0), look, **_v(kw, 1)),
    ], lag=lag, aperture=aperture, shake=shake)


def push(name, start, end, pivot, yaw, pitch0, dist0, dist1, focal0=30, focal1=None, pitch1=None, look=None,
         aperture=None, occlusion=True, **kw):
    """Slow push-in toward the subject."""
    d = end - start
    return shot(name, start, end, "Orbit", [
        key(0, pivot, yaw, pitch0, dist0, focal0, (0, 0, 0), look, **_v(kw, 0)),
        key(d, pivot, yaw, pitch0 if pitch1 is None else pitch1, dist1, focal0 if focal1 is None else focal1,
            (0, 0, 0), look, **_v(kw, 1)),
    ], aperture=aperture, occlusion=occlusion)


def _v(kw, index):
    """Pass per-key 9:16 overrides: vpitch=(a, b) or vpitch=a for both keys."""
    out = {}
    for name in ("vyaw", "vpitch", "vdist", "vfocal", "vlook"):
        if name in kw:
            value = kw[name]
            # vlook=(x, y, z) is one vector for both keys; vlook=((..), (..)) gives one per key.
            per_key = isinstance(value, (list, tuple)) and not (name == "vlook" and not isinstance(value[0], (list, tuple)))
            out[name] = value[index] if per_key else value
    return out


# --- Maps -------------------------------------------------------------------------------------------------------------

GAME_MODE = "/Game/RTS_Survival/Blueprints/GameSettings/RTS_BP_GamemodeBase.RTS_BP_GamemodeBase_C"
SURVIVAL_GAME_MODE = "/Game/RTS_Survival/SurvivalMode/Blueprints/BP_SurvivalGameMode.BP_SurvivalGameMode_C"

MAPS = {
    "Berlin": {"MapPath": "/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin", "GameModeClassPath": GAME_MODE},
    "Ore": {"MapPath": "/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains", "GameModeClassPath": GAME_MODE,
            "HideActorPrefixes": ["ORE_PH_"]},
    "Pripyat": {"MapPath": "/Game/RTS_Survival/Maps/GerMissions/Ger_PripyatMe410", "GameModeClassPath": GAME_MODE},
    "OderWarthe": {"MapPath": "/Game/RTS_Survival/Maps/GerMissions/Ger_OderWarthe", "GameModeClassPath": GAME_MODE},
    "Desert": {"MapPath": "/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01", "GameModeClassPath": SURVIVAL_GAME_MODE,
               "LaunchArgs": ["-SurvivalAutoStart", "-SurvivalGrace=3600"],
               "HideNear": [("BP_Electricity_SinglePole", 0)],
               "SuppressClassPrefixes": ["Ger_Aircraft_JU390B_AirborneUnarmed"]},
    "Kessel": {"MapPath": "/Game/RTS_Survival/SurvivalMode/Maps/SM_Kessel_01", "GameModeClassPath": SURVIVAL_GAME_MODE,
               "LaunchArgs": ["-SurvivalAutoStart", "-SurvivalGrace=3600"],
               # Debug-looking chrome spheres seen on the POI pass (survival spawn markers).
               "HideNear": [("BP_SurvivalEdgeSpawnZone", 60000), ("BP_SurvivalStartLocation", 60000)],
               # Survival drop ships crossed the 9:16 sky of the farmstead battle.
               "SuppressClassPrefixes": ["Ger_Aircraft_JU390B_AirborneUnarmed"]},
}


def base_job(name, category, map_key, anchor, anchor_yaw, duration, **extra):
    job = {
        "JobName": name, "Category": category, "Fps": 60, "WarmupSeconds": 6.0, "DurationSeconds": float(duration),
        "Anchor": vec(anchor), "AnchorYaw": float(anchor_yaw), "ClearUnitsRadius": 9000.0,
        "bKeepVoiceLines": True, "bKeepAnnouncer": False, "bKeepMusic": False,
        "Events": [], "Shots": [],
    }
    job.update(MAPS[map_key])
    job["HideActorPrefixes"] = list(MAPS[map_key].get("HideActorPrefixes", []))
    for prefix, radius in MAPS[map_key].get("HideNear", []):
        # Runtime-only declutter around the scene (power-line poles cutting through every desert shot).
        job["Events"].append(ev(-3.5, "Hide", Text=prefix, Radius=float(radius)))
    job.update(extra)
    return job


def ev(t, kind, tag="", **fields):
    """An event; Offset/TargetOffset given as (x, y) or (x, y, z) tuples in the anchor frame."""
    e = {"Time": float(t), "Type": kind, "Tag": tag}
    for name, value in fields.items():
        if name in ("Offset", "TargetOffset") and isinstance(value, (tuple, list)):
            value = vec(tuple(value) + (0,) * (3 - len(value)))
        e[name] = value
    return e
