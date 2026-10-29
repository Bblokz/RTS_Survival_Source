# Ambient landscape effects: dust devils

Dust devils drift over the sand parts of the desert maps (Ger_Trains, the sandy parts of Ger_FirstMission). They are
drawn by **one Niagara system for the whole map**, the same idea as the trace impacts of the small arms: however many
devils are alive, the cost is one Niagara component, one GPU simulation per emitter and one draw per emitter.

| Part | Where |
|---|---|
| Subsystem | `Source/RTS_Survival/Environment/AmbientFx/LandscapeAmbientFxSubsystem.h/.cpp` (`ULandscapeAmbientFxSubsystem`, a tickable world subsystem) |
| Project settings | `RTSAmbientFxSettings.h` — Project Settings → Game → **RTS Ambient Effects** (system, counts per level, radii, lifetimes, colour, sand surface types, excluded maps) |
| Niagara system | `/Game/RTS_Survival/Blueprints/Environment/AmbientFx/NS_AmbientDustDevils` (emitters `DustDevil_Column`, `DustDevil_Skirt`, `DustDevil_Grit`) |
| Material, texture | `M_AmbientDust` (lit translucent, volumetric non-directional lighting, depth fade), `T_AmbientDust_Puffs` (2x2 swirled puff atlas) |
| Emitter HLSL | `Source/RTS_Survival/Environment/AmbientFx/DustDevilHlsl/*.hlsl` |
| Authoring command | `DustDevilNiagaraAuthoring.cpp`: editor console command `RTS.AmbientFx.PatchNiagaraSystem`; headless: `Source/Tools/AmbientFx/patch_dust_devils.sh` |
| Sand masks | `/Game/RTS_Survival/Blueprints/Environment/AmbientFx/SandMasks/SandMask_<MapName>` (`UAmbientFxSandMask`), baked by the editor command `RTS.AmbientFx.BakeSandMask` / `Source/Tools/AmbientFx/bake_sand_masks.sh`; previews in `Saved/AmbientFx/SandMask_<MapName>.bmp` |
| Texture generator | `D:/Blender/BlenderProjects/MappingDetails/AmbientFx/gen_dust.py` |
| Player setting | Escape menu → Graphics → **Ambient Effects**: Disabled / Low / Medium / Maximum (`ERTSAmbientEffectsLevel`, saved in GameUserSettings.ini) |

## How it works

1. **Where devils may form.** None of the landscape layers carries a physical material, so a trace cannot tell sand
   from grass or rock. Instead each map gets a **baked sand mask**: `RTS.AmbientFx.BakeSandMask` samples the landscape
   paint layers on a 25 m grid in the editor (five samples per cell) and marks the cells where the layers in
   `SandLayerNames` (Sand, AridSoil, Desert, GraySand, ...) cover at least half. When the map begins play the subsystem
   loads `SandMask_<MapName>`, traces each sand cell once for its ground point and keeps the flat ones. The log line
   `Ambient effects: map ... baked sand mask grid ..., flat sand cells N` shows the result. Without a mask it falls
   back to the physical surface of a trace (`SandSurfaceTypes`), which today finds nothing; a map with no sand cells,
   no landscape, or listed in `ExcludedMaps` gets no devils and no component.
   - Ger_Trains: 208 of 925 cells (the desert band in the north, sandy tracks); Ger_FirstMission: 691 of 1353.
   - Rebake after repainting a map's landscape: `bash Source/Tools/AmbientFx/bake_sand_masks.sh [map...]`.
2. **Only near the camera.** Every 0.5 s the camera's view ray is traced onto the landscape. New devils start on a
   random sand point within `SpawnRadius` (60 m; the RTS view is about 65 x 36 m) of that point, at least 25 m from the others, one every
   `SpawnInterval<Level>` seconds while fewer than the allowed count are alive. A devil fades out at the end of its
   `Lifetime`, when it drifts off the sand, or when it is further than `DespawnRadius` from the camera focus.
3. **The CPU side is tiny.** Each devil is a slot (max 16) with a ground point, scale, strength, seed, spin and drift
   heading. The subsystem moves them (drift plus a slow wander), traces the ground under one devil per tick, and
   writes the slots into two `float4` array user parameters, plus the list of growing slots, each tick that devils
   exist. With no devil alive the Niagara component is deactivated and costs nothing.
4. **The GPU side does the rest.** Each emitter spawns `PerDevilRate × growing devils` particles per second. The
   spawn HLSL gives each particle a devil slot and random shape values; the update HLSL places it every frame relative
   to its devil, so the whole column moves, leans with the drift, sways and fades with the devil's strength.
   - **Column:** dust lifted at the foot spirals up a widening funnel (~14 m at scale 1), thinner towards the top.
   - **Skirt:** a low ring of dust rolling outward around the foot.
   - **Grit:** small dark specks whirling fast inside the lower column; they make the rotation readable from the RTS
     camera, where soft dust alone reads as haze.
5. **Lighting.** `M_AmbientDust` is lit translucency (volumetric non-directional: the cheapest lit mode), so the dust
   takes the sun and sky like the rest of the scene. Base colour is `DustColor` (linear albedo ~0.4/0.33/0.22),
   darkened up to 28 % where the puff texture's inner billow is thick. Fog of war hides devils over unexplored ground
   like any other scene geometry.

## The player setting

`rts.AmbientFx` (0 to 3) carries the setting from the settings menu to the subsystem; the subsystem reads it every
tick, so a change applies at once (extra devils fade out). Per level (project settings):

| Level | New devil every | Most alive | Alive on average (devil lives ~37 s) |
|---|---|---|---|
| Low | 120-210 s | 1 | 0.2 |
| Medium | 40-70 s | 3 | 0.7 |
| Maximum | 20-35 s | 6 | 1.4 |

The intervals were set on 2026-10-28 to a quarter of the first rate (a devil every 2-6 s up to the cap, which kept
every level at its cap). The caps (at most 16) now only limit a lucky streak. Particles per devil are the same at every level, so a devil looks the same everywhere.

`rts.AmbientFx.Instant 1` makes devils appear at full strength up to the maximum at once and vanish at once, for
benchmarks.

## Editing the look

1. Edit `DustDevilHlsl/Common.hlsl` (spawn prelude, devil read) or a layer file (`Column`, `Skirt`, `Grit`). Each file
   is split by `// @section <Name>` lines: a layer has `Shape` (spawn: lifetime and `DevilShape` from the random
   numbers `R0..R3`) and `Place` (position, colour, size, rotation from the devil `PS`/`MO` and the age `T`).
2. Close the system's Niagara tab in the editor (or unload it), then run `Source/Tools/AmbientFx/patch_dust_devils.sh`
   (headless) or the console command `RTS.AmbientFx.PatchNiagaraSystem` in an editor that has the current DLL. The log
   prints one `compiled ... status/errors` line per script.
3. A new layer: add an emitter named `DustDevil_<Layer>` from the classic `Minimal` emitter template, add a `SpawnRate`
   module bound to `User.DustDevil_<Layer>_SpawnRate`, set the sprite renderer material to `M_AmbientDust` and
   Sub Image Size 2x2, add `<Layer>.hlsl` and its entry in `Layers[]` (particles per second per devil), then patch.

The patch command makes each `DustDevil_*` emitter a GPU emitter in world space with fixed bounds, keeps it looping
forever with system scalability, declares `Particles.DevilSlot`, `Particles.DevilShape` and `Particles.SubImageIndex`
with a Set Parameters module, inserts the custom HLSL nodes `DustDevilSpawn` / `DustDevilUpdate` before the script
outputs, wires them to the user parameters and writes the code from the .hlsl files.

## Niagara lessons (why the patch command does what it does)

- **Attribute trimming.** Niagara drops particle attributes it thinks no script reads, and a read inside custom HLSL
  does not count. `DevilSlot` / `DevilShape` vanished from the GPU particle data, so every update read slot 0 and a flat
  shape (the particles stayed at their spawn point with alpha 0: invisible). The patch adds them to the emitter's
  `AttributesToPreserve` and also feeds them to the update node as inputs from a parameter map read.
- **`ExecIndex() % AliveCount`** did not spread the new GPU particles over the devils (all went to the first slot),
  although `ExecIndex()` does vary per particle (it seeds the hash). The slot is picked with a hashed random number.
- **Packaging:** the sand masks are loaded by map name, so `Config/DefaultGame.ini` always cooks
  `/Game/RTS_Survival/Blueprints/Environment/AmbientFx`; keep new masks in that folder.
- **Sorting:** the component sits at the world origin with map-sized bounds, so the renderer sorts the whole dust
  system as one translucent object; a devil can draw behind nearby smoke from another system. The trace impacts have
  the same limit.
- The patch dumps Niagara's translated code to `Saved/AmbientFx/Hlsl/<Emitter>_<Script>.hlsl`; check
  `LoadUpdateVariables` there when an attribute seems to read zero.
- Do not call UECP `get_niagara_runtime_state` on the shared editor: it crashed the editor (2026-10-28).

## Cost (2026-10-28, DebugGame, headless Ger_Trains, 40 tanks driving, 1920x1080, Maximum = 6 devils)

Measured with the tank trail benchmark switching `rts.AmbientFx` between 0 and 3 (`rts.AmbientFx.Instant 1`), six
off/on pairs of 240 frames, ProfileGPU once per phase:

`EXEC="rts.AmbientFx.Instant 1, rts.AmbientFx.Log 1" bash Source/Tools/TankTrails/run_bench.sh ddperf -TankTrailBenchCVar=rts.AmbientFx -TankTrailBenchOnValue=3 -TankTrailBenchAt=-25700,30000 -TankTrailBenchRepeats=6 -TankTrailBenchProfileGPU`

- **GPU:** pair delta median **+0.016 ms** (range -1.26..+0.59, i.e. inside the noise of a 6.6 ms frame). Each of the
  three dust devil simulations is one dispatch that rounds to 0.00 ms in ProfileGPU (about 2,300 particles alive for
  six devils). Translucency for the whole scene stays 0.02-0.21 ms, driven by explosions, not by the dust.
- **Game thread:** pair delta median -0.14 ms (noise). The subsystem does one landscape trace per tick plus one
  view trace every 0.5 s and uploads two 16-entry arrays.
- **For comparison:** the same frames run 170-212 other Niagara GPU dispatches (14 barracks smoke instances,
  20 tank-shell instances, ...), one per emitter per component. All dust devils together stay at three.
- With no devil alive (or the setting Disabled) the component is deactivated: zero dispatches and zero draws.
- Start-of-mission cost: loading the mask and tracing the sand cells, 2-5 ms once.
