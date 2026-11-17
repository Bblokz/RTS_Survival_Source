# Ger_FrozenLake map build

Status (2026-11-17): **first pass built, real ice placed**. The map has terrain, landscape paint, light, the lake ice
with its three types and nav areas, water, and editor notes. It has no kit meshes, no units and no mission scripting. Design: [FrozenLakeMission.md](FrozenLakeMission.md),
layout sketch A (`MapDesigner/FrozenLake/frozen_A.layout.json`).

## Files

- **Map:** `/Game/RTS_Survival/Maps/GerMissions/Ger_FrozenLake`, copied from `/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopyMap`.
- **Tools:** `Source/Tools/FrozenLakeMap`.
  - `fl_terrain_gen.py` turns the sketch into heights, paint layers and a resolved layout in `Saved/FrozenLake/terrain`.
    It also runs the triangle nav-reach checks (`check.txt`).
  - `fl_map_build.py` holds the in-editor steps.
  - `fl_editor_server.py` and `fl_cmd.py` drive a throwaway editor through `Saved/FrozenLake/cmd`.
- **Landscape material:** `Source/Docs/Landscape/ls_frozenlake_build.py` (run headless) builds
  `/Game/RTS_Survival/Materials/ground/Landscape/FrozenLake/M_RTS_Landscape_FrozenLake`, the MI and seven layer infos.
  - It is the Pripyat landscape graph run with its own layer table.
  - The nine layers (2026-11-17, swampy Karelian tundra; the owner found the brown sand did not fit) are FLSnow
    (Brushify snow), FLFrost, FLMoss (sphagnum), FLLichen (pale reindeer lichen), FLHeath (Megascans dried grass,
    rust), FLPeat (wet peat; also the lake bed and the cut floors), FLGranite (rock_05), FLGravel and FLBurnt
    (Megascans burnt forest floor).
  - The old layer infos FLScorched, FLDryGrass, FLMud and FLAsh are still in `DATA/`, unused.
  - 237 pixel instructions, 2 samplers.
  - MI overrides are in `fl_map_build.LANDSCAPE_TUNE`. The source textures are dark, so the tints lift each layer to
    a readable albedo. They were measured from a base-colour capture (`SHOT_SOURCE = SCS_BASE_COLOR`, then the mean
    per dominant layer): moss about 0.2, lichen 0.45, granite 0.35, gravel 0.3, peat and burnt floor about 0.1, snow
    0.78.
- **Ice:** `fl_ice_gen.py` (plain Python) builds all ice data from one grid of ice types into
  `Saved/FrozenLake/ice/map`, and `fl_map_build.step_ice_real` places it. Assets are in
  `/Game/RTS_Survival/Blueprints/Environment/FrozenLake/`:
  - `Map/SM_FL_IceLake` (actor `FL_IceLake`, z 0, landscape collision, affects navigation): the lake outline pushed
    1.5 m under the shore, with the three leads cut out as real holes.
  - `Map/MI_FL_IceLake`: a child of the ice session's `Ice/MI_FrozenLakeIce_Lake` (Masked, `UseStateMask` on), with
    `StateMask` = `Map/T_FL_IceState`, `LakeOrigin` (-23200, -22500), `LakeSize` 51500.
  - `Map/SM_FL_IceLeadWater` (actor `FL_IceLeadWater`, z -30, no collision, no navigation): water under the lead holes,
    with `Ice/MI_FrozenLakeWater`.
  - `Nav/BP_NavArea_FL_IceNoHeavy` and `Nav/BP_NavArea_FL_IceInfantryOnly`: Blueprint children of
    `NavAreaMeta_SwitchByAgent`. They are Null for the blocked agents and Default for the rest.
  - Actors `FL_IceNav_Lead` (`NavArea_Null`), `FL_IceNav_InfantryOnly` and `FL_IceNav_NoHeavy`. Each holds many box
    components set as nav-only dynamic obstacles (no collision, hidden in game) with that area.
  - The old `SM_FL_IcePlaceholder` / `MI_FL_IcePlaceholder` assets are unused now.

## Rebuild

1. `python Source/Tools/FrozenLakeMap/fl_terrain_gen.py`
2. Launch the throwaway editor:

   ```
   UnrealEditor.exe RTS_Survival.uproject -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap/fl_editor_server.py" -unattended -nosplash -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -ResX=960 -ResY=540 -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
   ```

3. Run the steps through `fl_cmd.py`:
   - `step_duplicate`, `step_strip`, `step_landscape`, `step_fit_level_actors`, `step_lighting`,
     `tune_landscape_material`, `step_ice_real` (after `python fl_ice_gen.py`), `step_water`, `step_notes`;
   - then save after the navmesh build (`Saved/FrozenLake/c_rebuild_landscape.py` is the pattern).
   - A height-only change needs `step_reimport_heights`. A paint change needs `step_landscape` followed by
     `step_fit_level_actors` and `step_ice_real` (the ice copies the landscape's collision).
   - An ice layout change (regions in the sketch) needs `fl_terrain_gen.py`, then `fl_ice_gen.py`, then `step_ice_real`
     and a save after the navmesh build (`Saved/FrozenLake/c_ice_real.py` does this and probes the paths per agent).

## Snow ruts

Tank trails cut deeper-looking ruts in snow and frost than in the bog. The shared `MF_RTS_LS_TankTrails` is not
changed: about 20 maps call it, and rebuilding it unhooks them. Instead, `Source/Docs/Landscape/ls_frozenlake_snowruts.py`
adds a stage to this map's landscape material only. It sits between the shared function and the `LS_TankTrails`
switch, and it acts where FLSnow (plus 60 % of FLFrost) lies. It reads the same `RT_TankTrails` as the shared
function, with five samples inside a branch, and needs no extra sampler.

- **Wider:** `SnowRut_Low` / `SnowRut_High` (0.05 / 0.35) smoothstep the same rut value lower than the soil ruts do.
- **Deeper shading:** the rut slope tilts the normal by `SnowRut_NormalStrength` (5.5, against 0.7 on soil).
- **Rut floor:** compacted, shadowed snow (`SnowRut_Tint` 0.30/0.34/0.44). The deepest part shows dark peat
  (`SnowRut_Soil`, `SnowRut_Expose` 0.6).
- **Berm:** a brighter, rougher rim just outside the rut edge (`SnowRut_BermBrightness`).
- **On soil:** snow keeps only 35 % of the shared soil rut look (`SOFT_LAYERS`), so the two looks don't stack.
- **Shading only:** the landscape has a vertex every metre and the ruts are 30-90 cm wide, so there is no real
  displacement.
- **Fade:** ruts fade at the shared rate (`UTankTrailSubsystem`, global), the same as on soil.
- **Tuning:** the values live in `fl_map_build.LANDSCAPE_TUNE` (MI) and as the master defaults.
- **Checked 2026-11-17:**
  - The trail bench on Ger_FrozenLake (`MAP=... run_bench.sh fl_snow -TankTrailBenchAt=-22500,-22500`) passed:
    40 tanks, GPU on/off median +0.02 ms.
  - Its view shot is too far out to show single ruts.
  - A painted test pattern (`Saved/FrozenLake/c_rut_test.py`) shows wide dark troughs in snow and thin grooves on
    moss. In base colour, the snow ruts measured about 30 % darker than the snow beside them.
  - Not seen in a real PIE game camera yet.

## How the terrain works

- **Height frame:** the ice surface is z = 0.
  - **The ice is not landscape**, as the owner decided: it is a separate mesh, so vehicles can break through and sink.
  - The landscape under the lake is a lake bed, 0.6 to 4.5 m below the ice.
  - The shore follows the exact sketch outline. The ground sits 25 cm under the ice at the outline, crosses ice level
    about 1.3 m inland, and climbs to the land (about 1.8 m) over 11 m, with a maximum slope of 17 degrees.
  - The ice mesh reaches 1.5 m under the shore.
- **Blocking ground** uses 60-degree banks of at least 2.4 m, which the 44-degree nav agents cannot climb:
  - the Forge dam (5.6 m crest, stopped at the ice so the spillway gap stays open);
  - pump 1's outflow gorge;
  - the spillway canal.
- **Placeholder fill:** the canal bridge is a 10 m earth fill for now, until the dam kit's bridge mesh exists.
- **Paint:**
  - snow inside the frost ring (the pumps' range), with a frost crust at its rim and on banks steeper than 14 degrees;
  - outside the ring, the thawed Karelian bog: moss as the base; wet peat in the hollows (ground under its 15 m mean)
    and in scattered wet patches; lichen on the rises; broken heath patches; granite on slopes over 14 degrees and on
    a few glacial outcrops on the rises;
  - lingering snow patches over the bog (snow layer at up to 90 %): ragged drifts from three noise scales, more in the
    hollows, none on the wind-scoured rises, so moss shows through at the edges;
  - gravel on the roads (smooth edges from the exact distance) and on the pads;
  - burnt floor in the dead forests (frost inside the ring), the scorched craters and at the burnt convoy;
  - peat under the ice and water.
- **Light:** a low, cold sun from the south-west, 34 degrees up, with pale blue sky light and a thin cold haze.

## The ice

**One grid decides everything.** `fl_ice_gen.py` paints an ice type into every 1 m cell, in the sketch's region
order, so a later region covers an earlier one:

| Type | Regions | Holds | Nav |
|---|---|---|---|
| Heavy (white, T 1) | `lake_ice`, `inlet_ice`, `forge_road_ice`, `heavy_road_ice`, `rig_pad` | everything | plain ground |
| Medium (grey, T 0.5) | `thin_*` | medium and light tanks, infantry | `NoHeavy`: Null for HeavyTank |
| Infantry (black, T 0) | `vein_*` | infantry only | `InfantryOnly`: Null for Light, Medium and HeavyTank |
| Open water | `lead_finger`, `lead_ne`, `outflow_lead` | nobody | hole in the mesh, plus `NavArea_Null` from the bed to over the ice |

- **Order matters where regions cross.** The heavy ice road is painted after the vein, so it stays heavy ice where
  it crosses a black vein finger.
- **What comes out of the grid:**
  - **The state mask**, in the format of [FrozenLakeIce.md](FrozenLakeIce.md):
    - R = thickness, blurred 0.8 m so the material's band borders land on the grid borders;
    - G = 0;
    - B = open water, which crosses 0.5 at the hole edge so the slush rim lines up with the cut;
    - A = the vein.
    - The image has north to the right and east down.
  - **The mesh holes**, from exact signed distances.
  - **The nav boxes.**
- **Nav areas grow past their ice:** black ice by 1 m, grey ice by 1 m, and leads by 1.5 m. The boxes snap to 2 m
  blocks and merge only over cells already blocked for the same units, so they never cover heavy ice beyond that.
  That gives about 460 boxes. The heavy road keeps about 8.5 of its 10 m for heavies.
- **Why the leads need Null as well as a hole.** The lake bed under a hole is walkable landscape. It is only 25 cm
  under the ice at the shore, so without the Null box units would walk down into the lead and under the ice.
- **A hard block, chosen to be easy to relax.** Heavies cannot be ordered onto grey ice, and no tank onto black ice,
  so pathing will not take a player's heavy onto grey ice to break through. The scripted lesson has to force it. To
  make it a strong preference instead, set the meta area's agent slot to a high-cost area in the Blueprint; no code
  is needed.
- **Later: the ice mechanic.** The runtime ice (melt front, holes) must update the same three things: the state mask
  (a render target), the hole collision and the nav boxes. The project uses `RuntimeGeneration=DynamicModifiersOnly`,
  so moving or adding nav boxes at runtime works.

## Checked

- **Generator (triangle reach from the HQ, with the ice counted as walkable at z = 0):** every objective, resource,
  spawn, the airfield and the King Tiger can be reached. No dam crest cell and no gorge or canal floor cell can be
  reached.
- **Editor navmesh (four agents: Character, Light, Medium and Heavy tank):** full paths from the HQ to the rig, the
  Shield HQ, pump 3, the King Tiger and the airfield. Paths to the dam crest and the gorge floor are partial.
- **Ice, generator (per unit class, over the actual nav boxes):**
  - Infantry reaches every feature.
  - Light and medium tanks reach everything except the reveal spot, which is on black ice.
  - Heavy tanks also miss the Radixite upwelling (on grey ice).
  - No lead and no bed under a lead can be reached.
- **Ice, editor navmesh (each of the four agent navmeshes, 2026-11-17):**

  | Path | Character | Light | Medium | Heavy |
  |---|---|---|---|---|
  | HQ -> Shield HQ | 591 m | 596 m | 596 m | 629 m |
  | HQ -> grey ice | reached | reached | reached | none |
  | HQ -> black ice | reached | none | none | none |
  | Lead hole, lead bed | none | none | none | none |

  - The S spawn reaches the HQ round the outflow lead: 460 to 469 m.
  - HQ -> rig for heavies comes back partial. The rig is reachable for heavies, but only by a 561 m detour over the
    heavy road and the Forge spur, which runs past the project's `DefaultMaxSearchNodes` (2048) with 10 m tiles.
    Both legs (spur -> rig and spur -> Shield HQ) are full paths. In game, a heavy ordered to the rig from the HQ stops
    at the nearest point on the heavy road (53 m short) unless the search limit is raised.
- **Fixed during the build:**
  - The first placeholder was one rectangle at z = 0. It reached under the dam and the shore, and the navmesh grew a
    tunnel on it under the land. It is now a lake-shaped mesh (a fan over the outline, plus the inlet strip). The OBJ
    import mirrors y, so the generator writes -y.

## Not done or not verified

- **Ice look.** The first ice look is in place. The ice session is still tuning it: diagonal streaks on the heavy ice,
  pure-black lead water, the cold-cyan warn and unit readability on black ice. Shots are in
  `Saved/FrozenLake/shots/ice_*.png`.
- **Pressure ridges.** The sketch's ridges are not built yet; they are part of the ice kit meshes.
- **Tundra look.** First pass, judged from scene captures (`Saved/FrozenLake/shots/tundra_*3.png`). There is no
  raised relief for the granite outcrops yet (paint only), and no bog pools or foliage.
- **Look.** Judged only in scene captures, which are darker than the game (fixed capture exposure). The gravel, frost
  and dry-grass albedos are much darker than the snow and sand, so their tints were raised. No PIE or in-game shot has
  been taken yet. Close up, the snow texture reads speckled.
- **Missing pieces.** No kit meshes (pumps, rig, dam, ridges and so on; see
  [FrozenLakeMeshPrompts.md](FrozenLakeMeshPrompts.md)), no foliage, no steam effects, and no grass or tank-trail check.
- **Commits.** Map saves are auto-committed by the editor's revision-control plugin when it is on. The landscape
  material assets were committed in `5d6cfbcbb` together with other sessions' assets. The map itself is not committed.
