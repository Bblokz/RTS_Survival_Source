# Field defenses (HQ training menu)

Field defenses are emplacements that a trainer builds from its training queue. The player picks one in the **Field Defenses** tab of the German HQ's training menu, places it on the map, and the HQ's queue builds it there.

This is not the field construction ability of squads (`UFieldConstructionAbilityComponent`, hedgehogs and the like). The two share nothing but the construction preview actor, and each has its own preview mode.

## What can be built

| Slot | Expansion type | Requirement |
|---|---|---|
| Pak 40 Pit | `BTX_GerPak40Pit` | none |
| Flak 37 Pit | `BTX_GerFlak37Pit` | none |
| Puppchen Pit | `BTX_GerPuppchenPit` | none (15 s build, see [../Units/GerSmallPits.md](../Units/GerSmallPits.md)) |
| Panzerschreck Foxhole | `BTX_GerPanzerschreckPit` | none (12 s build) |
| leFH 18 Pit | `BTX_GerLeFH18Pit` | Communication Center (the tier II building) |
| sFH 18 Pit | `BTX_GerSFH18Pit` | Communication Center (the tier II building) |
| Flak 88 Emplacement | `BTX_GerFlak88Pit` | Communication Center (see [../Units/GerFlak88Pit.md](../Units/GerFlak88Pit.md)) |

The Communication Center is the tier II building; Bas confirmed this on 2026-10-15. It is the tier II building because every existing tier II and III requirement already points to it: the Airbase, the Experimental Units Factory and five squad options. The Armory, the other tier II building in the HQ's economy tab, is required by nothing. Commit `321b314b` ("Update T3 factory requirement to med factory + comm center") moved the T3 factory's requirement from the Armory to the Communication Center.

The four pits are no longer expansion options of the HQ. Their training time is `BxpT2BuildTime`, the construction time they had as expansions. Their cost is the expansion cost from `BxpDataHelpers`. The cost is paid per second while the item trains, like any other training item.

- **Slots:** `UTrainingLayoutDataAsset::MakeDefaultHeadquartersLayout`, using `FTrainingLayoutSlot::FieldDefense`.
- **Options and requirements:** `UTrainingMenuManager::InitAllGameFieldDefenseTrainingOptions`.
- **Icons:** the rows `Train_BTX_GerPak40Pit`, `Train_BTX_GerFlak37Pit`, `Train_BTX_GerLeFH18Pit`, `Train_BTX_GerSFH18Pit` and `Train_BTX_GerFlak88Pit` of `DT_TrainingOptions`. They use the `SB_OptionImage_GerPit_*` brushes.

## Flow

1. **Click.** A left click on a field defense checks its requirement, then calls `ACPPController::StartFieldDefensePlacement`. If the trainer already has 8 planned field defenses, the error *"Cannot plan more than 8 field defenses ahead!"* (`EPlayerError::Error_TooManyFieldDefensesPlanned`) is shown instead.
2. **Placement.** The construction preview runs in its own mode, `EConstructionPreviewMode::Construct_FieldDefense` (controller mode `FieldDefensePreviewMode`). The spot is valid when:
   - the whole grid-aligned footprint (`TGameBxpConstructionRules`, 1000 to 1400 cm) is free;
   - the ground under the footprint is flat enough (see *Incline* below);
   - the spot is inside one of the player's building radii.
   
   Hold shift to keep placing more of the same pit.
3. **Queued.** The placement click leaves an `AStaticPreviewMesh` at the spot. A box on the building placement channel blocks exactly the checked tiles, so later previews and other buildings cannot overlap the planned footprint. Player units standing in the footprint are moved out. The click then calls `UTrainerComponent::AddFieldDefenseToQueue`. The queue item carries a `FieldDefenseId` into the trainer's `FTrainerFieldDefenses`, which keeps the spot, the rotation, the preview and later the expansion.
4. **Its turn.** Items ahead of it train first, whatever they are (units, trucks, other pits). The preview waits. When the item becomes the active item:
   - `ARTSAsyncSpawner::AsyncSpawnFieldDefenseExpansion` loads and spawns the normal expansion class. This load runs apart from the player's own expansion placement, so the two never cancel each other.
   - `ABuildingExpansion::StartFieldDefenseConstruction` builds the pit at the preview's spot. Units are moved out again and the preview is removed.
   - The item does not spend time or resources before its pit stands.
5. **Building.** Every queue tick syncs the pit to its item through `ABuildingExpansion::SyncFieldDefenseConstructionProgress`: the progress bar (`UTimeProgressBarWidget::SyncProgressBar`) and the construction animation. The construction animation puts the original materials back slot by slot. The pit pauses whenever the queue pauses: a player pause, insufficient resources, or the HQ packing up. When construction starts, the queue clock restarts, so the queue ticks, the HQ's bar, the menu clock and the pit's bar all line up.
6. **Done.** When the item completes, `FinishFieldDefenseConstruction` finishes the pit and the trainer gets `OnTrainingComplete` with the pit. A finished pit has no expansion owner and belongs to nobody's queue.

## Limits and cleanup

- **Planned limit:** at most `MaxPlannedFieldDefenses` (8) field defenses per trainer may wait for their construction. The one under construction does not count.
- **Cancel:** a right click on the item with the queue paused removes the last placed one of that type. Its preview, or its unfinished pit, is destroyed and the paid part is refunded.
- **Requirement lost:** an item whose requirement fails while it is queued is removed and refunded, like a unit item. Its preview or unfinished pit is destroyed.
- **Pit destroyed while building:** the one-second requirement check removes the item. Nothing is refunded, as for a regular expansion.
- **HQ dies:** `ANomadicVehicle::UnitDies` calls `UTrainerComponent::OnOwnerDies`. That stops the queue and destroys every planned preview and the pit under construction. Finished pits stay. The trainer's `EndPlay` does the same for an owner that is destroyed without dying first.
- **HQ packs up:** the queue pauses, so the pit under construction pauses with it. The previews stay and the queue resumes when the HQ unpacks.

## Incline

`ACPPConstructionPreview::IsFieldDefenseInclineValid` traces the ground at a 5×5 grid over the footprint tiles, on the landscape channel, from 20 m above to 20 m below the preview. Units on the spot therefore do not count as ground. It then fits a plane through the samples. The spot is valid when:
- the plane's incline is at most `DegreesAllowedOnHill` (5°);
- no sample lies more than `MaxHeightDeviationFromPlane` (75 cm) above or below the plane (bumps, ditches);
- every sample found ground.

The preview stats show the plane's incline. The constants live in `DeveloperSettings::GamePlay::Construction::FieldDefense`.

## Testing

`-FieldDefenseValidate` on `UT_Nomadics` runs `UFieldDefenseTestSubsystem` (95 checks). It needs a rendered viewport for the mouse step, so run it with `-RenderOffscreen` rather than `-NullRHI`; no window appears. The map holds a German HQ truck on a navmesh. The truck's expansion montage never finishes headless, so after 30 s the scenario calls `ShippingTest_ForceFinishConversionToBuilding`, which is compiled for map tests too. `Test_TrainingUI` does not work: it has no navmesh, so the truck cannot move to its building spot. The scenario:
1. expands the HQ and checks the tab, the removed expansion options and the Communication Center requirement;
2. drives the controller the way the training menu does;
3. plans a Pak 40 pit behind a harvester in training and checks that the preview waits for its turn;
4. checks that the planned footprint blocks the spot and half a footprint over;
5. checks that a 10° slab is refused and a 2° slab accepted, with the measured incline within 0.5°;
6. checks that the pit is built at its spot and its bar stays within 1.25 s of the queue item, also across a 3 s pause;
7. cancels a planned pit (nothing refunded) and a pit under construction (exactly the paid ticks refunded);
8. destroys a pit while it is built and checks that it leaves the queue and the next pit starts;
9. spawns a Communication Center, checks that leFH 18 and sFH 18 unlock, plans a leFH pit and kills the center, which removes the pit and its preview;
10. does what the player does:
    - a click on the Pak 40 item in the Field Defenses tab;
    - the viewport cursor over a free spot, with the preview following it;
    - a left click there;
    - two right clicks on the item: the first pauses the queue, the second cancels the pit.

    The cursor and clicks go straight to the game's scene viewport, so the machine's own cursor stays where it is. `Slate.EnableSyntheticCursorMoves` is off during this step, so Slate does not move the viewport cursor back to the real one. The press and the release come one frame apart, as from a player.
11. checks that eight pits can be planned and the ninth is refused;
12. checks that killing the HQ destroys the previews and the unfinished pit while the finished pit stays.

```bash
MSYS_NO_PATHCONV=1 UnrealEditor-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_Nomadics -game -unattended -nosound -RenderOffscreen -windowed -ResX=1920 -ResY=1080 -benchmark -fps=30 -DisablePlugins=Fab -FieldDefenseValidate
```

Grep the log for `RTS_FIELD_DEFENSE_TEST`; the last line holds `RESULT PASS` or `RESULT FAIL` with the number of failed checks.

To see the moments themselves, drop `-RenderOffscreen` and add `-FieldDefenseCapture=<folder>`. A game window opens and saves six screenshots with the UI:
1. the planned pit waiting behind the unit;
2. the pit building;
3. the built pit;
4. eight planned pits with the error;
5. the cursor-placed pit;
6. the field after the HQ dies.

The scenario holds the camera for the whole run and aims it itself. Edge scrolling reads the machine's real cursor even in an offscreen run, so wherever the cursor rests could scroll the camera off the test spots and fail the mouse step. In screenshots, a camera scrolled away from the player's vision shows the fog of war as a black frame. If the window holds the mouse, the cursor click is skipped with a `SKIP` line and the pit is planned through the placement path. Run it only with the user's go-ahead; the window takes the screen.

To see the tab itself, run the offscreen UI capture with `-UISnapshot -UISnapshotDelay=35 -UISnapshotFinishTruckAnims -UISnapshotTrainingTab=1` on `Test_TrainingUI`. The trainer captures in `Saved/UISnapshot/` then show the HQ's Field Defenses tab. That map has no Communication Center, so the leFH and sFH pits show as locked.

## Adding a field defense

1. The expansion needs a `TGameBxpConstructionRules` entry with a footprint, an entry in the async spawner's `BuildingExpansionMap` and `BxpPreviewMeshMap`, and expansion data.
2. Add its option in `InitAllGameFieldDefenseTrainingOptions`, with a requirement if it needs one.
3. Add a `Slot::FieldDefense(...)` to the trainer's layout.
4. Add a `Train_<type>` row to `DT_TrainingOptions` for its icon.

## Field trench (Barriers tab)

The HQ's fourth tab, Barriers, holds procedural defenses that the player drags out. The first one is the field trench, `BTX_GerFieldTrench`. It does not go through the trainer's queue.

- **Picking it:** `UTrainingMenuManager::OnLeftClickFieldDefenseItem` catches the trench and calls `ACPPController::StartFieldTrenchPlacement`, which turns on `EPlayerBuildingPreviewMode::FieldTrenchPreviewMode`.
- **Laying it out:** `UFieldTrenchPlacementComponent`, which lives on the controller, handles the clicks (`Player/ConstructionPreview/FieldTrenchPlacement`).
  - The first click sets the start.
  - The trench then follows the mouse. The path is recorded every 50 cm and planned again whenever it changes.
  - The second click confirms the trench. Both ends always get an End cap.
  - Right click, Escape, or the HQ dying removes a trench that is not confirmed yet.
  - The trench is cut short before the first piece that would lie outside the build radii or lean more than 15°. If no piece fits, the whole plan shows in red.
- **Planning:** `FieldTrenchLayout::PlanAlongPath` (`Buildings/FieldTrench`) chains kit pieces along the path, using straights of 125, 250 and 500 cm and bends of 15, 30 and 45° (Corner_90 above 75°). It works in the kit's port contract (`GerDefenses/FieldTrenches/Exports/field_trench_kit.json`, flipped to Unreal space).
- **Uneven ground:** `ARTSFieldTrench::PlanOnGround` swaps 5 m straights for shorter ones when the ground strays more than 20 cm from a piece.
- **Paying and digging:** `ARTSFieldTrench` holds one static mesh component per piece and lays each piece onto the ground, tilted with the slope. Pieces are dug in order, at 1 s per metre.
  - Each piece is paid for when its digging starts: `GerFieldTrench_RadixitePerMetre` (10 Radixite per metre) times its length. The tooltip cost is that per-metre price.
  - While the player cannot pay, digging waits and checks again twice a second.
- **Cover:** built pieces give their `cargo_N` sockets to the cover finder through `IRTSCoverSocketSource`. Soldiers are snapped onto the sockets 60 cm under the ground, as in `BP_TrenchMaster` trenches.

Map tests:
- `-FieldDefenseValidateTrench` on `UT_Nomadics`: the menu click, the two clicks along a bent path, digging with per-metre payment, waiting while the player cannot pay, and cancelling.
- `-CoverFinderValidateRuntimeTrench` on `TestCover/TestFieldTrench`: runtime cover publishing, a piece removed and added again, a dug trench on uneven ground, and the full stand-up cycle in a runtime trench.
