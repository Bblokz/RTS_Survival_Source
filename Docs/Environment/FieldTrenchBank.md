# Field trench earth bank and vehicle ramp

Since 2026-11-12, every field trench kit piece (German and Soviet) has an earth bank. Spoil is heaped against the bag walls
up to the bottom of the top three courses, then slopes 2.5 m out to the ground. The trench reads as dug in, but the
landscape is never cut.

- **Why not cut the landscape:** UE 5.5 has no runtime landscape edit in a packaged game.
- **Why not push it down with WPO:** Nanite landscape ignores WPO.
- **Why not edit the heights:** at 1 m per vertex, a height edit can only make a shallow dish.

The trench geometry itself is unchanged: floor, sockets, ports and rim height are the same.

| Part | Where |
|---|---|
| Bank, bag cull, ramp hulls (Blender) | `GerDefenses/FieldTrenches/generate_field_trenches.py` (Soviet: `SovietsStuff/FieldTrenches/generate_sov_field_trenches.py` patches it) |
| Validator | `validate_field_trenches.py` / `validate_sov_field_trenches.py`: bank port profile, buried bags, ramp coverage and closure (630 / 627 checks) |
| Bank material | `/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Materials/M_FieldTrench_Bank`, built by `Source/Tools/FieldTrenches/ue_field_trench_bank_material.py` |
| Import | `ue_field_trench_import.py` (German) and `ue_sov_field_trench_import.py` (Soviet), steps `meshes`, `ramps`; ramps via `ue_field_trench_ramps.py` |
| Ramp component | `ARTSFieldTrench` (`FFieldTrenchPieceRuntime::RampComponent`), `FieldTrenchLayout::GetPieceRampMeshPath`, `FRTS_CollisionSetup::SetupVehicleRampCollision` |
| Test | `-TrenchRampValidate` on UT_Nomadics (`Buildings/FieldTrench/Tests/TrenchRampCrossingScenario`) |
| Look check | `Saved/FieldTrenchBank/bank_capture.py` (throwaway editor, any map) |

## Bank (Blender)

**Shape**
- The bank's height is a function of the distance to the piece's centreline:
  - straight arms: slab distance;
  - bends: arc distance;
  - corners: round joint for the height, square for the cut.
- The cross-section has a flat crest, a linear slope of `BANK_SLOPE_RUN` (250 cm), and runs on below the ground to −30 cm.
- **German:** crest 74 cm, out to 172 cm from the centreline; foot at 422 cm.
- **Soviet:** crest 95 cm, out to 165 cm; foot at 415 cm.
- Both are computed from the kit's `ROWS`.

**Mesh**
- Built on a grid whose breaklines are the port planes, the cut line, the crest edge and the foot, plus the vertex-colour ramp breaks.
- The flat runs are then dissolved.
- On every port plane the height is forced to the straight's profile, so neighbouring pieces meet.
- The outer edge (already at −30 cm) and every port cap drop straight down to `BANK_SKIRT_DEPTH` (−400 cm). Hilly ground that falls away from the piece's plane meets a vertical earth skirt instead of a gap under the bank (user report, 2026-11-12), and an unjoined port (open end, pit neck, a piece still being dug) shows no hollow bank.

**Bag cull**
- The bank is part of the piece mesh, so the hidden-triangle ray cull deletes the bags it buries.
- `cull_under_bank` then removes any bag triangle whose three corners all lie under it.
- Whole kit, bank included: German 62.7k → 35.9k triangles, Soviet 78.2k → 41.1k.

**Vertex colours**
- **R:** fresh spoil, 1 on the crest and 0 at 80 % of the slope.
- **G:** foot blend, 0 → 1 over the lower 60 % of the slope.

## Material

| Input | What drives it |
|---|---|
| Base colour | `RVT_Environment` sampled in the pixel shader, so the bank has the landscape's colour under it. A map without the volume samples black and falls back to `FallbackColor`. |
| Fresh spoil | R², broken up by `T_RTS_PackedNoise`, pulls the colour toward darker `FreshSoilColor`. Grass gets a brown crest. |
| Normal (world space) | Mesh normal plus the RVT normal's xy (`DetailNormalStrength`), so the sand ripples continue. Toward the foot (G), the landscape's own RVT normal takes over, so no crease line shows. |

RVT volumes were checked on Ger_IntroMission (DesertPaintable) and Ger_Trains (DryCastle).

## Vehicle ramp

**Hulls (Blender)**
- `<piece>_Ramp` is an invisible static mesh made only of UCX convex hulls: a straight prism, 15° bend wedges, a corner quarter-frustum, an end half-frustum.
- Its cross-section is the convex hull of the bank and the bags: from the bank foot straight to the outer face of the top course at rim height + 4 cm, then a flat lid over the trench.
- A rigid hull rests on that, so tanks ride over the top courses instead of through them. A loose extra bag on top of the rim can still clip about 15 cm.

**Component (`ARTSFieldTrench`)**
- One ramp component per piece, attached to the piece, so it rises with the piece while it is dug.
- Hidden.
- Physics only, so no trace, sweep or click ever sees it.
- Object type PhysicsBody. It blocks PlayerObj, EnemyObj, EnemyHull and Vehicle.
  - Infantry capsules (query only, ignore PhysicsBody) and hull boxes never touch it.
  - EnemyHull must block, or enemy vehicles drop through it (see `COLLISION_OBJ_ENEMY_HULL`).
- Its collision is off while the piece is only planned.

## Test results (2026-11-12, DebugGame, UT_Nomadics, `-TrenchRampValidate`)

RESULT PASS, 49 checks. All of these cross on the ramp, rising 1.38–1.74 m with the up vector staying at 0.87 or more:

| Vehicle | Lane |
|---|---|
| Panzer IV H | straight on, and at 45° over a 15 m trench |
| Puma (8-wheeled) | straight on |
| Sd.Kfz. 250 half-track | straight on |
| Panzer I harvester | straight on |
| HQ truck | straight on |
| Enemy T-34 | straight on, Soviet kit |
| Enemy BA-11 (wheeled) | straight on, Soviet kit |

- A trench dug under a parked Panzer IV lifts it 1.32 m onto the lid, upright.
- The Kübelwagen never moves in this test, on its trench lane or on its open-ground control lane, so its lane is reported SKIP. The ramp is not the cause.
- Look checks:
  - Ger_IntroMission (desert) and Ger_Trains (grass), German kit.
  - Soviet kit on the desert map.
  - Forced LOD 2 and 3 close up: the bank stays closed, with no holes onto the culled bags.

**Regressions run 2026-11-12 (DebugGame, UT_Nomadics)**

| Test | Result |
|---|---|
| `-FieldDefenseValidate` | PASS |
| Spline join check (`check_field_trench_spline_joins.py`) | PASS, 27 checks |
| `-FieldDefenseValidateTrenchAnchor` | FAIL, 2 checks |
| `-CoverFinderValidateRuntimeTrench` | runtime part 0 failures; scenario FAIL on 1 setup check |

- **Trench anchor:** the Flak 88 bunker's Doors port plans 0 pieces. It is the first plan of the run, made before any trench exists, and planning doesn't read the kit meshes. The other three bunker ports plan normally. This was not traced further.
- **Runtime trench cover:** the failed check is "a player squad, an enemy and the move planner exist", in the hand-placed trench part.

## Known limits

- **Infantry walks through the bank**, just as it walked through the bag walls before. The bank does not affect navigation.
- **Footprint:** the bank widens a trench's footprint from about 4 m to about 8.5 m. It can bury the walls of pits, buildings and parallel trenches next to it.
- **Pit necks** (`TrenchNeck` meshes on pits) have no bank. The trench piece on a pit port ends in its cap face there.
  The neck generators (`gerpit_trench_anchors.py` and the Soviet `*_trench_anchors.py`) still run, but they reuse `trench.MATERIALS`. A regenerated neck would carry an unused third material slot (`FieldTrench_Bank`).
- **Hand-placed `BP_FieldTrench`** uses the kit meshes, so it gets the bank but no vehicle ramp. Only `ARTSFieldTrench` (the spline tool, the player's HQ trench, the enemy AI's trenches) adds ramps.
- **No collision on the bank:** the bank section has collision off (`ue_field_trench_ramps.disable_bank_collision`, run by both imports).
  - Weapon, visibility, camera and landscape-channel traces see the trench walls exactly as before the bank.
  - A trench planned next to another one finds the real ground, not its neighbour's bank.
  - Only the vehicle ramp is solid.
- **Old kits:** `Backup_PreBank/` in both Blender folders.
