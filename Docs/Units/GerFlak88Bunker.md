# German Flak 88 bunker (breakthrough doctrine)

An Atlantikwall Flakstand: a concrete casemate with an octagonal open gun pit on its roof holding the Flak 88, stairs up the right side behind a spiked cheek wall, two camo nets, ready-round lockers round the gun and a cast block of six open cells full of rounds. Below half health it swaps to a damaged mesh: breaches with rebar, bitten corners, roof craters, rubble, torn nets.

| | |
|---|---|
| Blueprint | `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/Flak88Bunker/Ger_BXP_Flak88Bunker` |
| Expansion type | `BTX_GerFlak88Pit` for now, so it has the pit's stats and cost. See [Not done](#not-done). |
| Turret | `RTS_BP_EmbTurret_GerPit_Flak88` (child actor `Howitzer`): the Flak 88 pit's gun, limits (0-10 degrees pitch) and three-man crew |
| Size | 13.5 x 12 m casemate plus stairs and cheek wall, roof 3.4 m, parapet top 5.0 m |
| Triangles | 7.3k main mesh, 18.6k damaged mesh, 18.2k rounds |

Blender source, design history, the gun sweep and the damage spec: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/Flak88Bunker` (`Flak88Bunker_Brief.md`, `Model/README.md`).

Import: `Tools/BreakthroughBuildings/ue_flak88_bunker_import.py` (UECP `execute_python`). It is safe to rerun after a Blender rebuild. The damage is built by `SharedParts/bf_damage.py` in the Blender folder, which the Flakvierling bunker's damaged mesh uses too.

## Assets

| Asset | |
|---|---|
| `Flak88Bunker/Mesh/SM_BF_Flak88Bunker` | Intact mesh: 18 UCX hulls, sockets `Turret`, `Exp1-3` |
| `Flak88Bunker/Mesh/SM_BF_Flak88Bunker_Damaged` | 50 % health mesh: same pivot, sockets, UCX and material slots |
| `Flak88Bunker/Mesh/SM_BF_Flak88Bunker_Ammo` | All 157 rounds (hoist stack, lockers, cells), no collision, shared by both health states |
| `BreakthroughShared/M_Ammo_FactionShared` | The faction's shared ammo atlas (`T_Ammo_Shared_*`, ORM) |
| `BreakthroughShared/MI_BF_SootedFortress_UVTile`, `M_BF_Trim`, `M_BF_CamoNet` | Concrete, trim, net (shared with the other Breakthrough buildings) |

## Blueprint

A copy of the Flak 88 pit's expansion `GerPits/Ger_BXP_GerPit_Flak88`. What changed:
- `BuildingMeshComponent` and both `BP_InitBuildingExp` meshes use `SM_BF_Flak88Bunker`; 8 construction smokes over 8 m.
- `Howitzer` stands on the `Turret` socket, (-100, 185, 340): the gun's foot on the casemate roof. The pit deck is cast at the pit's 0.56 m over it, so the turret, its crew clips and the runner's path carry over unchanged.
- `Rounds`: a static mesh component on `BuildingMeshComponent` with the rounds, no collision.
- Selection box 12.4 x 16 m, decal, progress bar and `NavModifier` failsafe extent sized to the bunker.

### The 50 % mesh swap

- `Health.HealthLevelsToNotifyOn` lists 75, 66, 50, 33 and 25 %, so the health component reports crossings both ways (damage and repair).
- `Bp_OnHealthChanged` (a function override) calls `ApplyHealthMesh`: at or below `DamagedAtOrBelow` (0.5) health the building mesh is `DamagedMesh`, above it `IntactMesh`. It reads the actual health percentage, so a big hit that skips levels still lands right.
- `SetBunkerBusy` is called after the existing construction and pack-up events:
  - On start of construction or pack-up it sets `bMeshBusyWithConstruction` and hides `Rounds`. The swap waits, so the construction mesh is never replaced.
  - On finished construction or cancelled/finished pack-up it clears the flag, shows `Rounds`, and applies the mesh for the current health. The C++ puts the intact mesh back before these events fire.
- Both meshes have the same slots and materials, so the component's material overrides carry over.

## Verified

In a Play session on the StartGame map (spawned by hand at the origin):
- intact mesh with the right materials;
- the Flak 88 turret exactly on the `Turret` socket, with `Crew1`, `Crew2` and `Runner`;
- `Rounds` visible, no collision;
- health set to 60 % kept the intact mesh, 40 % and 20 % showed the damaged mesh, repair to 55 % and 90 % went back to intact;
- in-game shots of both states: `Model/Renders/ue_intact.png`, `ue_damaged.png` in the Blender folder.

The Blueprint compiles with no errors or warnings.

**Not verified:** firing and the crew drill on this mesh, auto-engage, the damage explosions and death, navigation blocking (`LetBuildingMeshAffectNavMesh` is false as on the pit; `NavModifier` covers the footprint), and the swap during a real construction or pack-up.

## Testing

`-GerFlak88BunkerValidate` on TestCover runs the Flak 88 pit's own test on the bunker, as the player's (`GunPitTestScenario`):
- the crew and rounds ride on `PitOrigin`;
- the gun stands 340 cm above the bunker's pivot;
- the gun and its crew turn to a ground target;
- two shots: the barrel kicks and the crew brace;
- one full handover drill, with hands within 70 cm of the round;
- the runner takes a round at the hoist bay and lays it on his locker lid.

All 26 checks passed on 2026-11-03 with the pit's animations unchanged. The hoist bay and the locker ring keep the pit's numbers, so nothing needed re-baking.

```bash
MSYS_NO_PATHCONV=1 UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/TestCover/TestCover -game -unattended -nop4 -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -benchmark -fps=30 -DisablePlugins=Fab -GerFlak88BunkerValidate -GerPitCapture=<folder> -CaptureExposureBias=3 -ExecCmds="r.TextureStreaming 0"
```

Leave out `-GerPitCapture` (and use `-NullRHI`) for a quick run without pictures. Build the DebugGame editor target first if the Development editor is open (Live Coding blocks a Development build).

## Not done

It is not in the HQ's build menu, and it shares `BTX_GerFlak88Pit` with the Flak 88 pit. Its own expansion type, data, footprint rules, async spawner mapping and training slot are C++ (the rail Pak bunker is the pattern; see [GerFlakvierlingBunker.md](GerFlakvierlingBunker.md#not-done)). Until then it must be placed in a level by hand.

The concrete material's damage layer is not turned up on the damaged mesh: cracks and soot would come from an instance of `MI_BF_SootedFortress_UVTile` with the damage and soot layers on.
