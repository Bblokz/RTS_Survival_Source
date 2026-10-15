# Shipping-ready UI pass (skin 13 Frontstahl)

What was changed to make the skinned game UI crisp at every resolution, where each piece lives, and how to
undo it. Generator and editor scripts live in `D:/UE5Projects/RTS_Survival/UISkins/_generator/` (outside the
Source repo); state files live in `Saved/UISkinSwitcher/`.

## 1. No stretched panels (9-slice material)

- `slice_gen.py` renders every skinned panel/button/slot as a slice set: base colour `T9_*`, normal map `T9N_*`
  and masks `T9M_*` (R polish, G lamps, B fill), plus `slices.json` with slice margins, insets and art scale.
  Output: `UISkins/13_Frontstahl_Gunmetal/Slice/`.
- `ui_skin_material.py` builds `M_UISkinSlice` (UI domain, translucent, unlit). Corners draw 1:1, middles
  tile with a phase blend near the far margin so partial tiles never seam.
- Material output rules, which were the cause of the earlier see-through, additive-looking panels:
  - Output linear colour with straight alpha.
  - Do not multiply by vertex colour: Slate already multiplies UI-material output by it (tint, widget colour,
    opacity), and the sRGB backbuffer does the encoding.
  - Set every channel on the ComponentMask that feeds Opacity explicitly. A new mask keeps R enabled, so
    Opacity read the dark red channel.
- `ui_skin_slices.py` converts 462 brushes (Border, Image, Button states, ProgressBar, style assets) to
  `MI9_*` instances. The originals are kept in `state.json` (`slice_originals`), and the skin switcher's
  reset restores them.

## 2. Texture pixels match screen pixels

Art is authored at 2 texels per UI unit (`ArtScale` 2) with mips, so 1080p samples a mip, 2160p is 1:1 and
1440p sits between them. The DPI curve is linear on the shortest side: 1080 → 1.0, 1440 → 1.33, 2160 → 2.0.

## 3. Whole pixels, no idle render transforms (`ui_pixel_snap.py`)

| Widget | Before | After |
|---|---|---|
| `BP_W_ActionUI_Item` | button render scale 1.25, icon 1.5 | ScaleBox (layout scale 1.25) in the 76 px cell, slot padding −10 (96 px), icon filled to a square 48 units; the button's (12, 1.5) content padding moved onto the hotkey slot so the icon is not squashed |
| `BP_W_KeyBindings` (15 cells) | button render scale 1.25 | same ScaleBox cell as the action panel |
| `W_BP_Behaviour` | button 1.25, icon 1.5 | slot padding −8 (80 px), style padding 0, icon fill with 10 px padding (60 px) |
| `BP_W_WeaponItem` | overlay render scale 1.1 | ScaleBox 1.1, same centred footprint |
| `BP_W_Requirement` checkboxes | render scale 2.0 | 32 px glyph brushes, style padding doubled, negative slot padding keeps the row |
| `BP_W_EscapeMenuSettings` | −1.5 px translation | identity |
| 56 screen-space canvas slots | offsets such as −880.448 | whole units (auto-size slots: position only) |

A ScaleBox scales layout, not the finished image, so hotkey text is rasterised at its final size. Health bar
shears, the energy bar rotation, the minimap's start rotation and the pooled timed bars' distance scale are
intentional and stay. All widget animations end at identity.

Not converted: `BP_W_ActionUI_Description` / `W_BP_GlobalAbiiltyDescription` icons (1.5; the Blueprint
resets the brush size at runtime) and `W_MissionOverlay` text (0.8 / 0.7 render scale).

## 4. Light on the metal (material)

The normal map comes from the rim profile plus a luminance relief. The material adds:

- tight specular highlights where `mask.r` marks polished metal (`SpecPower` 48);
- a light sweep on hover, a glint on press, and flickering lamps (`mask.g`, accent colour);
- non-repeating grime (`Grime`) over the tiled fill.

The button states choose these effects through `Fx` (normal, hovered, pressed, disabled).

## 5. Crisp edges

- The generator applies a mild unsharp mask (0.35) after rendering.
- The material draws a 1 px silhouette line found with screen derivatives, so it stays exactly one pixel wide
  at every DPI. The line is lit on the light-facing side and dark on the far side; scalar `EdgeLine` is 0.35.

## 6–8. Noise, bleed, compression

- Noise is scaled so micro-detail stays at least 2 screen px.
- Colour is bled into transparent texels.
- Small UI uses UserInterface2D without mips. Slice sets and `T_ArchivePanel` use mips (BC7 for the archive
  panel).

## 9. Text

- All font sizes are whole numbers.
- Rich-text rows without an outline (29 rows in `DT_RichText`, `DT_Resources_RichText` and
  `DT_DamageTypeText`) get a 1 px dark drop shadow. The original table JSON is in
  `Saved/UISkinSwitcher/text_tables_original/`.
- 11 plain TextBlocks on metal get the same shadow (training entries, tabs and counts, popup, HUD error
  message, hold-to-skip).

## 11. Measuring

`UUISnapshotSubsystem` (`Source/RTS_Survival/GameUI/Tests/`) captures the HUD offscreen. It starts the game,
selects the player's units and then a trainer, and at 1080p, 1440p and 2160p writes
`Saved/UISnapshot/<selection>_<height>p.png` plus a widget reflector snapshot (`Engine/Saved/*.widgetsnapshot`).
It runs without a visible window, next to an open editor:

```
UnrealEditor-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/Test_TrainingUI -game -unattended -nosound -RenderOffscreen -windowed -ResX=1920 -ResY=1080 -UISnapshot
```

It needs the Development editor target built; from Git Bash, prefix the command with `MSYS_NO_PATHCONV=1`.
`snapshot_report.py` joins a snapshot with the brush list to report scaled or fractional widgets.

Result on 2026-10-14 (Test_TrainingUI, all player units selected, 1080p, 1440p and 2160p): 1240 HUD widgets. Only
two carry a render transform, both intentional rotations: `W_BP_GlobalAbilityPanel` (180°) and
`BP_W_PlayerEnergyBar` (−90°).

About a third of the widgets still sit at fractional pixel positions. Most come from Fill-sized boxes (the action
grid cells), world-space health bars and centred text. Slate snaps image and box vertices to pixels when it draws
them, so this doesn't blur anything; putting the action grid on whole pixels would need fixed instead of Fill
sizing in `MainGameUI_HUD`.

## Undo

- Pixel snapping and text shadows: run `ui_pixel_snap.restore()` in the editor (state in
  `Saved/UISkinSwitcher/pixel_snap_state.json`).
- Slice materials and skin textures: run `ui_skin_switcher.reset()`.
- Backups of the first six converted widget blueprints are in `Saved/UISkinSwitcher/backup_pixelsnap/`.

## Minimap panel (`ui_hud_parts.py`, `ui_minimap_material.py`, `custom_parts.py`)

- **Resource counters:** `BP_W_PlayerResource.Border_53` uses a custom counter plate (`Custom_ResourceCounter`
  slice set, drawn by `custom_parts.py`): a recessed dark field with a thin chrome rim and no bolts. The old tip
  panel drew its visible plate on an inset fraction of the widget, so the text ran over its rim. The text now has
  6 × 3 units of padding.
- **Energy bar:** `M_UIEnergyGauge` (Part 0 track, 1 fill, 2 demand marker) is procedural and measured in screen
  pixels:
  - recessed track with cell dividers;
  - segmented fill with a hot core and a slow upward shimmer;
  - chevron demand marker with a drop shadow.

  The fill colour still comes from C++ (`SetFillColorAndOpacity`); the Blueprint defaults are now electric cyan
  (OK) and orange-red (deficit).
- **Minimap:** `M_UI_MiniMap` was rebuilt and now samples the `Terrain` capture that
  `UMinimapTerrainCaptureComponent` hands over (the old material had no Terrain input). It shows:
  - graded and sharpened terrain;
  - fog of war: visible at full colour, explored at half strength, unexplored faint and hatched;
  - a pixel grid, a vignette and an anti-aliased camera trapezoid.

  The brush's old cyan tint is gone.
- **Terrain capture:** it now repeats at 1, 4 and 10 real seconds through a core ticker, which works while the
  game is paused. In the editor it waits for the shader compiler to go idle. It hides skeletal meshes, particles
  and fog. The first-frame capture alone had untextured placeholder materials.
- **Testing:** run the offscreen capture with `/Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_GameStartTutorial`
  (it picks Normal difficulty by itself). Add `-UISnapshotEnergyPreview` to hold the energy bar at 62%. It writes
  `Saved/UISnapshot/minimap_terrain` (EXR).
- **Undo:** run `ui_hud_parts.restore()`. The original `M_UI_MiniMap`, `W_BP_MiniMap`, `BP_W_PlayerResource` and
  `BP_W_PlayerEnergyBar` uassets are in `Saved/UISkinSwitcher/backup_hudparts/`.

## Small panels, unit description, glass buttons

- **Plates:** `custom_parts.py` draws two plates with no bolts:
  - `Custom_ResourceCounter`, a label plate;
  - `Custom_ThinPanel`, a content panel with a 1-unit chrome rim.

  `ui_hud_parts.PLATE_BORDERS` puts them on:
  - the cost tooltip (`BP_W_CostDisplay`);
  - the hover info box (`BP_W_HoverActor`);
  - the expansion holder's titles and its "Place at Socket" label;
  - every title and the resistance group in `BP_W_UnitDescription_Horizontal`.

  Paddings live in `SLOT_PADDINGS` / `BORDER_PADDINGS`. A UBorder's own Padding overwrites its content slot on
  compile, so border padding is set on the border itself.
- **Unit description:** right-aligned in `MainGameUI_HUD` (8-unit margin), so its auto-size grows towards the
  screen.
- **Portrait hover:** hovering the selected unit's portrait now hides an open training UI and restores it on
  unhover. `UActionUIManager::OnHoverSelectedUnitInfo` forwards only hover changes, because the unit info polls
  every tick.
- **Glass buttons:** `glass_buttons.py` makes `T_GlassButton_<Harvester|NomadTruck>_<n|h|p>` (64 units, 2×),
  imported to `Skins/_Shared/GlassButtons`. Red is for idle harvesters, green for idle nomadic trucks. No
  idle-unit button exists in the HUD yet.
- **Capture switches:**
  - `-UISnapshotHoverPortrait` hovers the portrait;
  - `-UISnapshotGallery=/Game/A+/Game/B` places widgets with their designer content.

## Idle unit buttons and barracks tabs

- **Idle buttons:** `UW_IdleUnitButton` (`Source/RTS_Survival/GameUI/IdleUnits/`) is a self-building glass button
  (texture states n/h/p with a count badge) that is disabled while nothing is idle.
  - It counts player-owned harvesters (`UHarvester` owners) or nomadic trucks in truck mode whose active command
    is `IdIdle`, refreshing every 0.5 s.
  - A click selects the next idle unit and focuses the camera on it.
- **Placement:** `ui_hud_parts.apply_idle_buttons` stacks the two buttons (40 units each) left of the resources
  in `MainGameUI_HUD` (`M_IdleButtonsColumn`). The resource row box (`SizeBox_0`) now ends inside the panel rim:
  it used to stick out 70 units past the screen edge. The resource icons went from 80 to 70 units.
- **Glyphs:** `GlassButtons/Glyph_<Harvester|NomadTruck>.png` are white silhouettes cut from the unit side views.
  `glass_buttons.py` puts them under the glass.
- **Barracks tabs:** `W_BP_TrainingSubSectionTab` is 44 units tall (was 38) with 5 units of text bottom padding,
  so two-line titles clear the rim. The training panel slot in the HUD is 6 units taller at the bottom.

## Minimap glass

- **Where to tweak:** open `W_BP_MiniMap`, go to Class Defaults, then MiniMap | Glass | Glass Settings
  (`FMiniMapGlassSettings`). Changes preview live in the designer (`NativePreConstruct`).
- **Runtime:** `SetGlassSettings` changes the glass while the game runs.
- **Two layers:**
  - `M_UI_MiniMap` bends terrain, fog and grid under the glass band (`GlassWidth`, `Refraction`).
  - `M_UI_MiniMapGlass` (`ui_minimap_glass.py`) is drawn by `UW_MiniMap::DrawGlassOverlay` after the icons, text
    and camera frame. It holds the Fresnel edge tint, lit rim and inner lip, the dark lip at the frame, a diagonal
    reflection streak, frost and tapered scratches.
- **Rotation:** light and streak are given in screen space; the widget's start-direction rotation is undone in the
  shader (`ScreenRotation`).
- **Settings:**
  - band: `bShowGlass`, `GlassWidth`, `CornerRadius`, `Refraction`, `FresnelPower`, `EdgeTint` (alpha = strength);
  - rim: `RimIntensity`, `RimWidthPx`, `InnerShadow`;
  - reflection: `StreakIntensity`, `StreakAngleDegrees`, `StreakWidth`, `StreakOffset`;
  - frost: `FrostIntensity`, `FrostScale`;
  - scratches: `ScratchIntensity`, `ScratchDensity`, `ScratchCoverage`, `ScratchLength`, `ScratchWidthPx`,
    `ScratchEdgeBias` (1 = glass band only), `ScratchSeed`.

## Glass resource icons

- **Material:** `M_UI_GlassResourceIcon` (`ui_glass_resource_icons.py`) puts a resource icon behind a round
  tinted glass dome in a gunmetal bezel. It has:
  - a lens that magnifies the centre and a tint over the icon;
  - a glow from below and a reflection crescent;
  - a rim highlight and a bottom bounce in the glass colour.

  Parameters: `BezelWidth`, `Refraction`, `IconScale`, `Desaturate`, `TintStrength`, `GlowIntensity`,
  `CapIntensity`, `RimIntensity`, `GlassColor`, `BezelColor`.
- **Instances:** `MI_GlassResourceIcon_<Radixite|Metal|VehicleParts>` take their `GlassColor` from the matching
  `DT_Resources_RichText` row (Radixite, Metal, Vehicle). Rebuilding them picks up text colour changes.
- **Applied by:** `UW_PlayerResource::ApplyGlassResourceIcon`, right after the blueprint's `SetupResource`. The
  per-resource map is "Glass Icon Materials" in `BP_W_PlayerResource`'s Class Defaults.
- **Unchanged:** the flat `MI_ResourceIcon_*` instances stay as they are for the small inline icons in rich text
  (`DT_ResourceImages_RichText`).
- **Layout:** resource icons are 64 units; the counter plate tucks under only the lowest 8 units of the dome.

## Glass panes on square buttons

- **Material:** `M_UI_GlassPane` (`ui_glass_panes.py`) is a nearly clear square pane with:
  - a hairline rim lit from the top left and a slight dark lip at the frame;
  - a faint diagonal reflection and a very light edge tint;
  - sparse scratches that fade towards the centre (`ScratchEdgeBias`), so icons stay fully readable.
- **Scratch variety:** scratches are laid out in screen space, so every button shows a different part of the
  pattern.
- **Where:** it is an extra hit-test-invisible Image (`M_GlassPane`) above the icon in:
  - `BP_W_ActionUI_Item` (under the hotkey);
  - `BP_W_TrainingItem` (under the count);
  - `W_BuildingItem` and `BP_W_OptionExpansionBuilding` (the button content became an overlay of
    [icon, pane]).
- **Hover:** the button's own hover animation (slice material sweep or outline) shows through the pane.
- **Instances:** each widget type has its own (`MI_GlassPane_ActionButton`, `_TrainingItem`, `_BxpItem`,
  `_BxpOption`), whose `InsetFrac` puts the pane on that button's inner face (SceneItem body at 0.11 plus bevel).
  Without it the overflowing action buttons' panes overlapped and showed light bars.
- **Undo:** run `ui_glass_panes.restore()` (state in `Saved/UISkinSwitcher/glass_panes_state.json`, backups in
  `Saved/UISkinSwitcher/backup_glasspanes/`).
- **Capture switches for the glass panes:**
  - `-UISnapshotHoverAction` hovers the first action button.
  - `-UISnapshotHoverTraining` hovers the first visible training item.
  - `-UISnapshotDelay=N` adds N real seconds after the start.
  - Gallery widgets now show their collapsed parts, so training items render outside the training panel.
- **Training panel in offscreen captures:** use `-UISnapshotFinishTruckAnims -UISnapshotDelay=35` on
  `Test_TrainingUI`.
  - Offscreen, the trucks' unfold montage never ends (the 3D scene is not drawn, so the meshes never count as
    rendered).
  - The switch calls `OnTruckMontageFinished`, which is what the blueprint calls when the montage ends.
  - The HQ then finishes converting, and selecting it opens the real training panel.
  - Add `-UISnapshotHoverTraining` to hover the first training item.
- **Other switches:** `-UISnapshotTickPoses` (force pose ticks) and `-UISnapshotFocusTrainer` (camera on the
  trainer). The selection log now prints the trainer's nomad status.

## Weapon items, reload bars, mission UI

- **Weapon glass:** `BP_W_WeaponItem` gets a pane from `MI_GlassPane_WeaponItem` (`ui_glass_panes.py`). It is
  stronger than on the action buttons, with no colour:
  - rim 0.34, streak 0.09, dust 0.08;
  - scratches 0.42 at coverage 0.62, with edge bias 0.4 so they also cross the weapon picture.
- **Reload bar:**
  - `UWeaponState::GetIsReloading` / `GetReloadProgress` (from the reload timer) cover full magazine reloads
    only, not shot cooldowns.
  - `UW_WeaponItem` binds the optional `M_ReloadProgressBar` and, in `NativeTick`, shows it only while the
    loaded weapon reloads.
  - The bar (`M_ReloadBarBox`, 6 units) hangs under the card (bottom aligned, negative bottom padding) and uses
    the energy gauge track/fill instances, tinted amber.
  - `UWeaponState::Debug_StartReload` (non-shipping) and the capture switch `-UISnapshotReloadWeapons` start
    reloads for captures.
- **Mission UI:**
  - `W_BP_Mission.Border_84` (expanded dialog) and `W_BP_MissionDescription.Border_56` use the new
    `Custom_DialogPanel`: a thin chrome rim, deep field and cyan corner brackets.
  - The collapsed title plate (`Image_56` in `SizeBox_279`) is the label plate stretched behind the title row
    (12 / 6 / 14 / 6 padding, 8 between button and text), instead of a fixed 200 × 50 image.
  - The `ProtoRichTextBlock`s (typewriter effect) are untouched.
  - `-UISnapshotCollapseMission` presses the collapse button for captures.

## Full map view

- **Open button:**
  - `M_OpenFullMapButton` sits in `MainGameUI_HUD`, in the minimap's top right corner.
  - It is a `UW_GlassIconButton` (`GameUI/GlassButtons`) with the `OpenFullMapView` action, at 38 units, placed by
    `ui_hud_parts.py` `apply_full_map_button`.
  - The button builds its own widget tree. The action calls `UMainGameUI::OpenFullMapView` on its outer main UI,
    so it needs no wiring.
- **View:** `UW_FullMapView` (`GameUI/MiniMap/FullMapView`) is created on first open and added to the viewport at
  Z 400, below the archive and the tech tree. It builds its own tree:
  - a dark backdrop that swallows clicks and the mouse wheel;
  - a square frame sized to 86% of the screen height;
  - a second `W_BP_MiniMap` inside the frame, with the minimap's Active/Passive/Terrain textures, fog of war
    manager, start-direction rotation and glass (thinner band, 2.5× scratch density);
  - the camera frame (`P1P2`/`P3P4`), copied from the minimap's material every tick.
- **Painted labels:** A–H / 1–8 labels and grid ticks in the frame band, the "TACTICAL MAP" title and a hint line.
- **Clicks:**
  - A click on the large map is re-broadcast through the minimap's `OnMiniMapClicked`, so the camera pawn moves
    as for a minimap click. The view stays open.
  - The red close button (`T_GlassButton_MapClose`) calls `UMainGameUI::CloseFullMapView`.
- **While open:**
  - The main HUD is hidden.
  - `ACPPController::SetIsPlayerInFullMapView` blocks the free camera and hover tooltips. The game does not pause.
  - A cinematic takeover, or hiding all game UI, closes the view.
- **Art:**
  - `glass_buttons.py` draws the procedural glyphs: a folded map with a route (`MapOpen`, cyan glass) and an X
    (`MapClose`, red glass).
  - `custom_parts.py` `map_frame()` draws `Custom_MapFrame`: 256 px, an 80 px slice margin (= the 40 unit band),
    a heavy chrome rim, a raised lip around the map, corner bolts and cyan brackets.
  - The frame's shading depends only on the distance to the nearest edge, so the tiled edge sections do not band.
  - The instance is `MI9_Custom_MapFrame_128x128_u_n`.
- **Captures:**
  - `-UISnapshotOpenFullMap` presses the real map button.
  - `-UISnapshotCloseFullMapAtStep=N` presses the close button when capture step N starts.

## Tactical map filters (discovered radixite, metal, scavenge objects)

- **Panel:** `UW_FullMapView` builds a "MAP FILTERS" panel left of the frame (300 units wide; the frame shrinks on
  narrow screens so it always fits). Each row is a `UW_MapObjectFilterToggle` (`GameUI/MiniMap/MapObjectFilters`):
  swatch, label and three buttons (Off, Tactical map only, Minimap + tactical).
- **Buttons:** `map_filter_buttons.py` draws `T_MapFilterButton_<Off|Tactical|Both>_<dim|lit>`, 56×40 units at
  2×: scuffed gunmetal bezel, glass well and glyph (slashed eye, folded map, map + round minimap). The selected
  option uses the `lit` face: a cyan backlight, a glowing glyph and an indicator lamp in the bottom bar. Hover and
  press are tints. Import them with `ui_hud_parts.import_map_filter_buttons()` into `Skins/_Shared/MapFilterButtons`.
- **Glass:** each button has a dynamic `M_UI_GlassPane` over its well (InsetFrac 0.075, 3 px corners, the weapon
  item scratch values, its own `ScratchSeed`).
- **Dots and discovery:** settings are in Project Settings > Game > Map Object Filters (colour, minimap and
  tactical map diameter, ring, default mode, refresh rate, threshold). `URTSMapObjectFilterSubsystem` reads the
  fog's explored render target back asynchronously (only while a filter is visible on an open map) and
  `UW_MiniMap::DrawMapObjectFilters` draws the dots under the unit icons.
- **Captures:** `-UISnapshotMapFilters` turns all three filters to "Minimap + tactical" and logs registered and
  discovered counts. `-UISnapshotMapFiltersRevealAll` also sets the console variable
  `RTS.MapObjectFilters.RevealAll 1` (non-shipping), which treats everything as discovered.

## Dead space on the minimap and tactical map (`ui_minimap_material.py`)

- **Where the dead space is:** where the terrain capture hit nothing (alpha 1), outside the playable area.
- **Boundary:**
  - The capture's coverage is averaged over a 12-sample ring (`BoundarySmoothPx` 3), so the stepped landscape edge
    becomes a clean contour.
  - A dashed cyan line follows it: `BoundaryColor`, `BoundaryLineWidthPx`, `BoundaryDashPx`, `BoundaryDashDuty`.
    The dashes run along the edge.
  - The terrain stops at the line and darkens slightly towards it (`BoundaryEdgeShade`).
- **Glow:** a second jittered ring (`BoundaryGlowPx`, `BoundaryGlowStrength`) adds a soft cyan glow just outside
  the line.
- **Unsurveyed ground:**
  - Diagonal hatching against the unexplored hatch direction (`HatchPeriodPx`, `HatchWidthPx`, `HatchColor`).
  - Faint contour lines in map space (`ContourLevels`, `ContourColor`).
- **Units:** all sizes are in screen pixels, so the minimap and the tactical map look alike.

## Behaviour hover description (`ui_hud_parts.py` `apply_behaviour_panel`)

- **Panel:**
  - `UW_BehaviourDescription` used to stretch a 766 × 1260 texture over the description.
  - `FBehaviourDescriptionStyle` now has `PanelMaterial` and `PanelBrushSize`. When a material is set, it is drawn
    tiled at that brush size; `PanelTexture` stays as the fallback.
- **Panels per type:** in `BP_W_BehaviourDescription`'s Class Defaults, Neutral and Buff use
  `MI9_Custom_BehaviourPanel_64x64_u_n` (cyan corner brackets) and Debuff uses
  `MI9_Custom_BehaviourPanelDebuff_64x64_u_n` (red brackets, warmer field). Both come from `custom_parts.py`.
- **Title:** sits on the label plate, centred, with 14 / 4 / 14 / 5 padding and 8 below.
- **Padding:** panel 16 / 10 / 16 / 12; the description and lifetime lines use 6 at the sides, instead of 15 / 10
  all round.
- **Wrapping:** the description wraps at 320 instead of 300. At 300, one word dropped to a third line at 1080p.
- **Captures:** gallery behaviour descriptions get sample data through `SetupDescription`, alternating buff and
  debuff: `-UISnapshotGallery=<BP_W_BehaviourDescription>+<BP_W_BehaviourDescription>`.

## Action button cooldown fill (`ui_glass_panes.py`)

- **Shader:** `M_UI_GlassPane` has a `CooldownRemaining` scalar (0 = ready, 1 = just used). While it is above 0:
  - the part still cooling down is shaded (`CooldownDim`);
  - a lit line marks the fill level (`CooldownEdgeIntensity`, `CooldownEdgeWidthPx`), with a faint wash below it
    (`CooldownTint`, alpha = wash strength);
  - the fill rises from the bottom to the top of the pane as the cooldown runs out.
- **Unchanged:** the cooldown layer sits under the rim, reflection and scratches. The button's hover shows through
  the shade. At 0 (the default) every pane looks exactly as before, so the training, building and weapon panes are
  unaffected.
- **Runtime:** `UW_ItemActionUI` binds the optional `M_GlassPane`, makes a dynamic instance on the first cooldown and
  sets `CooldownRemaining` in `NativeTick`. `UCommandData` counts cooldowns down in whole seconds, so the widget
  anchors an estimated end time (`FActionItemCooldownFillState`) and keeps the earliest estimate. The fill is
  smooth and never jumps backwards between updates.

## Crew repair progress bar

- **Widget:** `W_BP_CrewRepairProgress` (`UW_CrewRepairProgress`) shows the crew repair icon (`T_AB_Repair`) next to
  a green bar with the dig-in bar's textures. The bottom spacer sets how far above the health bar it sits.
- **Setup:** Project Settings > Game > Crew Repair Progress Bar (`UCrewRepairProgressSettings`), so no tank
  Blueprint needs changes.
- **Runtime:**
  - `ATankMaster` creates a screen-space widget component at the health bar's anchor on the first crew repair. It
    shows the component when the repair starts and hides it when the repair stops.
  - The tank syncs the remaining work (`GetRemainingCrewRepairSeconds`) every repair tick and after each module
    batch. The widget fills smoothly in between.
  - The full bar is the work at the start. New damage during the repair extends it.
  - Only player-owned tanks show the bar, so enemy repairs are not revealed through the fog of war.
