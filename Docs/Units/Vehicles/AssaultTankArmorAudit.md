# Assault-tank armour boxes and vehicle modules

Audit of every `RTS_BP_AssaultTank` child (53 Blueprints, found through the asset registry's parent chain) plus
`Rus_Vehicle_IS-1_Harvest`. Two questions: do they start vehicle modules, and do shells from the side, front and rear
resolve the plate on that side? Runs from 2026-11-05 on `UT_B4Howitzer`.

## Vehicle modules (fix 1)

`ATankMaster::BeginPlay_OnModulesComplete_FinalizeVehicleModules` must be called by the Blueprint after it installs
its hull modules. `RTS_BP_TrackedTankMaster` does that in its `ReferenceCasts` macro, which `Event BeginPlay` runs after
`Parent: BeginPlay`:

```text
ReferenceCasts: InitSelectionComponent -> SetupModules
SetupModules:   ArmorCalculation = GetComponentByClass(ArmorCalculation); if valid:
                SetVehicleModuleProfile(BP_GetVehicleModuleProfileForTankSubtype(RTS.GetSubtypeAsTankSubtype))
                SetRunningGearType(Wheels if the profile is ArmoredCar, else Tracks)
                SetupModule(Engine); SetupModule(Ammo)
                BeginPlay_OnModulesComplete_FinalizeVehicleModules
```

`RTS_BP_AssaultTank` had none of it, so no assault tank ever had modules (audit: `modules_finalized=0` on all of
them, `=1` on the IS-1). The children run `SetupArmor` (ClearArmorSetup + InitArmorCalculation) after
`Parent: BeginPlay`, so after `SetupModules`, the same order as the tracked tanks. Finalization waits a tick for the
turrets, and add-on zones install then, so the cleared and re-registered armour is in place by then.

The same `SetupModules` function was added to `RTS_BP_AssaultTank`, called from its `ReferenceCasts` macro after
`InitSelectionComponent`. The only difference: a `Switch on EVehicleModuleProfile` replaces the Branch on
`Equal (Enum)`, because the editor bridge cannot create the enum-equality node. ArmoredCar goes to Wheels, the other
four profiles to Tracks. The switch has no default pin: a sixth profile value would install no modules until it is
wired. No child overrides that flow. Every child's `Event BeginPlay` calls its parent's, and
`Rus_Vehicle_Su85_Harvest` calls `RTS_BP_AssaultTank`'s directly (it skips `Rus_Vehicle_Su85`'s BeginPlay, see
Open points).

Notes:
- Neither master installs a Tracks or Wheels module. The only module types installed in C++ are turret, weapon and
  add-on armour (`TankMasterTurretModules.cpp`, `AddOnArmorPanelsComponent.cpp`). So "track modules" do not exist on
  any tank yet; the running-gear type only switches the plate rules.
- Embedded guns (`GetHasOwnVehicleModuleMount() == false`) install no Weapon module by themselves. A casemate Weapon
  module would be a Blueprint `SetupModule` call bound to the hull mesh. Not added.
- `BP_GetVehicleModuleProfileForTankSubtype` maps the Sdkfz 251 subtypes to ArmoredCar (Wheels).

## Armour audit tool

`UArmorPlateAuditSubsystem` (`Utils/RTSMapTests/ArmorPlateAudit/`), started by `Tools/ArmorPlateAudit/run_armor_audit.sh`.
It runs an offscreen game and leaves the open editor alone. It spawns each class from `assault_tanks.txt` in turn and
logs, under `RTS_ARMOR_AUDIT`:
- `vehicle`: armour component present, `modules_finalized`, installed modules;
- `mesh` and `plate`: every registered box in actor space, as the hit test covers it (see below);
- `probe`: a shell traced from the left, right, front and rear into the registered meshes, several points per side,
  with the plate the armour calculation resolves and a verdict: `ok`, `NOTE` (side shot resolves a front or rear plate,
  or the reverse), `WRONG` (opposite side or opposite end), `MISS` (trace hit no registered mesh);
- `summary` per vehicle and one `RESULT` line.

`-ArmorPlateAuditAsDrawn` also re-registers each vehicle's plates at the size the debug draw shows (unit scale) and
probes again (`<name>@asdrawn`). That previews a unit-scale migration without touching any asset.

```bash
PROJECT=D:/UE5Projects/RTS_Survival_AddOnTest/RTS_Survival.uproject \
ENGINE=D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe \
TAG=Name Tools/ArmorPlateAudit/run_armor_audit.sh -ArmorPlateAuditAsDrawn
```

On `UT_B4Howitzer` the map's artillery killed 9 tanks within two seconds in the first run. The audit now waits at
most 3 game seconds for the modules, and all 54 vehicles were audited.

## Root cause: legacy boxes cover extent × scale²

Most wrong-side results are not mirrored or swapped boxes. `InitArmorCalculation` multiplies each box by its
transform scale, and the hit test divides the hit by that scale again. A box authored with extent `e` and scale `s`
therefore covers `e * s²`, while `DebugArmorPlates` draws `e * s`. The box scale fix (B4b, 2026-10-27,
`ArmorBoxScaleTests.cpp`) kept that coverage on purpose and only evaluates unit-scale boxes first.

Plates are tested in hierarchy order (front, then rear, turret, and sides last), and the first box that contains the
hit wins. So an inflated front, rear or turret box swallows side shots. Examples from the dump, in actor space:
- E25 `Plate_Front` (scale 2.25 × 6.75 × 4.5) spans Y ±1456 cm on a 380 cm wide hull. Every side shot resolves
  `Plate_Front` or `Plate_Rear`.
- Su-85, Su-100, Su-122: `Turret_Front` (scale 4 × 4.5 × 3) covers the whole vehicle. Rear shots resolve
  `Turret_Front` (WRONG).
- IS-1 Harvest: `Plate_SideLeft` and `Plate_SideRight` are placed correctly (centres at Y -95 and +96). Inflated, each
  one covers both sides, and `Plate_SideLeft` is tested first, so right-side shots resolve it.
- Sdkfz 251: `Plate_Front` and `Plate_Rear` (scale 4 × 5 × 4) both cover the whole vehicle, so rear shots resolve
  `Plate_Front`.

## Genuine authoring errors

Box centres are the designer's placement (scale inflates around them).
- Brummbar, DirectSiege, LateWar: `Plate_SideLeft` sits at the rear centre (X -178, Y -1, scale 1.5 × 4 × 3). It is a
  rear box with the wrong label. `Plate_SideLowerLeft` repeats `Turret_SideLeft`'s box on the superstructure (X -10,
  Y -76, Z 228). So there is no lower-left hull plate. The right side has `Plate_SideLowerRight` at Z 79, Y +81 and no
  `Plate_SideRight`. With drawn-size boxes the right side resolves `Plate_SideLowerRight`, but the left side still
  resolves `Turret_SideLeft` or `Turret_Rear`.
- Brummbar_LateWar: `Turret_SideLeft` and `Turret_SideRight` have equal armour and hierarchy. Their order is a tie,
  so the inflated `Turret_SideRight` is tested first and catches left shots too.
- Su-85 family: `Plate_SideLowerLeft` sits at the front centre (X 233, Y 0, Z 18), so it is a front plate with the
  wrong label. Check also `Plate_Rear` (X -9, Z 196, the casemate's rear wall height) against `Turret_Rear` (X -229,
  Z 123, the hull rear): they look swapped.
- Tiger H1 RailBunker: check `Plate_RearUpperGlacis`, which sits at roof height in the middle (X 35, Z 310).

## Per-vehicle results

Run `BeforeAsDrawn` (`Saved/Logs/ArmorPlateAudit_BeforeAsDrawn.log`), with Blueprints as they were before any change.
"As drawn" is the what-if with every plate at unit scale.
- Side shots: left and right probes that resolve a plate of their own side (`Plate_Side*`, `Plate_SideLower*`,
  `Turret_Side*`), out of the probes that hit the vehicle.
- Opposite-side or wrong-end: the audit's `WRONG` count (a left shot resolving a right plate, a rear shot resolving a
  front plate, and the reverse).
- Middle: the plates the two middle-of-side probes resolve.

Plate codes: F/FUG/FLG front, upper and lower glacis; R/RUG/RLG rear; SL/SR side; SLL/SLR lower side; TF/TR/TSL/TSR
turret (superstructure) front, rear and sides; TSAR turret sides and rear; TM mantlet; TC cupola.

| Vehicle | Modules finalized | Side shots on own side plates, before / as drawn | Opposite-side or wrong-end shots, before / as drawn | Middle of left side, before → as drawn | Middle of right side, before → as drawn |
|---|---|---|---|---|---|
| Sdkfz-251_75mm | no | 0/20 / 7/20 | 7 / 2 | F → SL | F → SL/SR |
| Sdkfz-251_75mm_HeavyAA | no | 0/20 / 6/20 | 6 / 3 | F → SR/SL | F → SL/SR |
| Sdkfz-251_75mm_RadixiteCharged | no | 0/20 / 7/20 | 7 / 2 | F → SL | F → SL/SR |
| Sdkfz_234_3 | no ArmorCalculation | - | - | - | - |
| Sdkfz_234_3_IncendiarySupport | no ArmorCalculation | - | - | - | - |
| Sdkfz_234_3_StummelAssault | no | 16/22 / 16/22 | 1 / 1 | SL/SR → SL/SR | SR → SR |
| E25 | no | 0/16 / 10/16 | 0 / 0 | R → SL | R → SR |
| E25_Assault | no | 0/22 / 11/22 | 0 / 1 | R → SL/SR | R → SR |
| E25_Hunter | no | 0/26 / 15/26 | 0 / 0 | R → SL | R → SR |
| JagdE100 | no | 4/30 / 12/30 | 0 / 0 | TF → SLL/TSL | TF → SLR/TSR |
| JagdPanther_Bollwerk | no | 13/20 / 16/20 | 0 / 0 | SLL/TSL → SLL/TSL | SLR/TSR → SLR/TSR |
| JagdPanther_Flamer | no | 2/18 / 11/18 | 0 / 1 | F/TSL → SLL/TSL | F/TSR → SLR/TSR |
| JagdPanther | no | 2/18 / 11/18 | 0 / 1 | F/TSL → SLL/TSL | F/TSR → SLR/TSR |
| JagdPanther_Pfeil | no | 2/20 / 11/20 | 0 / 1 | F/SLL → SLL/TSL | F/SLR → SLR/TSR |
| JagdTiger | no | 0/22 / 10/22 | 0 / 0 | TSAR → SL/TSAR | TSAR → SR/TSAR |
| JagdTiger_Scrap128 | no | 0/22 / 10/22 | 0 / 0 | TSAR → SL/TSAR | TSAR → SR/TSAR |
| SturmTiger | no | 6/28 / 17/28 | 0 / 0 | TSL → SLL/TSL | TSR → SLR/TSR |
| SturmTiger_Scatterer | no | 6/24 / 17/24 | 0 / 0 | TSL → SLL/TSL | TSR → SLR/TSR |
| TigerH1_Rail_RailBunker | no | 18/28 / 24/28 | 0 / 0 | SL → SL | SR → SR |
| Hetzer_Flame | no | 3/24 / 14/24 | 2 / 0 | RUG/TR → SLL/TSL | RUG/TR/TM → SLR/TSR |
| Hetzer | no | 0/22 / 9/22 | 2 / 0 | RUG/TR → SLL/TSL | RUG/TR/TM → SLR/TSR |
| Hetzer_Mortar | no | 0/24 / 10/24 | 2 / 0 | RUG/TR → SLL | RUG/TR → SLR |
| Marder | no | 6/18 / 15/18 | 2 / 0 | TF → TSL/TF | TF → SLR |
| Marder_DualBarrel | no | 6/18 / 15/18 | 2 / 0 | TF → TSL/TF | TF → SLR |
| Pz1_15CM | no | 0/22 / 6/22 | 0 / 0 | TSAR/TR → SLL/TSAR | TSAR/TR → SLR/TSAR |
| Pz1_15CM_DirectFire | no | 8/22 / 11/22 | 0 / 0 | TSAR/SL → SL | TSAR/SR → SR |
| Pz1_15CM_IndirectFire | no | 0/22 / 6/22 | 0 / 0 | TSAR/TR → SLL/TSAR | TSAR/TR → SLR/TSAR |
| PanzerJager1 | no | 4/20 / 8/20 | 0 / 0 | FUG → SLL/TSAR | FUG → SLR/TSAR |
| PanzerJager1_AmbushShield | no | 6/20 / 8/20 | 0 / 0 | SLL/FUG/TSAR → SLL/TSAR | SLR/FUG/TSAR → SLR/TSAR |
| PanzerJager1_MobileBattery | no | 6/24 / 8/24 | 0 / 0 | FUG → SLL/TSAR | FUG → SLR/TSAR |
| PanzerJager_Laser | no | 6/22 / 8/22 | 0 / 0 | FUG → SLL/TSAR | FUG → SLR/TSAR |
| SDKFZ-140_Bunker | no | 25/26 / 26/26 | 1 / 0 | SLL/SL → SLL/SL | SLR/SR → SLR/SR |
| Wespe | no | 22/28 / 22/28 | 0 / 0 | SLL/TSL → SLL/TSL | SLR/TSR → SLR/TSR |
| Wespe_CamouflagedNest | no | 20/26 / 20/26 | 1 / 1 | SLL → SLL | SLR → SLR |
| Wespe_RadixiteHowitzer | no | 22/28 / 22/28 | 0 / 0 | SLL/TSL → SLL/TSL | SLR/TSR → SLR/TSR |
| Brummbar | no | 3/24 / 7/24 | 3 / 0 | TSL → TSL | TSL → SLR/TSR |
| Brummbar_DirectSiege | no | 3/24 / 7/24 | 3 / 0 | TSL → TSL | TSL → SLR/TSR |
| Brummbar_LateWar | no | 3/26 / 6/26 | 3 / 0 | TSR → TSL | TSR → SLR/TSR |
| JagdPanzerIV70 | no | 15/16 / 15/16 | 1 / 1 | SLL → SLL | SLR → SLR |
| JagdPanzerIV70_Ambush | no | 19/22 / 19/22 | 1 / 1 | SLL/SL → SLL/SL | SLR/SR → SLR/SR |
| JagdPanzerIV70_Siege | no | 15/16 / 15/16 | 1 / 1 | SLL → SLL | SLR → SLR |
| StugIII | no | 4/16 / 8/16 | 0 / 0 | TSAR/SLL → SLL/TSAR | SR/TSAR → SR/TSAR |
| Rus BM13 | no ArmorCalculation | - | - | - | - |
| Rus BM13_Forge | no ArmorCalculation | - | - | - | - |
| Rus ISU130 | no | 29/30 / 29/30 | 0 / 0 | SL → SL | SR → SR |
| Rus ISU152 | no | 29/30 / 29/30 | 0 / 0 | SL → SL | SR → SR |
| Rus SU76 | no | 28/28 / 28/28 | 0 / 0 | SL → SL | SR → SR |
| Rus Su85 | no | 0/28 / 14/28 | 6 / 1 | TF → SL/SR | TF → SR |
| Rus Su85_Harvest | no | 0/28 / 14/28 | 6 / 1 | TF → SL/SR | TF → SR |
| Rus Su85_Soviet | no | 0/28 / 14/28 | 6 / 1 | TF → SL/SR | TF → SR |
| Rus Su100 | no | 0/22 / 8/22 | 9 / 0 | TF → SL/R | TF → SR/R |
| Rus Su100_Soviet | no | 0/22 / 8/22 | 9 / 0 | TF → SL/R | TF → SR/R |
| Rus Su122 | no | 0/28 / 14/28 | 6 / 1 | TF → SL/SR | TF → SR |
| Rus IS-1_Harvest | yes | 6/28 / 12/28 | 6 / 0 | SL → SL | SL → SR |

The vehicles that already resolve their sides well (ISU-130, ISU-152, SU-76, the JagdPanzer IV/70 family) have scale 1
on every box: they were authored at unit scale. That is the model for a migration.

## Status (2026-11-05)

- **Fix 1 is saved and verified.** The user saved `RTS_BP_AssaultTank` at 10:08. The audit run `AfterModules`
  (`Saved/Logs/ArmorPlateAudit_AfterModules.log`) shows `modules_finalized=1` with Engine and Ammo installed on all
  50 assault tanks that have an `ArmorCalculation`. The 4 without one are listed under Open points. No subtype hit the
  "no module profile" error. The "Modules finalized" column in the table above shows the state before the fix.
  The unchanged asset is backed up at `Saved/BlueprintBackups/AssaultTankArmor_2026-11-05/`.
  - `run_panels_validate.sh` on `Ger_Vehicle_SDKFZ-140_Bunker` (INJECT of its panel hull, no
    `-AddOnArmorPanelsStartModuleSetup`; logs `AddOnArmorPanels_AssaultModules_Sdkfz140*.log`): modules finalize on
    their own, with no NOTE, and `modules left=9 right=10`. All six left panels are covered, and damage, crew repair
    and full service pass.
  - One check fails: "the right side keeps its armour". The armour values do not change. The same middle-right
    reading point resolves `Plate_SideRight` (50 + 15) before the crew repair and `Plate_SideLowerRight` (25 + 15)
    after it. That point sits on the Z = 128 edge between the unit-scale `Plate_SideLowerRight` box (Z 68 to 128) and
    the legacy scaled `Plate_SideRight` box (Z 76 to 220). A small change in the hull's pose flips it.
  - The earlier run with `-AddOnArmorPanelsStartModuleSetup` (Prep11) passed, but it had no Engine or Ammo modules.
    Now side hits also damage those modules and the crew repairs them, which likely moves the tank between the two
    readings. The check now logs both readings (`right now ... (intact ...)`). This is a test-point placement issue
    on the SDKFZ-140's overlapping boxes, not a module failure. Left as a FAIL.
- **Fix 2, option (a) chosen by the user: Brummbar family saved by the user at 10:14 and checked.** Both changes compile:
  - `Ger_Vehicle_Brummbar` `SetupArmor`: `Plate_SideLowerLeft` now takes a mirror of the `Plate_SideLowerRight`
    transform, built from the `Right*` getters with Y, yaw and roll negated. Both boxes are ±32, so only the
    transform needs mirroring. The old `Left*` getters and their `Make Transform` stay in the graph, unconnected.
    The `Extrasidesarmor` box at the rear centre is relabelled from `Plate_SideLeft` to `Plate_Rear`.
    `Brummbar_DirectSiege` inherits this.
  - `Ger_Vehicle_Brummbar_LateWar` `SetupArmor` (literal transforms): `Plate_SideLowerLeft` moved from
    (-5.75, -77.59, 207.5) × (2, 3, 2) to (-5.75, -78.57, 58.96) × (7, 3, 2), the mirror of `Plate_SideLowerRight`.
    `ExtraSides` is relabelled from `Plate_SideLeft` to `Plate_Rear`.
  - Not changed: the LateWar tie between `Turret_SideLeft` and `Turret_SideRight` (an ordering effect, not a label),
    and the Su-85 and RailBunker checks.
  - Originals are in the same backup folder.
  - Check run `BrummbarCheck` (`Tools/ArmorPlateAudit/brummbar_check.txt`, with `-ArmorPlateAuditAsDrawn`):
    - The dump shows `Plate_SideLowerLeft` at the lower hull (Z -48 to 207, the mirror of the right box) and the
      rear-centre box as `Plate_Rear`. All three Brummbars finalize their modules.
    - With legacy coverage, left shots still resolve `Plate_Rear`, `Turret_SideLeft` and `Turret_Front`; the
      inflated boxes come first in the check order. In the as-drawn preview the middle-left shots now resolve
      `Plate_SideLowerLeft`; before this fix they resolved `Turret_SideLeft`.
    - LateWar's middle-left shot still resolves `Turret_SideRight` (WRONG), from the tie between its two
      superstructure side boxes.
- The wider decision, kept for reference:
  - (a) Fix only the authoring errors above. Side shots on most assault tanks keep resolving front, rear or turret
    plates (legacy coverage).
  - (b) Move the assault tanks' plates to unit scale (drawn size), as previewed in the table. Opposite-side results
    drop to 0 or 1 per vehicle, and side shots on side plates roughly double. B4b left unit-scale authoring as the
    way forward; this completes it for these vehicles, at the cost of a balance change: every hit meets different
    armour. Some drawn boxes still reach past the centre line (Sdkfz 251, Su-85, E25_Assault), so (b) still needs
    per-vehicle box tuning.
  - How to convert one box through the editor bridge: in each `SetupArmor`, `Make Transform` takes Location,
    Rotation and Scale from function-local variables (for example the Brummbar's `LeftLocation`, `TurretLeftScale`).
    `Make Box` Min and Max are pin literals. So converting a box takes three patch steps (`apply_graph_patch`) and no
    local-variable edit: unwire the Scale pin, set Scale to 1,1,1, and multiply Min and Max by the old scale.

## Open points

- No `ArmorCalculation`: `Ger_ArmoredCar_Sdkfz_234_3`, `..._IncendiarySupport` (only `StummelAssault` adds one),
  `Rus_ArmoredCar_BM13`, `BM13_Forge`. They get neither modules nor armour plates. Giving them armour means choosing
  plate values.
- `Rus_Vehicle_Su85_Harvest`'s `Event BeginPlay` calls `RTS_BP_AssaultTank`'s, skipping `Rus_Vehicle_Su85`'s
  `ReferenceCasts`/`SetupArmor`. Its armour comes from the construction script only.
- `Ger_Vehicle_SturmTiger_Scatterer`: `Turret_SideLeft/Right` carry 15 `AddOnArmorValue` from its own `SetupArmor`,
  permanently.
- `RTS_BP_TrackedTankMaster` children were not audited. The IS-1 Harvest shows they have the same legacy-coverage
  effect.
