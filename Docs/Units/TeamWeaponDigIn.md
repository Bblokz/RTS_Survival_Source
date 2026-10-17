# Team weapon dig-in: sandbag pits

A deployed team weapon can dig in. Its crew build a square sandbag pit around the gun and themselves. The pit protects the gun:
- +33 % max health;
- the gun's damage-taken multiplier ×0.8 (1.0 on every gun today, so it becomes 0.8).

Unlike the tank dig-in walls, the pit is not a unit and cannot be destroyed on its own. Shots that hit the sandbags hurt the gun, with the dig-in protection applied. The walls block the opponent's shots and navigation. Units enter through a small gap in the middle of the rear wall.

## Use

- **Dig In** on the command card. A packed gun deploys first, then digs in.
- The sandbags rise out of the ground over 10 s. The protection applies once they stand.
- While dug in, the card shows **Break Cover** instead of Dig In. Move, Rotate Towards and Tow are taken off the card, and the hull is locked.
  - Shots on a target out of range end the attack order instead of moving the gun.
  - The guards walk round to the entrance and stand at posts inside the pit.
- **Break Cover** removes the pit and the protection, at any time (also while the sandbags are still rising). The card goes back slot for slot.
- The pit and the protection are also removed when:
  - a truck hooks the gun;
  - the crew is lost (the gun is abandoned);
  - the gun dies.

## Pit design (one mesh per gun Blueprint)

The pits come from one generator, sized from a survey of every deployed gun: its bounds, muzzle socket and crew posts.
- A square of straight walls around the gun, its operators and guard posts.
- A 150 cm entrance in the middle of the rear wall.
- German pits use the German single sandbags and pit-atlas planks.
- Soviet pits use the Soviet pack sandbags and pine.
- The lowest course reaches 80 cm into the ground (only the bottom course is stretched, so it costs no triangles). The pit is fitted to the ground:
  - a least-squares plane through a 3×3 grid of ground samples under it, tilted up to 20°;
  - then sunk by its worst gap, up to 75 cm for high pits and 30 cm for slit pits (their slit has to stay at the barrel).

  A pit sunk into a slope reads as dug in; one standing proud of it would show a gap under the bags.

| Style | Guns | Walls |
|---|---|---|
| Slit, 2 bags thick | PaK 38, PaK 40, 53-K, ZiS-3, Maxim | One course is left out at barrel height along the front and side walls. The corners (piers) are laid full height and a timber beam resting on them carries the courses above. The course height is tuned per gun (15–19 cm) so the slit is centred on the barrel. The barrel shows through the slit. |
| Slit, 3 bags thick | BS-3 (100 mm) | as above |
| High, 2 bags thick | Flak 37, Soviet 80 mm mortar | 8 courses (142 cm) |
| High, 3 bags thick | Flak 88, leFH, 21 cm Mörser, Soviet 120 mm mortar, M1938, ML-20, B-4 | 8 courses for the 120 mm mortar, 9 (160 cm) for the rest. |

On high pits:
- the outer corners step down for the top two courses;
- the front wall stands far enough ahead of the muzzle for a shot at 40° (mortars) or 25° (howitzers) to clear it.

The Flak and the B-4 fire over the wall even at 0°.

Triangles (LOD0, after the cut):
- German pits: 4.7k–9k.
- Soviet pits:
  - Maxim: 7k;
  - other slit pits: 15k–24k;
  - mortar pits: 12k–20k;
  - howitzer pits: 30k–39k.

The B-4 pit is about 12.4 m across. LODs come from the LargeProp group.

## Code

| Piece | What it does |
|---|---|
| `Units/TeamWeapons/DigIn/UTeamWeaponDigInComponent` | Default subobject on `ATeamWeapon`. Finds the pit mesh `/Game/RTS_Survival/Meshes/Props/TeamWeaponDigIn/SM_TWDigIn_<gun Blueprint>`: it strips `_C` and `_Soviet` and walks up parent Blueprints. A `WallMeshOverride` on the component replaces that. It loads the mesh asynchronously, spawns the wall, places it on the ground, and applies and removes the protection by the same factors. It tells the controller through `IDigInUnit` (`OnDigInCompleted`, `WallGotDestroyedForceBreakCover` when the mesh fails). Its settings per gun Blueprint: build time, health and damage multipliers. |
| `Units/TeamWeapons/DigIn/ATeamWeaponDigInWall` | The pit actor: a scene root plus the mesh. The mesh rises relative to the root while building, so the root must not be the mesh. Collision: one simple box per wall run (`collision_boxes` in the layout; slit walls leave the slit open between a box below it and one from the beam up), used for complex queries too. The bags' own triangles let a ray slip through the seam between two courses. Responses: `SetupWallActorMeshCollision` (blocks the opponent's weapon channel, overlaps building placement); Visibility is ignored, so clicks reach the gun. `TakeDamage` forwards to the gun and returns the gun's result, so kills are credited. It creates one `URTSNavCollisionEnvObstacle` (NavArea_Null) per `NavBox_<n>` socket. The boxes stand upright on the pit's yaw, 300 cm taller than the wall: boxes tilted with a pit on a slope did not carve, and the tall ones beside the entrance would lean into it. |
| `ATeamWeaponController` | `ExecuteDigIn` deploys first if needed, then `StartTeamWeaponDigIn`. `ApplyDigInCommandCard` / `RestoreCommandCardAfterDigIn` handle the card. `RemoveTeamWeaponDigIn` runs on break cover, tow hook and abandon. The out-of-range reposition is gated while dug in. Guards use the pit's `GuardPost_<n>` sockets: `GetIsGuardMisplaced`, `MoveGuardsToGuardPositionsIfMisplaced`, and no engage-flow moves while dug in. A guard left standing inside a wall run (off the navmesh) is set on the nearest walkable spot before it walks to its post (`PlaceGuardOnNavMeshIfStranded`). The old `UDigInComponent` path is gone: no team weapon Blueprint had one. |
| `UHealthComponent` | `Get/SetDamageReductionMultiplier`. |

Sockets on every pit mesh (set by the import script):
- `NavBox_0..4`: one per wall run (front, rear left, rear right, right, left). Location = centre; scale = half extent / 100.
- `GuardPost_0..3`: guard spots inside the pit. The Maxim pit has 2; its third guard shares a post, 70 cm along.
- `ProbeWall`, `ProbeSlit`, `Entrance`, `Inside`: points used by the map test.

The pit folder is in `DirectoriesToAlwaysCook` (`Config/DefaultGame.ini`), because only C++ names the meshes.

## Asset pipeline

1. **Survey.** `Source/Tools/TeamWeaponDigIn/run_twdig.sh survey /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -TWDigInAt=16796,-15808 -TWDigInSurvey` writes `Saved/TWDigIn/tw_digin_survey.json`. Copy it to `CustomMapItems/TeamWeaponDigIn/`.
2. **Generate.** In `CustomMapItems/TeamWeaponDigIn`, run `blender -b --factory-startup -P generate_tw_digin_walls.py -- [preview] [only=<Gun>]` (Blender 4.0). The per-gun table (faction, style, rows, courses, muzzle socket, fire elevation) is `GUNS` at the top. It writes `Exports/SM_TWDigIn_<Gun>.fbx`, `Exports/tw_digin_layout.json` and `Preview/`.
3. **Cut.** `pit_mesh_optimize.py`: `TWDigIn_GerSandbag=0.4`, `TWDigIn_RusSandbag=0.35`. It writes `_Opt.fbx`.
4. **Validate.** `validate_tw_digin_walls.py` checks sizes, entrance and slit openings, materials and the cut. It writes `tw_digin_report.txt`; the current result is 171/171 PASS.
5. **Import.** Copy `Exports/tw_digin_layout.json` to `Source/Tools/TeamWeaponDigIn/`, then run `UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/TeamWeaponDigIn/ue_twdigin_import.py`. The editor must be closed, or the assets must not be open. The script sets materials, complex-as-simple collision, Nanite off, LargeProp and the sockets.

Traps:
- Export centimetre data in a 0.01-unit scene with `FBX_SCALE_ALL`, as the other pit generators do. Data scaled to metres came in at 1/100 after `pit_mesh_optimize.py`.
- Decimating the Soviet bag templates themselves (instead of the optimizer's per-material cut, which keeps the normals) turned them into shards.

## Test

`-TeamWeaponDigInValidate` (`Units/TeamWeapons/Tests/TeamWeaponDigInTestSubsystem`) spawns every team weapon squad Blueprint by class (16, one per gun) in a 4×4 grid. Each squad deploys, digs in, is checked, breaks cover, digs in again and cancels at 20 % of the build. The checks:
- protection: max health ×1.33 and multiplier ×0.8;
- hit transfer: a 200-damage hit on the wall equals a direct hit, and is smaller than before the dig-in;
- traces on the opponent's channel: the wall stops a shot, and the slit lets one through at barrel height;
- navmesh: carved under the front wall, and a path from in front into the pit runs through the entrance;
- crew: operators at their posts, guards at the pit's posts;
- the wall stands on the ground (at most 12 cm gap at 9 points; 20 cm for slit pits);
- command card: while building, while dug in, a move order refused, and restored slot for slot;
- removal: break cover and cancel take off the pit, the health and the multiplier;
- after removal: the navmesh comes back where the front wall stood, and the gun drives out through it.

```bash
Source/Tools/TeamWeaponDigIn/run_twdig.sh flat /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -TWDigInAt=16796,-15808
```
```bash
Source/Tools/TeamWeaponDigIn/run_twdig.sh hills /Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission -TWDigInAt=-41800,-28250
```
Other switches:
- `-TWDigInOnly=<name part>` limits the run to matching Blueprints;
- `-TWDigInDir=` (set by the runner) saves a picture per pit;
- `-CaptureExposureBias=<stops>` brightens the pictures (Ger_FirstMission is lit as night).

The test waits for the pit to be carved into the navmesh before its checks: the mission map is still building its navmesh minutes after loading, and `IsNavigationBuildInProgress` never clears there. A pit with no navmesh just in front of it or behind its entrance (the map's navmesh edge) skips the path and guard checks and logs `skip_nav_checks`.

Results (2026-10-17, DebugGame):

| Run | Result |
|---|---|
| flat (RTS_ToCopySmall) | 486/486 PASS |
| hills (Ger_FirstMission, slopes up to 21°) | 458/458 PASS. Three or four pits in one grid column sit at the navmesh edge and skip their nav and guard checks. One earlier hill run failed on timing instead: the mission map's navmesh updates were slow, and guns waiting for their pit to carve were killed by the map's enemies. |
| `-TeamWeaponQualityValidate` flat (regression) | 267/267 PASS |
| Blender `validate_tw_digin_walls.py` | 171/171 PASS |

Fixed along the way:
- The pit's mesh was its root, so the rising animation moved the whole pit to the world origin.
- Tilted nav boxes did not carve on slopes.
- Uphill ground samples started under the terrain.
- Guards stood inside wall runs and could not path out.

## Not done / open

- The pits are not destructible and have no build progress bar; the rising sandbags show the build.
- A gun dug in with its rear entrance against a cliff or the navmesh edge has guards that cannot walk in; they stay outside.
- The German walls read dark in the test captures. They use the dark camo atlas of the existing German pits.
- The test never drives three paths: tow hook while dug in, abandon while dug in, and Dig In ordered on a packed gun (every subject is already deployed). The first two only call `RemoveTeamWeaponDigIn`, the same removal break cover runs; the third goes through the existing deploy-then-dig-in flag.
- A gun Blueprint without a generated pit (`BP_TW_Lefh18`, `BP_TW_Ger80MM_Mortar`; no squad uses either) reports an error, and Dig In ends without a pit. There is no size-class fallback mesh.

## Squad and gun setup (fixed 2026-10-17)

Where things are set:
- **Subtype:** a `Set Squad Subtype` node with a literal value in each squad's and gun's construction script. Faction copies (`*_Soviet`) inherit it.
- **Gun weapon:** the gun Blueprint's `Weapon Name` class default.
- **Owner:** the `RTS` component, which children override per Blueprint.

`run_twdig.sh roster ... -TWDigInSurvey -TWDigInAllBlueprints` logs all of these for all 23 squad Blueprints. `Tools/TeamWeaponDigIn/ue_tw_roster.py` reads the class defaults; `ue_tw_roster_apply.py` holds the fix that was applied.

What was fixed:

| Blueprint(s) | Was | Now |
|---|---|---|
| 53-K squad and gun (+`_Soviet`) | ZiS-76 | `Squad_Rus_53K_45MM` |
| BS-3 squad and gun | ZiS-76 | `Squad_Rus_BS3_100MM` |
| Soviet 80 mm mortar squad and gun (+`_Soviet`) | 120 mm mortar, weapon `Mortar_40MM` | `Squad_Rus_80mm_Mortar`, `Mortar_80MM_TW` |
| Mörser squad and gun | leFH 18, leFH 105 mm weapon | `Squad_Ger_210MM_Morser`, `Morser_18_210MM_TW` |
| ML-20 squad and gun | B-4 | `Squad_Rus_ML20_152mm` |
| Flak 37 gun | PaK 38 | `Squad_Ger_37mmFlak` |
| Maxim gun (+`_Soviet`) | ZiS-76 / DShK, DShK 12.7 mm weapon | `Squad_Rus_Maxim`, new `Maxim_M1910_7_62MM_TW` (DShK belt and rhythm, 7.62 mm: damage 8 × DamagePerMM instead of 13, pen 13/7 instead of 27/20, belt 250) |
| ZiS-3 gun (+`_Soviet`) | `ZIS_3_76MM` | `ZIS_3_76MM_TW` (same data) |
| leFH gun | `LeFH_18_105MM` | `LeFH_18_105MM_TW` (same data) |
| sFH 18 gun (`BP_TW_Lefh18`, no squad) | leFH 18, leFH weapon | `Squad_Ger_SFH18_150mm`, `sFH18_150MM_TW` |
| German 80 mm mortar squad | Soviet 120 mm subtype, Soviet 120 mm gun, Soviet riflemen, owner 2 | `Squad_Ger_GrW42_80mm`, `BP_TW_Ger80MM_Mortar` (now with its own pit), 4 `Ger_Inf_Kar`, owner 1 |
| `BP_TW_Ger80MM_Mortar` | Soviet 120 mm subtype, owner 2, spotter post 3.7 m to the side | GrW 42 subtype, owner 1, spotter at (−91, −145) like the Soviet 120 mm |

The Rus 80 mm mortar crew carries SVTs (hazmat riflemen); that was left as it is.
