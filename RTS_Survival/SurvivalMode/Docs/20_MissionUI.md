# 20 - Survival mission UI

Owner: mission-UI agent. Files: `Source/RTS_Survival/SurvivalMode/UI/`, `Content/RTS_Survival/SurvivalMode/UI/`,
this doc, `Docs/ui_specs/`, `Docs/supply_costs.json`.

Three screens, all in the Frontstahl look of the unit variant menu and the faction unit browser:

1. **Army selection** (`UW_SurvivalArmySelection`, phase `ArmySelection`): drag faction units, each shown as the
   variant the player picked, into a fixed number of slots within a supply budget. Slot 0 is the locked HQ.
2. **Start location picker** (`UW_SurvivalStartLocationPicker`, phase `StartLocationSelection`): cycle the start
   locations (the camera follows) and confirm one.
3. **Survival HUD** (`UW_SurvivalHud`, from `Deploying` on): grace countdown, survival clock, wave counter,
   incoming-wave banner, defeat result with Retry / Main menu.

## Design

### Approach

- C++ owns all logic. Each screen is a C++ `UUserWidget` with `BindWidget` / `BindWidgetOptional` members; the thin
  Blueprint child (`WBP_*` in `Content/RTS_Survival/SurvivalMode/UI/`) holds only the static layout, built from the
  JSON specs in `Docs/ui_specs/`. Only the containers the code cannot work without are `BindWidget`; everything else
  is optional, so a partly built Blueprint still compiles.
- Dynamic pieces build their own widget trees in C++, like `UW_FactionUnitTile` does: roster entries
  (`UW_SurvivalRosterEntry`), category tabs (`UW_SurvivalArmyTab`) and army slots (`UW_SurvivalArmySlot`). They have
  no Blueprints.
- All brushes, fonts and colours come from one data asset, `DA_SurvivalUiStyle` (`USurvivalUiStyleAsset` holding
  `FSurvivalUiStyle`). Every screen points its `M_StyleAsset` at it. At `NativeOnInitialized`, and in the designer
  through `NativePreConstruct`, each screen applies the style to its bound widgets. A brush whose resource is empty is
  skipped, so whatever the Blueprint shows stays. `Docs/ui_specs/post_build_setup.py` fills the asset by copying from
  `BP_W_UnitVariantSelectionMenu` and `BP_W_FactionUnitBrowser`; see "Style asset" below for the full list.
- Sizes live in `SurvivalUiLayout.h`, the same convention as `FactionUnitBrowserLayout`.

### Army selection wireframe (1920x1080 reference; the content box is 1600x920 and shrinks to fit)

```
+--------------------------------------------------------------------------------------------+
|                           [ STARTING ARMY / BREAKTHROUGH DOCTRINE ]   (header plate 560x80) |
| +-- roster 480 -----------------+  +-- army panel (fill) -------------------------------+  |
| | AVAILABLE UNITS               |  | YOUR ARMY                              7 / 12 SLOTS |  |
| | [ALL][INFANTRY][VEHICLES][..] |  | +----+ +----+ +----+ +----+ +----+ +----+           |  |
| | +---------------------------+ |  | | HQ | |PzIV| | 12 | | +  | | +  | | +  |   slot grid |  |
| | |[icon] PANZER IV H     21  | |  | |lock| |var | |    | |    | |    | |    |   (uniform, |  |
| | |       STANDARD PLUS SUPPLY| |  | +----+ +----+ +----+ +----+ +----+ +----+   centred,  |  |
| | |       IN ARMY 1           | |  | +----+ +----+ ...                           resizes)  |  |
| | +---------------------------+ |  |                                                     |  |
| | | ...  (vertical scroll)    | |  | SUPPLY [==========-------------------]    40 / 100  |  |
| | +---------------------------+ |  | Not enough supply: Tiger H1 costs 30, 12 left.      |  |
| +-------------------------------+  | Drag units...        [ CLEAR ]  [   CONFIRM   ]     |  |
|                                    +-----------------------------------------------------+  |
+--------------------------------------------------------------------------------------------+
```

Interaction:

- **Add**: drag a roster entry onto a slot, or double-click it (fills the first free slot).
- **Replace / move**: dropping on a filled slot replaces its unit, with the supply checked against the difference.
  Dragging a filled slot onto another slot swaps the two.
- **Remove**: right-click a slot, or drag its unit out and drop it anywhere that is not a slot (the roster, the
  background).
- **Refusal**: while dragging, each slot under the cursor tints green (accepted) or red (refused). A refused drop
  writes the reason in the feedback line in the warning colour, and it clears after 3 s:
  - "Not enough supply: {unit} costs {n}, {m} left."
  - "Your army can hold at most {n} x {unit}."
  - "The first slot always holds your HQ."
  - "All {n} slots are full."
- **Roster state**: entries that cannot be added right now (over budget, at max copies, no free slot) are dimmed to
  45% but stay draggable, so a drop still explains why. Each entry shows "IN ARMY n" or "IN ARMY n / max".
- **Supply bar**: shows used / budget. The text and bar fill switch to the warning colour at 100%.
- **Confirm** (button, Enter or gamepad Start) builds `TArray<FSurvivalArmyPick>` (HQ at slot 0 plus one pick per
  filled slot, keeping slot indices) and calls `Director->ConfirmArmy(Picks, OutReason)`. When it returns false,
  OutReason goes in the feedback line. When it returns true, the button disables until the director removes the
  widget.
- **Clear** (button, Delete or Backspace) empties every slot except the HQ.
- **Tabs**: ALL / INFANTRY (Squad) / VEHICLES (Tank, Harvester) / TRUCKS (Nomadic) / AIRCRAFT. A tab whose category
  has no units is not created. The roster is sorted by category, then by supply, cheapest first.
- **Tooltip** on entries and slots: name, [variant], variant description, a stat line and "Supply n (max m)". The
  stat line is health and km/h for tanks and health per soldier for squads, read from
  `ACPPGameState::GetTankDataOfPlayer` / `GetSquadDataOfPlayer` (player 1). When there is no game state, the stat
  line is left out.

Icons and names follow the variant menu's order. Variant lookups use `UPlayerUnitVariantManager::GetSpawnerKey(Unit)`
(harvesters are keyed as tanks); table lookups use the raw unit. The icon is the first of:

1. the variant's `TrainingImageBrush`, when `bOverrideTrainingImageBrush` is set;
2. the variant's selection `Icon` texture;
3. the unit's `DT_TrainingOptions` ImageBrush.

The variant badge is `FUnitVariantDefinition::DisplayName` and is hidden for Base. The name is
`FSurvivalArmyEntry::DisplayName`, falling back to the table title and then to the subtype name. The data asset,
the table and every loaded icon texture are held in `UPROPERTY`s while the screen is open.

### Slot grid sizing

Every slot is designed once at the native tile size, 120x164 (the `MI9_Custom_FactionUnitTile` art). A `UScaleBox`
fits it into the grid cell, so text, badges and icon keep their proportions. `SurvivalUiLayout::ComputeSlotGridLayout`
is a pure function:

- It tries every column count and keeps the one that gives the largest cell while every row still fits the area
  height. Cells keep the 120:164 aspect.
- The cell size is clamped to 56..180 units. Below 56 the grid stops shrinking, keeps 56-unit cells and the
  `M_SlotScroll` scroll box scrolls vertically.
- Below 0.7 scale (cells under 84 units) the unit name collapses; the tooltip still has it.
- The area is measured from `M_SlotScroll`'s cached geometry on every tick, and the grid is re-laid out only when
  the size changes. A 14-unit scroll bar reserve and one gap of uniform-grid padding are taken off first. At design
  time the reference area of 1056x640 is used.

| Area | 8 slots | 12 | 16 | 40 | 60 | 100 |
|---|---|---|---|---|---|---|
| 1056x641 (1920x1080 reference) | 180 px, 4x2 | 165, 6x2 | 150, 6x3 | 96, 10x4 | 78, 12x5 | 61, 15x7 |
| 704x409 (raw 1280x720 units, no DPI scale) | 143, 4x2 | 107, 6x2 | 93, 6x3 | 61, 10x4 | 56, scrolls | 56, scrolls |

In game, 1280x720 is DPI-scaled (`UIScaleRule=ShortestSide`) to the same 1920x1080 units, so the first row is what
players see. `analyze_widget_layout` does not apply DPI scaling, so its 1280x720 pass is the harder case, shown in
the second row.

### Start location picker

A 578x242 dialog plate centred at the bottom of the screen: title, `[<] LOCATION NAME / 2 of 5 [>]`, a
`[DEPLOY HERE]` button and a hint line. The root is `SelfHitTestInvisible`, so the camera preview is never covered.

- Prev and Next (buttons, Left/Right, A/D, d-pad) wrap around and call `PreviewStartLocation`.
- Enter, gamepad A or the button calls `ConfirmStartLocation` once and then disables the button.
- `SelectLocationActor(ASurvivalStartLocation*)` lets a world-click handler select a location.
- A location whose `DisplayName` is empty is shown as "Start Location N".
- The previous arrow is the next arrow mirrored (`RenderScale -1,1`).

### Survival HUD

- **Status plate**: 510x130, top centre, 90 units from the top so it clears the main game UI's top bar. It shows a
  phase label above a big clock:

  | Phase | Label | Clock | Extra line |
  |---|---|---|---|
  | `Deploying` | DEPLOYING | `--:--` | |
  | `GracePeriod` | FIRST WAVE IN | `GetGraceSecondsRemaining` | |
  | `Waves` | SURVIVED | `GetElapsedSurvivalSeconds` | WAVE n (`GetWavesLaunched`) |

  It is hidden in the other phases. The plate is refreshed by a 0.25 s timer and also immediately on
  `OnPhaseChanged`, which the HUD unbinds in `NativeDestruct`.
- **Incoming-wave banner**: 511x150, below the status plate. It shows "WAVE n INCOMING" and "from the {edge}", and
  hides itself after 6 s or the duration passed in. Hook (BlueprintCallable):

  ```cpp
  void UW_SurvivalHud::ShowIncomingWaveBanner(int32 WaveNumber, const FText& EdgeLabel, float DisplaySeconds = 0.f);
  void UW_SurvivalHud::HideIncomingWaveBanner();
  static FText UW_SurvivalHud::GetCompassDirectionText(const FVector& WaveEntryLocation, const FVector& BaseLocation);
  ```

  Intended call from the wave director:

  ```cpp
  if (UW_SurvivalHud* Hud = M_Director->GetHudWidget())   // integration request 1
  {
      Hud->ShowIncomingWaveBanner(M_WavesLaunched, UW_SurvivalHud::GetCompassDirectionText(
          Zone->GetActorLocation(), M_Director->GetCurrentPlayerBaseLocation()));
  }
  ```

  The compass helper treats +X as north and +Y as east.
- **Defeat result**: `ShowDefeat(SecondsSurvived, WavesLaunched, BestSeconds, bIsNewBest)` hides the banner and the
  status plate, then shows a full-screen shade with a 750x450 panel: "YOUR LINES HAVE FALLEN", time survived, waves
  faced, best on this map, a NEW RECORD line (only when `bIsNewBest`), and [RETRY] / [MAIN MENU]. When `bIsNewBest`
  is set, the best time shown is `max(BestSeconds, SecondsSurvived)`, so it is right whether or not the director
  saved first. It also sets Game+UI input with the cursor shown and focus on the HUD.
  - **Retry** is `UGameplayStatics::OpenLevel(GetCurrentLevelName)`, the `UW_Defeat` pattern. The game instance keeps
    faction, variants and difficulty, so the survival flow starts again at army selection.
  - **Main menu** is `OpenLevel(UGameMapsSettings::GetGameDefaultMap())`, the escape menu's
    `UMainGameUI::OnEscapeMenuExitToMainMenu` pattern.
- The HUD root is `SelfHitTestInvisible`: only the defeat panel takes the mouse, so RTS input is never blocked.

## Class list (`Source/RTS_Survival/SurvivalMode/UI/`)

| File | What |
|---|---|
| `SurvivalUiStyle.h` | `FSurvivalUiStyle` and `USurvivalUiStyleAsset` (UDataAsset) |
| `SurvivalUiLayout.h` | `SurvivalUiLayout` constants and `ComputeSlotGridLayout` |
| `SurvivalArmyUiTypes.h` | `ESurvivalRosterCategory`, `FSurvivalRosterItem`, `SurvivalArmyUi::` helpers (category mapping and labels, `FormatClock`, `GetCompassDirectionText`) |
| `SurvivalArmyDragOperation.h` | `USurvivalArmyDragOperation` (RosterIndex, SourceSlotIndex) |
| `W_SurvivalArmySelection.h/.cpp` | Army selection screen: all army state and rules |
| `W_SurvivalRosterEntry.h/.cpp` | Roster row, 440x80, self-building; drag source and double-click add; not a button, since a button captures mouse-down and blocks drag detection |
| `W_SurvivalArmySlot.h/.cpp` | Army slot, self-building, scaled to its cell; drop target, drag source, right-click remove, drop tint; also used as the drag visual |
| `W_SurvivalArmyTab.h/.cpp` | Category tab, self-building (`UUnitVariantSelectionButton`, never takes focus) |
| `W_SurvivalStartLocationPicker.h/.cpp` | Start location picker |
| `W_SurvivalHud.h/.cpp` | HUD, banner, defeat result |

Contract signatures are unchanged: `InitArmySelection(ASurvivalModeDirector*)`,
`InitStartLocationPicker(ASurvivalModeDirector*)`, `InitHud(ASurvivalModeDirector*)` and
`ShowDefeat(float, int32, float, bool)`, all still virtual.

## BindWidget names

Widget names in the Blueprints must match exactly. The specs in `Docs/ui_specs/*.json` already use them.

**WBP_SurvivalArmySelection** (parent `UW_SurvivalArmySelection`)

- Required:
  - `M_RosterScroll` (ScrollBox)
  - `M_SlotScroll` (ScrollBox)
  - `M_SlotGrid` (UniformGridPanel, child of M_SlotScroll)
  - `M_SupplyBar` (ProgressBar)
  - `M_SupplyText` (TextBlock)
  - `M_ConfirmButton` (Button)
- Optional:
  - Border: `M_ScreenShade`, `M_HeaderPlate`, `M_RosterPanel`, `M_ArmyPanel`
  - HorizontalBox: `M_TabRow`
  - TextBlock: `M_TitleText`, `M_FactionText`, `M_RosterTitleText`, `M_ArmyTitleText`, `M_SlotCountText`,
    `M_SupplyLabelText`, `M_FeedbackText`, `M_HintText`, `M_ConfirmText`, `M_ClearText`
  - Button: `M_ClearButton`
- Class defaults:
  - `M_StyleAsset` = DA_SurvivalUiStyle
  - `M_DesignPreviewSlotCount` (12): designer only
  - `M_DesignPreviewRosterCount` (8): designer only

**WBP_SurvivalStartLocationPicker** (parent `UW_SurvivalStartLocationPicker`)

- Required: `M_LocationNameText` (TextBlock), `M_PreviousButton`, `M_NextButton`, `M_ConfirmButton` (Button)
- Optional: `M_PanelBorder` (Border); `M_TitleText`, `M_LocationIndexText`, `M_HintText`, `M_ConfirmText` (TextBlock)

**WBP_SurvivalHud** (parent `UW_SurvivalHud`)

- Required: `M_TimerText` (TextBlock), `M_DefeatPanel` (any widget, starts Collapsed), `M_RetryButton`,
  `M_MainMenuButton` (Button)
- Optional:
  - Border: `M_StatusPlate`, `M_BannerPlate`, `M_ResultPanel`
  - Any widget: `M_StatusPanel`, `M_BannerPanel`
  - TextBlock: `M_PhaseText`, `M_WaveText`, `M_BannerTitleText`, `M_BannerText`, `M_DefeatTitleText`,
    `M_TimeSurvivedLabel`, `M_TimeSurvivedText`, `M_WavesLabel`, `M_WavesText`, `M_BestTimeLabel`, `M_BestTimeText`,
    `M_NewRecordText`, `M_RetryText`, `M_MainMenuText`

## Blueprints and verification state

The three WBPs already exist in `/Game/RTS_Survival/SurvivalMode/UI/`, saved and confirmed on disk. They were built
from the specs with a plain `UserWidget` parent so the layout could be checked before the C++ compiles.

`analyze_widget_layout` results at 1920x1080 and 1280x720 (raw units):

| Widget | Result |
|---|---|
| WBP_SurvivalArmySelection | 0 errors. Two warnings (`M_TabRow` and `M_SlotGrid` have zero area) because those containers are filled at runtime. |
| WBP_SurvivalStartLocationPicker | 0 errors, 0 warnings. |
| WBP_SurvivalHud | 0 errors. With M_DefeatPanel set to Visible for the check: 0 warnings. With it collapsed, the warnings are only the zero-size collapsed contents. |

After the build, run once in the editor:

1. `execute_python(<Docs/ui_specs/post_build_setup.py>)`. It creates and fills `DA_SurvivalUiStyle`, reparents the
   three WBPs to their C++ classes, sets `m_style_asset`, compiles, saves and checks that the files are on disk.
2. **Slot acceptance**: set `m_design_preview_slot_count` on the WBP_SurvivalArmySelection CDO to 8, then 16, then
   40, and run `analyze_widget_layout(['1920x1080','1280x720'])` each time. The expected result is no clipped,
   overlapping or offscreen children of `M_SlotGrid`. Reset to 12 afterwards.
   - **Unverified**: whether the analyzer creates the widget in design mode (`IsDesignTime()`), which the C++
     preview needs. If the grid comes back empty, check the sizing by PIE capture instead (the grid uses the same
     function at runtime). The sizing table above comes from the same algorithm, run offline.
3. Point the director's `ArmySelectionWidgetClass`, `StartLocationPickerWidgetClass` and `HudWidgetClass` at the
   three WBPs. This is in the director Blueprint, so the integrator or the level designer does it.

## Style asset (DA_SurvivalUiStyle)

| Field | Source |
|---|---|
| HeaderPlate | variant menu `HeaderPlate` (MI9_Custom_VariantHeader_560x80) |
| RosterPanel | variant menu `VariantPanel` (MI9_Custom_VariantPanel_480x848) |
| RosterEntry, RosterIconWell | variant menu `UnitEntry` (352x80), `UnitIconWell` (72) |
| Badge | variant menu `SelectedMarker` (28x28) |
| ConfirmButton, BackButton | variant menu Confirm 280x48 / Back 240x48 |
| HeadingFont, BodyFont, TextColor, MutedTextColor, AccentColor | variant menu |
| WarningColor, AcceptColor | variant menu `RemovedColor`, `BetterColor` |
| Tab, TabSelected, TabTextColor | unit browser (MI9_Custom_FactionTab 104x32) |
| SlotEmpty, SlotFilled | unit browser `Tile`, `TileSelected` (MI9_Custom_FactionUnitTile 120x164) |
| SlotLocked | `TileSelected.Pressed` |
| SlotIconWell, SlotWear | unit browser `IconWell` (104), `TileWear` (MI_FactionUnitTileWear, dynamic per slot, Seed parameter) |
| ArmyPanel | MI9_Custom_ThinPanel_1078x679_u_n |
| DialogPanel | MI9_Custom_DialogPanel_578x242_u_n |
| HudPlate | MI9_Custom_ResourceCounter_510x130_u_n |
| BannerPlate | MI9_Theme01_T_Panel_Tip03_511x150_f_n |
| ResultPanel | MI9_Theme01_T_Panel06_Dark_750x450_f_n |
| ArrowButton | MI9_Theme0x_T_Btn_Theme*Next 62x69 (n/h/p/d) |
| SupplyBar | MI9_Theme02_T_LinearProgress03(_Bg)_987x42 |
| ScrollBar | MI9_Custom_SettingsScrollThumbV / TrackV 8x32 |

Art that is drawn at one size and displayed at another (the slice instances are fixed-size `IMAGE` brushes):

- Roster entries are 440 wide against 352-wide art.
- Tabs are 84 wide against 104-wide art.
- The army panel fills the remaining width.

If the stretched bevels look off, the UI skin generator should make dedicated slices (`Custom_SurvivalRosterEntry`
440x80, `Custom_SurvivalTab` 84x32, `Custom_SurvivalArmyPanel` 1096x828).

## Supply costs

The full table is `Docs/supply_costs.json`: 86 units of GerBreakthroughDoctrine and GerStrikeDivision from the live
`DA_TrainingLayout`, plus `Nomadic_GerHq` at 0. 11 variants on 8 bases have their own cost; every other variant costs
the base value.

**Formula**:

- Weighted resources: P = (Radixite + 2 x Metal + 3 x VehicleParts) / 100. The weights follow the GameBalance
  rarity note: metal is twice and vehicle parts three times as rare as radixite.
- Supply: raw = (Base + K x P) x (clamp(TrainingTime, 12, 45) / 20)^0.15, with Base / K per category:

  | Category | Base | K |
  |---|---|---|
  | Infantry | 2 | 0.9 |
  | Team weapons | 2 | 0.65 |
  | Vehicles | 6 | 1.0 |
  | Aircraft | 6 | 1.0 |

  Economy units (harvester, building trucks) use 2 + 0.6 x P with no time factor.
- Rounding: integers up to 30, multiples of 5 above.
- Overrides: HQ = 0, harvester at least 5.
- Variants: clearly stronger or weaker variants get x0.8 to x1.2 of base.

Examples:

| Unit | Supply |
|---|---|
| JagerTrupp, Scavengers, LMG squad | 4 |
| Panzer Grenadiers | 12 |
| Iron Storm | 15 |
| Sd.Kfz. 221, Panzerjager I | 7 |
| Puma | 10 |
| Pz IV G | 16 |
| Pz IV H | 21 (Standard Plus 23, Ostwind 18) |
| Panther D | 29 |
| Tiger H1 | 30 (ASPS 24) |
| King Tiger | 40 |
| JagdTiger, E-75 | 50 |
| Maus, E100 | 60 |
| Bf 109 | 20 |
| Me 410 | 24 |

Fallback per unit type: Squad 5, Tank 15, Harvester 5, Nomadic 10, Aircraft 20.

**Suggested defaults: SupplyBudget 100, SlotCount 12** (the HQ takes slot 0). That allows, for example:

- HQ + Pz IV H + Pz IV G + Panzer Grenadiers + 2 JagerTrupp + Scavengers + 2 Sd.Kfz. 250 + Pak 38 + harvester:
  89 supply in 11 slots.
- HQ + Tiger + Pz IV H + infantry: 89 supply in 9 slots.

The slot cap stops a swarm of 4-supply squads; the budget limits armour-heavy openers. For maps with many slots,
scale the budget about linearly: roughly 8 supply per slot, so 16 slots ≈ 130 and 40 slots ≈ 300. Otherwise most of
the slots can never be filled.

**Import note**: `supply_costs.json` is an object, not a bare array.

- Keys: `formula`, `suggested_defaults`, `fallback_per_unit_type`, `units[]`.
- Each unit has `unit_type` (EAllUnitType name), `subtype`, `subtype_value`, `display`, `factions`, `resources`,
  `training_time`, `base_supply` and `variants` (`{"VariantA": n}`, keys are `EUnitVariant` names).
- Map each unit to `FSurvivalUnitSupplyCost{Unit=(UnitType, SubtypeValue), BaseSupply, VariantSupply}`, and
  `fallback_per_unit_type` to `FallbackSupplyPerUnitType`.
- The harvester is listed as `UNType_Harvester` / 18. The training layout slot is `UNType_Tank` / 18, so add the
  entry for both unit types or match on subtype.

Data problems found while pricing:

- `GerItalianFaction` has no entry in `DA_TrainingLayout` (and none in the faction-menu fallback), so it has no
  units.
- Metal Vault's cost is registered as two Metal entries in `CPPGameState.cpp`, so only 125 Metal survives.
- Pz IV F1's training time is 4 s (`MediumTankTrainingTime - 20`), which looks like a slip.
- The Me 410's DT_TrainingOptions title says "JU87".

These are outside this area; they are listed for the integrator.

## Integration requests

1. **HUD getter on the director** (`Director/SurvivalModeDirector.h`, public), so the wave director can reach the
   banner:
   ```cpp
   /** @return The survival HUD once deployment started; null before. */
   UW_SurvivalHud* GetHudWidget() const { return M_HudWidget; }
   ```
2. **Wave director calls the banner** (waves agent, `Waves/SurvivalWaveDirectorComponent.cpp`), when it launches a
   wave:
   ```cpp
   if (UW_SurvivalHud* Hud = M_Director.IsValid() ? M_Director->GetHudWidget() : nullptr)
   {
       Hud->ShowIncomingWaveBanner(M_WavesLaunched, EdgeLabel);
   }
   ```
   `EdgeLabel` is either a new `ASurvivalEdgeSpawnZone::GetDisplayName()` (`UPROPERTY(EditAnywhere) FText
   DisplayName`, e.g. "river crossing"; the banner formats it as "from the {0}") or, when that is empty,
   `UW_SurvivalHud::GetCompassDirectionText(Zone->GetActorLocation(), Director->GetCurrentPlayerBaseLocation())`.
3. **Setup input mode** (`ASurvivalModeDirector::SetSetupInputMode`): drag and drop needs the mouse cursor shown, and
   mouse events must reach the widget (`FInputModeGameAndUI` or `FInputModeUIOnly`, `bShowMouseCursor = true`, focus
   on the widget). Both setup screens are focusable and handle keys (Enter, Delete, Left/Right). The army screen's
   shade is hit-testable, so it blocks world clicks behind it. The picker root is not, so the player can still pan
   the camera during the preview.
4. **Defeat call order**: `ShowDefeat` works whether `BestSeconds` is the best before or after saving; no change
   needed, just keep `bIsNewBest` correct.
5. **Optional, world click on a start location**: when the director or controller detects a click on an
   `ASurvivalStartLocation` during `StartLocationSelection`, call
   `Cast<UW_SurvivalStartLocationPicker>(M_ActiveSetupWidget)->SelectLocationActor(Location)`.
6. **Library display names**: `USurvivalArmyLibrary::BuildArmyRoster` should fill `FSurvivalArmyEntry::DisplayName`
   from the DT_TrainingOptions title, or the variant's `TrainingTitle` override when set. The UI falls back to the
   table title and then the subtype name if it is empty.
7. **Settings asset**: import `supply_costs.json` into `Army.UnitSupplyCosts` / `FallbackSupplyPerUnitType`; keep
   `SlotCount = 12`, `SupplyBudget = 100`.
8. **Director widget classes**: after `post_build_setup.py`, set `ArmySelectionWidgetClass`,
   `StartLocationPickerWidgetClass` and `HudWidgetClass` on the director Blueprint to the three WBPs.

## Open questions

1. **MaxCopies**: the supply table sets none. Should super-heavies (Maus, E100, JagdTiger, E-75) be limited to 1 per
   army? The UI already enforces and shows any MaxCopies.
2. **Italian faction**: it has no training layout, so army selection would offer only the HQ. Should it be excluded
   from survival, or fall back to another faction's units?
3. **Budget for large slot counts**: does the designer want the budget to scale with `SlotCount` automatically
   (about 8 per slot), or is it set by hand per map?
4. **Stat line**: only tanks (health, speed) and squads (health per soldier) have one. Should aircraft and trucks get
   one too?
