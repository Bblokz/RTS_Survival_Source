# GameMechanicsRecs (GMR)

Headless recordings of game mechanics: a job JSON says which map, where, what happens (events) and how it is
filmed (shots). One job renders a 16:9 take and a 9:16 take with Movie Render Queue, with no editor UI, and
each take becomes a finished mp4.

| File | What it does |
|------|--------------|
| `gmr.py` | Renders one job (one or both aspects) and posts each take |
| `gmr_post.py` | Posts one take: cut, fades, loudness, H.264, contact sheet, manifest copy, log excerpt. The other scripts also import its shared helpers |
| `gmr_queue.py` | Runs many jobs one after another in one process |
| `gmr_review.py` | Turns finished mp4s into image strips + `review.md` for a camera critic that cannot watch video |
| `Jobs/` | Job files. `_Example_TankDrive.json` is a template |

Python 3.11 (`E:/Python11/python.exe`); `ffmpeg`/`ffprobe` must be on PATH. `psutil` is optional; without it the
scripts use PowerShell `Get-CimInstance` to find processes.

## The C++ side

The C++ lives in `Source/RTS_GameMechanicsRecs`. It is an editor-only module, and only the render process loads
it (`-ExecCmds=Module Load RTS_GameMechanicsRecs` together with `-GMRJob=<Job.json>`). It is never part of
packaged or shipping builds. The runner builds a transient level sequence and MRQ config, renders the job's map,
and spawns the director `AGMRDirector` when the map is loaded. The director runs the events, flies the camera,
records the main submix to `<JobName>_capture.wav`, writes `Manifest.json` and logs `GMR_JOB_DONE`.

**Recordings never modify maps.** Everything the director spawns, hides or destroys exists only in the render
process. Nothing is saved.

## Quick start

```
cd D:/UE5Projects/RTS_Survival/Source/Tools/GameMechanicsRecs

# 1. Create a job: copy Jobs/_Example_TankDrive.json to Jobs/<Name>.json and edit it (reference below).

# 2. Probe: half resolution, short, to check framing and timing. Output is <Name>_<aspect>_probe.mp4.
E:/Python11/python.exe gmr.py Jobs/<Name>.json --probe
E:/Python11/python.exe gmr.py Jobs/<Name>.json --probe --aspects 9x16 --seconds 6

# 3. Full run, both aspects (16x9 renders first):
E:/Python11/python.exe gmr.py Jobs/<Name>.json

# 4. Several jobs, one after another:
E:/Python11/python.exe gmr_queue.py Jobs/A.json Jobs/B.json
E:/Python11/python.exe gmr_queue.py --list tonight.txt --aspects 16x9

# 5. A review pack for a critic (images + review.md):
E:/Python11/python.exe gmr_review.py D:/UE5Projects/RTS_Survival/Saved/GameMechanicsRecs/<Category>/<Name>
```

### gmr.py

```
gmr.py Jobs/<Name>.json [--aspects 16x9,9x16] [--probe] [--development] [--no-post] [--keep-frames] [--seconds N]
```

- `--aspects`: `16x9` renders at 1920x1080 and `9x16` at 1080x1920. When both are given, 16x9 always renders first.
- `--probe`: renders at half size (960x540 / 540x960) under a separate `_probe` name, so a final is never overwritten.
- `--development`: uses `UnrealEditor-Cmd.exe`. The default is `UnrealEditor-Win64-DebugGame-Cmd.exe`, from
  `D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64`.
- `--no-post`: renders only and leaves the take in its work dir.
- `--keep-frames`: keeps the work dir (JPEG frames + WAV) after post, so you can run the cut again with
  `gmr_post.py --work <dir>`.
- `--seconds N`: overrides `DurationSeconds`, for quick tests.
- Exit code 0 only if every requested aspect produced its final mp4. With `--no-post`, 0 means every render
  succeeded and wrote its manifest.

Each take goes through these steps:

1. Wait for the render lock `Saved/GameMechanicsRecs/_gmr.lock`. It holds the owner's PID and counts as stale once
   that PID is gone.
2. Wait while any other `UnrealEditor*-Cmd.exe -RenderOffscreen` process runs, such as another session's render or a
   Shorts render. The script checks every 30 s and prints once a minute.
3. Delete `Saved/GameMechanicsRecs/_work/<JobName>_<aspect>[_probe]/` if it exists, then create it again.
4. Write the resolved job there as `Job.json` (UTF-8 without BOM, because the C++ reader rejects a BOM).
5. Launch the render and poll the tail of `render.log` every 5 s for `GMR_JOB_DONE`. `=== Critical error:` or
   `Fatal error` count as failure. The render is killed after 900 s without log growth, and after 3 h in any case.
   If the render shows the modal **"Missing target file"** window (another session rebuilt the binaries), the
   script closes it and keeps waiting.
6. If `Manifest.json` exists, post the take (unless `--no-post` was given).

Engine arguments come in this order: the fixed render arguments, then the job's `LaunchArgs`, then the environment
variable `GMR_EXTRA_ARGS`, which is split like a Windows command line. For example:
`set GMR_EXTRA_ARGS=-LogCmds="LogGMR Verbose"`.

### gmr_post.py

```
gmr_post.py --work <work dir> [--keep-frames]
```

Also importable as `post_take(work_dir, keep_frames=False) -> Path | None`. The function:

1. Cuts frames `[StartFrame, EndFrame)` from `LS_GMR_<JobName>.<NNNNN>.jpeg` (`.jpg` also works). If `EndFrame` is
   below 0, it cuts to the last frame present. Up to 3 missing frames are filled with a neighbouring frame; more
   than that fails the take.
2. Encodes video as H.264 High, CRF 16, preset slow, yuv420p, keyframe every 2 s, faststart. The full-range
   BT.601 JPEG colour is converted to limited-range BT.709 and tagged so. It fades in from black over 0.4 s and
   fades out to black over 0.6 s.
3. Normalises audio from `<JobName>_capture.wav` (time 0 = StartFrame) in two passes. The first pass measures
   loudness. The second applies linear gain to -14 LUFS (I=-14, TP=-1, LRA=11), then a 4x oversampled limiter at
   -1 dBTP. Output is AAC 256k, 48 kHz, stereo, with a 0.3 s fade in and a 0.6 s fade out. The audio is padded
   with silence to the video length, and the video is never shortened. If the WAV is missing or silent (mean
   volume below -70 dB), the take still gets a silent track and a WARNING.
4. Writes `<JobName>_<aspect>[_probe]_sheet.jpg`. The contact sheet has one tile per second, numbered: 6 columns
   of 480 px tiles for 16x9, or 8 columns of 270 px tiles for 9x16.
5. Copies `Manifest.json` to `<JobName>_<aspect>[_probe]_manifest.json`.
6. Writes `<JobName>_<aspect>[_probe].log`. It holds only the `GMR_` lines, LogGMR errors and warnings, and crash
   lines, up to 2000 lines.
7. Prints a short report: duration, loudness before and after, manifest warnings and `VoiceLinesHeard`.
8. Deletes the work dir unless `--keep-frames` is given.

The new mp4 is encoded to a temporary file. It replaces the previous one only after it has been encoded and its
frame count checked, so a failed post never destroys the last good version.

### gmr_queue.py

```
gmr_queue.py Jobs/A.json Jobs/B.json ... [--aspects ...] [--probe] [--development] [--no-post] [--keep-frames] [--seconds N]
gmr_queue.py --list jobs.txt [...]
```

`--list` takes a text file with one job path per line; lines starting with `#` are skipped. All jobs run in this
process, one after another. One failed job does not stop the queue. Between jobs, the queue reads these files in
the GMR root:

- `_queue.hold`: the queue waits while this file exists.
- `_queue.stop`: the queue stops before the next job and deletes the file.
- `_queue_status.json`: the queue rewrites it before and after every job. It holds the state, the current job,
  the done list (each entry with Result `ok`/`failed`/`error` and Seconds) and the remaining jobs.

### gmr_review.py

```
gmr_review.py <mp4 or folder> [--out DIR] [--every 0.5]
```

Takes one frame every `--every` seconds, from the middle of each interval, so the first tile is not the black
fade-in. Each tile shows its time in the mp4. The frames are tiled into 4 x 3 strip images: 640 px tiles for
16x9, 360 px tiles for 9x16. `review.md` lists, per job:

- the mp4 paths and durations
- the manifest shot list: Name, Rig, start/end seconds from the cut, OcclusionPitchBoost, FirstFocal
- the manifest warnings
- the strip paths in order, with the time range of each strip

The default output folder is `Saved/GameMechanicsRecs/_review/<timestamp>/`.

## Where outputs land

```
Saved/GameMechanicsRecs/
  <Category>/<JobName>/
    <JobName>_16x9.mp4            <JobName>_9x16.mp4
    <JobName>_16x9_sheet.jpg      <JobName>_9x16_sheet.jpg
    <JobName>_16x9_manifest.json  <JobName>_9x16_manifest.json
    <JobName>_16x9.log            <JobName>_9x16.log
    <JobName>_16x9_probe.mp4 ...  (probe runs; the sidecar files of a probe carry _probe too)
  _work/<JobName>_<aspect>[_probe]/   one take while it renders (deleted after a successful post)
  _review/<timestamp>/                review packs
  _gmr.lock  _queue.hold  _queue.stop  _queue_status.json
```

**Only the latest version of a recording is kept.** A new run replaces the previous mp4 and its sidecar files.
Old takes are never stacked up, and the frames and WAV are deleted after post. Copy a version somewhere else
yourself if you want to keep it.

The environment variable `GMR_OUTPUT_ROOT` replaces `Saved/GameMechanicsRecs` for everything above, which is
useful for tests.

## Job JSON reference

A job is one JSON object. Its fields match the C++ struct `FGMRJob` (`Source/RTS_GameMechanicsRecs/GMRJobTypes.h`).
Key names are case-insensitive for the C++ reader. A UTF-8 BOM in the source file is fine, because `gmr.py`
rewrites the file without one. Unknown keys are ignored by C++; use `_comment` for notes.

### Python-only fields

| Field | Meaning |
|-------|---------|
| `Category` | Output folder under the GMR root (default `Misc`) |
| `Vertical` | Object merged over the job for the 9x16 take only. Dicts merge key by key and lists are replaced. Example: `{"VerticalFocalScale": 0.65, "Shots": [...]}` |
| `Landscape` | The same, for the 16x9 take only |
| `LaunchArgs` | List of extra engine arguments for this job, e.g. `["-SurvivalAutoStart", "-SurvivalStartIndex=0", "-SurvivalGrace=900"]`. They go after the fixed arguments and before `GMR_EXTRA_ARGS`. They can be overridden per aspect in `Vertical`/`Landscape` |

`gmr.py` sets these fields itself: `OutputDir`, `Aspect`, `ResolutionX`, `ResolutionY` (and `Probe`).

### Job fields (FGMRJob)

| Field | Default | Meaning |
|-------|---------|---------|
| `JobName` | file name | `[A-Za-z0-9_-]+`. Names the frames (`LS_GMR_<JobName>`), the WAV and all outputs |
| `MapPath` | (required) | Package path of the map, e.g. `/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall` |
| `GameModeClassPath` | `""` | Empty uses the map's own game mode |
| `OutputDir` | set by gmr.py | Work dir of the take |
| `Aspect` | set by gmr.py | `16x9` or `9x16` |
| `ResolutionX`, `ResolutionY` | set by gmr.py | 1920x1080 / 1080x1920 (half for `--probe`) |
| `Fps` | 30 | Output frame rate |
| `WarmupSeconds` | 6 | Rendered and then cut away; the director sets up the scene during these seconds |
| `DurationSeconds` | 20 | Length of the recorded timeline (the final video) |
| `EngineWarmUpFrames` | 32 | MRQ engine and render warm-up frames |
| `SpatialSamples`, `TemporalSamples` | 1, 1 | MRQ anti-aliasing samples |
| `Anchor` | `{X,Y,Z}` 0 | World anchor of the scene. With `AnchorActor` set, this is an offset from that actor. The anchor drops to the ground |
| `AnchorYaw` | 0 | Yaw of the anchor frame (+X forward, +Y right, +Z up). All event and shot offsets are in this frame |
| `AnchorActor` | `""` | `PlayerStart` = the first player start; otherwise the first actor whose name or label starts with this |
| `DestroyClassPrefixes` | [] | Level actors whose class name starts with one of these are destroyed at setup (mission managers, spawners) |
| `HideActorPrefixes` | [] | Level actors whose name or label starts with one of these are hidden and lose collision |
| `ClearUnitsRadius` | 0 | Removes pre-placed units and squads within this radius of the anchor (cm) |
| `ClearPlayers` | [] | Removes every pre-placed unit of these players (e.g. `[2]`) |
| `bDisableFogOfWar` | true | |
| `bKeepVoiceLines` | true | Unit voice lines stay audible |
| `bKeepAnnouncer` | false | |
| `bKeepMusic` | false | |
| `bNoDistanceAttenuation` | false | All sounds are forced 2D in headless renders. This flag also drops distance attenuation |
| `VerticalFocalScale` | 0.72 | 9:16 focal = 16:9 focal x this (when a key's `VFocal` is 0) |
| `VerticalDistanceScale` | 1.0 | 9:16 distance = distance x this (when `VDistance` is 0) |
| `VerticalPitchAdd` | 4 | 9:16 pitch = pitch + this (when `VPitch` is 0) |
| `MotionBlurAmount` | 0.35 | Motion blur on the cine camera; a negative value keeps the map's setting |
| `Events` | [] | Scripted gameplay (below) |
| `Shots` | (required) | Camera moves (below); the runner refuses a job without shots |

### Events

An event is a scripted action at a timeline second. All event types share one flat struct (`FGMREvent`), and each
type ignores the fields it does not use.

| Field | Meaning |
|-------|---------|
| `Time` | Seconds on the timeline (0 = the first recorded frame). **Negative = setup before the first frame** |
| `Type` | The event type (list below) |
| `Tag` | Units created by Spawn get this tag; other events act on all live units with it |
| `ClassPath` | Class to spawn, e.g. `/Game/.../BP_X.BP_X_C` |
| `Player` | Owning player (1 = the player, 2 = the enemy) |
| `Count` | How many to spawn |
| `Columns`, `Spacing` | Spawn grid: units per row and spacing in cm (default 1200). Rows go back along -X of the event yaw |
| `Offset`, `Yaw` | Location (cm) and yaw (degrees) in the anchor frame |
| `TargetTag` | Target units (Attack, Garrison, Trench pits, ...) |
| `TargetOffset`, `TargetYaw` | Target location and facing in the anchor frame (Move destination, line end, ...) |
| `Radius` | Area radius (cm) |
| `Value` | Generic number (amount, health fraction, dig seconds per metre, ...) |
| `Text` | Generic text (ability enum name, stance, voice line, ...) |
| `Index` | Selects one unit inside the tag (-1 = all) |
| `Settings` | Object of name -> number for events with many settings, e.g. `{"Yaw": 90, "SuppliesReadyTime": 5}` |

The **orders** `Move`, `AttackMove`, `Attack`, `AttackGround`, `Stop` and `Rotate` also accept `Text` = a voice
line for the first unit to play. Their Settings are `Queue` (1 = queue the order), `KeepFormation` (default 1) and
`ForceVoice`.

| Type | What it does |
|------|--------------|
| `Spawn` | Spawns `Count` x `ClassPath` for `Player` at `Offset`/`Yaw` in a grid. Tags them `Tag`. Settings: `FlightHeight` |
| `Move`, `AttackMove` | Order the units of `Tag` to `TargetOffset`, keeping their formation (`KeepFormation` scales the spread). They end facing `TargetYaw` |
| `AttackGround` | Units of `Tag` fire at the ground at `TargetOffset` |
| `Attack` | Units of `Tag` attack the units of `TargetTag`, spread over the targets |
| `Stop` | Stops the units of `Tag` |
| `Rotate` | Turns the units of `Tag` to `TargetYaw` |
| `Stance` | `Text` = `Stance_HoldPosition` or `Stance_Aggressive` |
| `Voice` | Plays a voice line: `Text` = an ERTSVoiceLine name, e.g. `Move`, `Attack`, `Select`. Settings: `ForceVoice` (default 1) |
| `Destroy` | Destroys the units of `Tag` |
| `Hide` | Hides level actors whose name, label or class starts with `Text` (empty = any) within `Radius` of `Offset` (0 = anywhere), for example a tree that blocks a shot. Recorded units are never hidden |
| `Clear` | Removes the pre-placed units (`ClearUnitsRadius`/`ClearPlayers` rules), plus any unit that is not recorded within `Radius` of `Offset` |
| `Resources` | Grants resources: `Value` = the amount |
| `Deploy` | Nomadic trucks of `Tag` build where they stand. Settings: `Yaw` (building yaw in the anchor frame) |
| `Expand` | Building expansion. `Tag` = the building, `Text` = the EBuildingExpansionType suffix, e.g. `GerHQRadar`. Settings: `Building` (index inside the tag) |
| `Ability` | Global ability. `Text` = an EGlobalAbility name, `Offset` = the target, optional `TargetOffset` = the end of a line. `Tag` claims the aircraft the ability spawns. Settings: `Aircraft`, `StrafeTime`, `Repeat`, `ClaimSeconds` |
| `Claim` | Claims level actors by label or class prefix into `Tag`, so later events can use them |
| `Trench` | Digs a trench between pits. `TargetTag` = the pits tag. Settings `From`/`To` = pit indices, plus `EndCap` and `Bend`. `Value` = dig seconds per metre. `Text` `Instant` = no dig time |
| `Garrison` | Squads of `Tag` move into the trench of `TargetTag` |
| `Capture` | Supply point near `Offset`. `Text` `Force` = captured at once. Settings: `SuppliesReadyTime`, `TruckLoadingTime`, `CaptureDuration`, `CaptureRadius`. `TargetOffset` = override for the supply base |
| `HideUI` | Hides the game UI |

`Source/RTS_GameMechanicsRecs/GMRDirectorEvents.cpp` is the authority for each type's details.

### Shots

A shot is a continuous camera move between two cuts. Each channel is splined (Catmull-Rom) over an eased shot
clock, so a move starts and ends at rest unless `Ease` is `Linear`.

| Field | Default | Meaning |
|-------|---------|---------|
| `Name` | | Shown in the manifest and in the review |
| `Start`, `End` | 0, 5 | Timeline seconds |
| `Rig` | `Orbit` | `Orbit`: the camera sits on a sphere around the splined pivot. `Fixed`: the camera stays where key 0 puts it at the shot start, and only the look point moves (a pan or tilt). `Follow`: like Orbit, but `Yaw` is relative to the heading of the first key's tagged units |
| `Keys` | | Camera keys (below) |
| `Ease` | `InOut` | `InOut`, `In`, `Out` or `Linear`. The S-curve has 35% ramps |
| `PivotLag` | 0.6 | Half-life (s) of the spring that smooths tracked pivots |
| `Aperture` | 0 | F-stop for depth of field; 0 = no DoF |
| `Shake` | 0 | Handheld drift in degrees |
| `bAvoidOcclusion` | true | Tilts the camera up when something blocks the view. The manifest reports this as `OcclusionPitchBoost` |
| `MinGroundClearance` | 300 | Minimum camera height above the ground (cm) |

Camera keys (`FGMRCamKey`):

| Field | Default | Meaning |
|-------|---------|---------|
| `Time` | 0 | Seconds from the shot start |
| `Pivot` | `""` | `""` = the anchor. `tag:<Tag>` = the centroid of the live units with that tag. `actor:<LabelPrefix>` = the first level actor whose name or label starts with the prefix |
| `Offset` | 0 | Added to the pivot, in the anchor frame (cm) |
| `Yaw` | 180 | Compass direction from the pivot to the camera, in the anchor frame. 0 = camera in front, along +X. 180 = behind |
| `Pitch` | 25 | Degrees above the pivot (positive looks down) |
| `Distance` | 3000 | cm from the pivot |
| `Focal` | 35 | Focal length in mm. The 16:9 sensor is 23.76 x 13.365 mm |
| `Roll` | 0 | Degrees |
| `LookOffset` | `{0,0,150}` | Look point = pivot + Offset + LookOffset (anchor frame) |
| `VYaw`, `VPitch`, `VDistance`, `VFocal` | 0 | 9:16 overrides. 0 = derived: focal x `VerticalFocalScale`, distance x `VerticalDistanceScale`, pitch + `VerticalPitchAdd`, same yaw |

### Additions (scene catalogue, scouting, skies)

- **Scene catalogue.** Every recording is generated by `gmr_jobs.py` from compact scene functions (base expansion,
  building expansions, vehicle battles, mixed battles, air strikes, trenches, supply point, POIs) using the camera
  grammar of `gmr_shots.py` (`orbit`, `track`, `pan`, `sweep`, `crane`, `follow`, `push` and the `STYLE` house
  look). Do not hand-edit generated `Jobs/*.json`; change the generator and run `python gmr_jobs.py`.
- **Location scouting.** `python gmr_scout.py --run [--only <name tokens>]` renders a top-down still per job anchor
  (up = the job's anchor +X, right = anchor +Y) with coloured markers: red = player 1 spawn, blue = player 2 spawn,
  yellow = move target, green = nomadic deploy spot, magenta = air-strike line, white = anchor. Check them before
  any final render: buildings, cliffs and lakes show up at once.
- **Skies.** Campaign maps have no sky (the RTS camera never sees the horizon). Job fields `SkyMaterial`, `SkyMesh`,
  `SkyRadius`, `SkyYaw`, `SkyDrop` put a render-only sky sphere around the anchor; the `Sky` event swaps its material
  mid-take (Text = material path, Value = new yaw). The catalogue uses the SkyboxPack_X `M_Flowmap_Inst_0N` set.
- **More events.** `Marker` (scouting column: Offset, Text = red/blue/green/yellow/white/magenta, Value = radius,
  Settings.Height), `Claim` with a `ClassPath` (actors of that class the game spawns during Settings.ClaimSeconds join
  the Tag, e.g. the supply truck), `Deploy` with Settings.Instant = 1 (the building stands before the take).
- **Fixed-rig vantage point.** Shot fields `CameraPivot`, `CameraOffset` (anchor frame; Z = height above the ground)
  and `VCameraOffset` (9:16) place a Fixed (pan) camera independently of what it looks at; `sweep()` uses them to
  pan along a row of subjects.
- **Camera behaviour.** The first shot is framed during the warm-up (no stale first frame); the camera manager is
  refreshed after every move (no one-frame lag, cuts land on their frame); the RTS camera pawn is hidden and follows
  the look point; a subject hidden for more than 0.25 s by terrain or buildings (2 of 3 sight lines blocked) makes the
  camera crane up around it and settle back once clear.
- **Voices.** The manifest's `VoiceLinesHeard` counts voice-line sounds that were actually audible (wave volume
  above 0.01 after mix and attenuation); the log lists each with its loudest volume (`GMR_VOICE_HEARD`).

- **Pivots over several units.** `tag:X` is the centroid of every live unit with the tag; when the units are far apart
  (two aircraft, two squads 18 m apart) that point is empty sky or ground. Use `tag:X[i]` for one unit (registration
  order) or `live:X` for the nearest live one.
- **Trenches.** The `Trench` event digs an `ARTSFieldTrench` between two tagged pits' TrenchPort sockets (Settings From,
  To, Bend cm, Kit 0 = German / 1 = Soviet; Value = dig seconds per metre). Starts skip ports an earlier trench used
  (a start on a used port loops in place); ends may share one. The recorder pins LOD0 on its trench pieces (the kit
  meshes have empty LODs 1-3). The German kit pieces do not draw in these renders at all, so `TRENCH_KIT` in
  `gmr_jobs.py` films the Soviet kit until that is fixed. `GMR_TRENCH`/`GMR_TRENCH_PIECE`/`GMR_TRENCH_PATH` log lines
  show dig progress and the planned pieces.
- **Aircraft claims.** An `Ability` event with a `ClassPath` claims only aircraft of exactly that class (the survival
  drop ships derive from the Bf 110 Blueprint).

### Manifest (written by the director)

`Manifest.json`: JobName, Aspect, Fps, StartFrame, EndFrame (exclusive), TotalFrames, bComplete, Anchor,
Shots[{Name, Rig, StartFrame, EndFrame, OcclusionPitchBoost, FirstCameraLocation, FirstLookAt, FirstFocal}],
Events[{Frame, Type, Tag, Result}], Warnings[], VoiceLinesHeard. All frames are sequence frames.

The C++ writer uses camelCase keys (`startFrame`, `bComplete`); the Python tools accept either form.
