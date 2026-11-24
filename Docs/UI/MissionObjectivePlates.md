# Mission objective plates (design A1-c)

The mission panel in the top left shows each mission as a gunmetal plate with a bolted enamel shield:
- **Major objective:** a steel blue shield with a glass star.
- **Bonus objective:** a signal green shield with a glass diamond.

Picked from the sketch rounds on 2026-11-18 (canvas "Mission Overview Sketches", board A1-c).

## Marking a mission major or bonus

`FMissionWidgetState::ObjectiveType` (`EMissionObjectiveType`: Major / Bonus). On a mission in a mission manager it
sits under *Widget > Mission Widget State > Objective Type*.

`Major` is entry 0 and the struct default. Missions saved before the field existed therefore load as major; no mission
asset had to be re-saved. `RefreshMissionWidget` takes the type from the state it is given, so a mission that copies its
own `MissionWidgetState` before changing the title keeps its type.

## How it is built

`UW_Mission` builds the plate in C++ in `NativeOnInitialized`
(`Missions/MissionWidgets/ObjectivePlate/W_MissionObjectivePlate.cpp`). It then makes the plate the root of the
instance's widget tree.

**Moved from the Blueprint, not rebuilt:** `W_BP_Mission`'s own `M_RichTitle` and `M_RichDescription`. Its graph
keeps working on them: the typewriter (`DetermineTextAnimation`), the sounds and `UpdateText`. The rest of the
Blueprint layout (switchers, backplate, old buttons) stays off-screen. Its `OnChangeWidgetState` graph still runs and
does no harm.

**The plate's buttons route through C++** (`OnClickedObjectiveToggle` / `OnClickedObjectiveConfirm`). They set the state,
update the plate, then fire the Blueprint event. The Blueprint's own buttons called that event without telling C++,
which is why they are not reused.

The parts:

| Part | Asset |
|---|---|
| Plate | `MI9_RTS_T_Sidebar_TopPanel_128x128_u_n`: the HUD sidebar frame with hatched chrome edges and dome bolts, as a 9-slice brush with `ImageSize` = `BrushSize` (128) and `Tiling` Both |
| Shield, no icon | `M_UI_ObjectiveShield` → `MI_ObjectiveShield_Major` / `_Bonus`. Parameters: `EnamelTop`/`Bottom`, `BezelColor`, `BezelWidth`, `GlossIntensity`, hex nuts, drop shadow |
| Glass icon, its own image over the shield | `M_UI_ObjectiveGlassIcon` → `MI_ObjectiveGlassIcon_Star` (`IconShape` 0) / `_Diamond` (`IconShape` 1). Colours: `GlassColor`, `DeepColor`, `RimColor`. Glass: `FacetStrength`, `GlowIntensity`, `SpecIntensity`, `StreakIntensity`, `Opacity`, `OuterGlow`, `ShadowOpacity`, `IconScale` |
| Expand / collapse knob | `M_UI_ObjectiveKnob`. Dynamic instances per button state (`PointUp`, `Hover`, `Press`) |
| OK / Next | `MI9_Theme01_T_Btn06_*` (normal / hovered / pressed) |
| Labels and untagged titles | Engine Roboto Bold (`RR_mainFont` only holds the Faster One display face). Tagged titles keep their `DT_RichText` row |

All new assets are in `/Game/RTS_Survival/Blueprints/Missions/MissionUI/ObjectivePlates`. That folder is in
`DirectoriesToAlwaysCook`, because only C++ settings name the assets.

**Rebuild the materials** with `Source/Tools/MissionObjectivePlates/build_objective_plate_materials.py`, headless:
`UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<file>`. Rebuilding resets hand-tuned
instance values.

**Settings:** *Project Settings > Mission Objective Plates* (`UMissionObjectivePlateSettings`) holds the asset slots,
label colours, title sizes, expanded width (540), minimum title width and the bonus indent.

**Switching off:** `bUseObjectivePlates` = false brings back `W_BP_Mission`'s own layout. So does a missing material,
which also reports an error.

## States

| State | Plate |
|---|---|
| Minimized (title view) | Title only. Major: no label. Bonus: inline BONUS label, indented. Knob points down. |
| Expanded | MAJOR / BONUS OBJECTIVE label, title, groove, description. Knob points up. |
| ExpandedNext | Like expanded, but the knob is hidden and an OK / Next button completes the mission (as in `W_BP_Mission`). |

## Checking it

**Offscreen capture:** run `Test_TrainingUI -game -RenderOffscreen -UISnapshot -UISnapshotPreviewMissions
-UISnapshotToggleMissions`. It shows two major and two bonus preview missions. On the trainer captures it clicks every
knob once. `UISNAPSHOT mission widget` / `toggled mission` log lines give the type and state.
`-UISnapshotCollapseMission` collapses the first expanded plate.

**Known:** a `<Text_Armor>` tag in a description showed as raw text in the preview. The likely cause is that the
description's style set (`DT_AnimatedRichText`) has no `Text_Armor` row, which would mean it predates the plates;
this was not checked against the old layout. Descriptions use `<text>` tags.
