# Info takeovers (vehicle info widget)

A paused, full-screen panel that introduces a unit the first time a mission shows it. Built 2026-11-11 for vehicles;
squads added 2026-11-15 (same widget and style); the base is ready for buildings.

## Using it in a mission graph

| Node (on `UMissionBase`) | What it does |
| --- | --- |
| `Show Vehicle Info Takeover (Vehicle, Library Entry)` | Opens the prefab row `Library Entry` of `DT_VehicleInfoTakeovers` for the live `Vehicle`. The pin is a dropdown of the row names. |
| `Show Custom Vehicle Info Takeover (Vehicle, Info)` | Same widget, with the text written on the node (split the `Info` pin). |
| `Show Squad Info Takeover (Squad, Library Entry)` | Opens the prefab row `Library Entry` of `DT_SquadInfoTakeovers` for the live `Squad`. The pin is a dropdown of the row names. |
| `Show Custom Squad Info Takeover (Squad, Info)` | Same, with the text written on the node (split the `Info` pin). |

- The first parameter is the tank master in the world. Its weapons and armour are read from it when the node runs, so
  they always match the current balance. A queued takeover still shows them if the tank dies before it opens.
- The squad nodes take an actor: a squad controller (all its living soldiers are read) or a solo infantry unit such as
  `BP_Solo_Rus_Bazooka` (just itself). Anything else reports an error and returns false.
- The nodes return false when the player turned info takeovers off, or when the unit or row is missing.
- Calls never stack. The subsystem (`UInfoTakeoverSubsystem`) queues them and opens the next one when Understood is
  clicked. It waits while a cinematic, the tech tree, the archive or another locked takeover holds the screen.

## Camera controls takeover (field manual)

A three-page panel, paged like the tutorial overtakes (one footer button, a page switcher). It has no label over the
title (`HideKindLabel`); the title is the first line of each page:

- **Page 1/3, Camera Controls (CONTINUE):** three looping illustrations over Ger_IntroMission's real minimap.
  - Arrow keys: the pressed key lights and the camera's view frame moves over the minimap that way.
  - Edge panning: the cursor pushes against a screen edge, the edge lights, and the zoomed minimap slides, faster the
    longer the cursor stays there.
  - Zoom: the wheel rolls and the view frame on the minimap shrinks (zoom in), then grows back (zoom out).
- **Page 2/3, Camera Rotation (CONTINUE, added 2026-11-12):** one large illustration with two texts beside it, laid out
  like the marquee page. In code and input assets this is "panning" (`IA_PanCamera`, `ECameraControlsTakeoverPage::
  CameraPanning`, `WBP_CamCtl_Panning`); the player-facing text says "rotate" (user's word).
  - Rotate the camera: the mouse's wheel lights (middle button held) and the cursor moves right, so the zoomed minimap
    turns the other way, with a north dial turning along. Then the cursor moves up and the ground squashes vertically
    (tilt).
  - Reset the view: Alt lights with a middle click, and the map and dial turn back.
- **Page 3/3, Selecting Units (UNDERSTOOD):** three Panzer II F and a Hazmat Vulture squad (the game's unit icons) on a
  plain ground. A drag box in the HUD's colour grows from a left-button press over them; on release each one shows its
  real selection decal.

**Art rule (user):** no drawn art except the mouse and cursor (`T_CamCtl_Cursor`) and the marquee box. Everything else
is game art or plain UMG:
- **Minimap:** `T_Minimap_Ger_IntroMission`, exported from the game's minimap terrain capture with
  `-MinimapTerrainExport` (`UMinimapTerrainExportSubsystem`) and cropped by the brush's UV region. The minimap has no
  stored texture; it is a render target captured at begin play.
- **Unit icons:** `T_Ger_LightTank_PanzerII_F` and `T_Ger_Squad_VultureHazmats`.
- **Selection decals:** `M_UI_SelectionDecal_Box` and `_RoundThin` are UI-domain copies of the units' decal
  materials (`M_Selection_Box` for the Panzer II, `M_Selection_Round_Thin` for the Vulture), coloured by
  `MPC_Selection.Selected_Color`.
- **Keys, wheel, edge bars, view frames:** plain UMG borders and text glyphs.

The widgets are rebuilt by `Source/Tools/InfoTakeover/camera_controls_illustrations.py` (UECP script).

CONTINUE swaps the page and types the new title and description; the game stays paused and locked. Enter, Space and
Escape act like the button. As a game-mechanic field manual it has no "never show these info widgets again" box and
always shows, even when the player switched unit info off. Pages follow `ECameraControlsTakeoverPage`; `TryShowNextPage` steps to the next one
until the last.

What the rotation page teaches is taken from `RTS_BP_PlayerController` and `IMC_RTSDefault`:
- `IA_PanCamera` is the middle mouse button or P. While it is held, Mouse X adds to the camera pawn's yaw and Mouse Y
  to its pitch, both times `GetCameraPanSpeed` (the "camera pan speed" setting scales it). The pitch is clamped to
  ±`GetCameraPitchLimit` (18° in gameplay).
- `IA_PanReset` is the middle mouse button with a chord on `IA_Alt` (Left or Right Alt). It calls
  `UPlayerCameraController::PanReset`, which zeroes yaw and pitch.
- Page 1's "Edge panning" card is about scrolling the camera, not this. The game uses "pan" for both.

What the marquee page teaches is taken from the code:
- `IA_Marquee` is the left mouse button with a short hold trigger, which starts `ACPPController::CPPMarqueeSelectionStart`.
- `ACPPHUD` draws the translucent blue box and, on release, collects your own selectable units inside it.
- `SelectUnitsFromMarquee` replaces the selection. With Shift held (`bIsHoldingShift`) it adds the units instead, or
  removes them when every boxed unit was already selected.

**Mission node:** `Show Camera Controls Takeover` (async node, category Mission|Info Takeover, self is filled in).

- **On Closed (Was Shown):** fires once, after UNDERSTOOD on the last page, with the game unpaused. CONTINUE does not
  fire it. Play the announcer lines from this pin.
- Was Shown is false only when the widget could not be created. The "never show again" setting does not apply: it
  only hides unit intel (vehicles, squads), and this field manual still opens when it is off.
- If the world ends first, On Closed does not fire.
- It queues with the vehicle takeovers and waits for cinematics, like they do. All info takeovers also wait while the
  start-game screen (`UPlayerStartGameControl`) waits for the player, so the node can be called on the mission's
  first frame.
- It is archived under Game Mechanics ("Camera Controls") when it closes.

The bindings it teaches come from the real input: `MoveForward`/`MoveRight` axis mappings on the arrow keys
(DefaultInput.ini, `RTS_BP_PlayerController` → `ForwardRightMovement`). W, A, S and D are action buttons in
`IMC_RTSDefault`. Edge scrolling is `UPlayerCameraController::EdgeScroll`: the outer 2.5% of the screen, and it speeds
up while held. Zoom is `ZoomIn`/`ZoomOut` on the mouse wheel.

**Assets:** `GameUI/InfoTakeover/WBP_CameraControlsTakeover` (`UW_CameraControlsTakeover`; a copy of
`WBP_VehicleInfoTakeover`, so it has the same `M_Style`). It is made by
`Source/Tools/InfoTakeover/create_camera_controls_takeover.py`.

The illustrations are plain UserWidgets in `GameUI/InfoTakeover/CameraControls/`:

| Widget | Animation |
| --- | --- |
| `WBP_CamCtl_KeyboardPan` | `PanLoop`, 4 s |
| `WBP_CamCtl_EdgePan` | `EdgeLoop`, 6 s |
| `WBP_CamCtl_Zoom` | `ZoomLoop`, 4 s |
| `WBP_CamCtl_Panning` (page 2) | `PanningLoop`, 8 s |
| `WBP_CamCtl_Marquee` (page 3) | `MarqueeLoop`, 5 s |

- Each page-1 illustration is 330 x 186; the panning and marquee ones are 520 x 292. Each is clipped to its size.
- In the panning illustration, the turn (`Angle`) is on `MinimapImage` and the tilt (`Scale.Y`) on its parent
  `TiltLayer`. A widget's render transform scales before it rotates, so a scale on the turning image would squash
  along the turned axis instead of the screen's vertical.
- The C++ class plays every UMG animation of an illustration on an endless loop; no graph is needed.
- UMG animations tick on real time from the widget's Slate tick, so they run while the takeover pauses the game. The
  validation run checks this.
- **Moving parts come from a script.** Translation and scale keys are authored by
  `Source/Tools/InfoTakeover/camera_controls_transform_tracks.py` (the `SPECS` table; run it in the editor's Python).
  Every run rebuilds those RenderTransform tracks, so a retime made in the designer is overwritten. Change `SPECS`
  instead. The opacity tracks (key glows, edge bars, chevrons, wheel arrows, decals) come from
  `camera_controls_illustrations.py`.
- **Pitfall:** a float track on a RenderTransform sub-property ("RenderTransform.Translation.X") writes the UPROPERTY
  but never reaches Slate, so nothing moves on screen. A UMG RenderTransform track (MovieScene2DTransformTrack) works.
  Do not name a widget `Cursor`: it collides with `UWidget::Cursor` and the Blueprint fails to compile.
- **Pitfall:** render opacity does not fade a rounded-box brush's outline, so an element that fades in or out must be
  fill-only (outline width 0, outline colour transparent). The key glows, edge bars and the drag box are fill-only for this
  reason.

The card texts are in `UW_CameraControlsTakeover::MakeCameraSections`, `MakePanningSections` and `MakeMarqueeSections`
(C++). The panning and marquee pages share `BuildSideIllustrationPage`. Pages use
`UW_InfoTakeover::TryShowNextPage`, `SetConfirmButtonLabel` and `RestartTyping`, so another takeover can page the same
way.

## Cover takeover (field manual)

One page titled "Cover", with no kind label (`HideKindLabel`). It has three cards, one per colour of the squad move
preview, each with a screenshot of the real preview, and UNDERSTOOD.

**Mission node:** `Show Cover Takeover`, with the pin **On Closed (Was Shown)**. It has the same contract as Show Camera
Controls Takeover:
- it queues with the other takeovers;
- it waits for the start screen, cinematics, the tech tree and the archive;
- it ignores "never show again" (field manuals always show) and has no checkbox;
- it fires once after close, with the game unpaused.

Both nodes are subclasses of `UAsyncShowInfoTakeoverBase` (`AsyncShowInfoTakeover.h`). A subclass only names which
takeover `UInfoTakeoverSubsystem` queues (`QueueWaitedTutorialTakeover`).

**What the colours mean** comes from `FSquadMovePreviewStances::GetStanceForPosition` and the stance meshes' materials
(`Blueprints/Squads/SquadUnits/Stances`, parent `M_CoverIndicator`):

| Colour | Material | Stances (ERTSCoverType) |
| --- | --- | --- |
| Green | `MI_StrongCover` | `ProneCover` (Prone), `TrenchCover` (TrenchStandUp) |
| Yellow | `MI_WeakCover` | `CrouchCover` (Crouch), `HighCover` (StandingLeft/Right: stands behind a wall, steps out to fire) |
| Red | `MI_NoCover` | `NoCover`: the planned position is not a cover point |

The preview is shown while squads are the whole selection, and the move order sends every soldier to the spot it
showed (`USquadMovePreviewComponent`, `ACPPController::MoveUnitsToLocation`). Cover stops shots that its geometry
blocks. The combat cover logic treats an enemy as blocked only within `M_CombatCoverProtectedHalfAngleDegrees` (60°)
of the cover's facing, and soldiers in a firefight reposition when flanked.

**Screenshots:** `GameUI/InfoTakeover/Cover/T_CoverPreview_Green`, `_Yellow` and `_Red` (330 x 186, UI textures).
- They come from a rendered run on `/Game/RTS_Survival/Maps/TestCover/TestCover` with `-SquadMovePreviewValidate
  -SquadMovePreviewCaptureStances`. That run saves `Saved/CoverFinderDebug/SquadPreviewStances_<n>.png`: crouch
  cover, standing cover, open ground, and prone cover when a free point is left.
- They were cropped and scaled; the crops are in `Saved/InfoTakeover/Cover/Previews/`.
- The Blueprint child `GameUI/InfoTakeover/Cover/WBP_CoverTakeover` is made by
  `Source/Tools/InfoTakeover/create_cover_takeover.py`.

## Spaced armour takeover (field manual)

One page titled "Spaced Armour", with no kind label (`HideKindLabel`), and UNDERSTOOD. Under the description:
- **Left card:** an in-game render of a Panzer II Luchs with a side plate flying off after a hit, and a one-line caption.
- **Right card:** the three states of a side's plates, with the cover takeover's green, yellow and red labels:
  INTACT (full bonus), DAMAGED (half the bonus; any hit can knock plates loose, even one that bounces off, and
  explosions nearby; rifle and machine gun fire cannot), STRIPPED (no bonus; repair to put the plates back).

**Mission node:** `Show Spaced Armour Takeover`, with the pin **On Closed (Was Shown)**. It is a
`UAsyncShowInfoTakeoverBase` subclass with the same contract as the camera controls and cover nodes. It is archived
under Game Mechanics ("Spaced Armour", key `InfoTakeover.SpacedArmour`), without a picture, like cover.

**What the text says** comes from the vehicle module rules (`VehicleModuleBalance.h`,
`Docs/Units/Vehicles/VehicleModuleSystemDesign.md`, `AddOnArmorPanels.md`):
- An add-on armour zone adds its armour on top of the structural side plates. Yellow gives half
  (`UVehicleModuleAddOnArmorBehaviour`), red none. The visible panels follow the zone, and repairs bring them back
  (a full service brings back all of them).
- Every plate rule has an AddOnArmor candidate with probability 1.00 and `EModuleNonPenPolicy::External`, so
  **non-penetrating hits can also damage the zone**. The shell rule scales that chance: 0.25 for AP/APCR, 0.33 for
  APHE, 0.70 for HE and HEAT (live values on 2026-11-11). That is why the text says "can", not "will".
- Splash damage hits the zone too. Ballistic hits under 20 mm calibre (`MinBallisticCalibreMmByModuleType`) never
  damage add-on armour.

**The render:** `GameUI/InfoTakeover/SpacedArmour/T_SpacedArmour_PlateShotOff`, 1280 x 720, shown at 640 x 360. It was
taken by the add-on armour validation with its hero camera (2026-11-11):

```
ENGINE=.../UnrealEditor-Win64-DebugGame-Cmd.exe \
VEHICLE=/Game/.../PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs_SpacedArmor.Ger_Vehicle_PanzerII_Luchs_SpacedArmor_C \
INJECT=/Game/.../PanzerII_Luchs/Meshes/SM_Luchs_SpacedArmor_Hull_AddOnPanels.SM_Luchs_SpacedArmor_Hull_AddOnPanels \
MAP=/Game/RTS_Survival/Maps/UnitTests/UT_B4Howitzer TAG=Luchs_Hero_Sun \
Source/Tools/AddOnArmorBreak/run_panels_validate.sh -AddOnArmorPanelsHeroCamera -AddOnArmorPanelsFaceSun
```

- `-AddOnArmorPanelsHeroCamera` adds a 1920 x 1080 three-quarter front-left camera (`Hero_*.png`) and pictures at
  0.05 to 0.8 s after the first shell (`Hero_1_FirstHit_Flight_<ms>ms.png`) in `Saved/AddOnArmorBreak/<tag>/`.
- `-AddOnArmorPanelsFaceSun` turns the tank so its left side faces the map's directional light, which is otherwise
  behind it on UT_B4Howitzer.
- The picked frame is `Hero_1_FirstHit_Flight_100ms.png`: the plate is tearing away and the gap shows the road wheels.
  By 167 ms the plate has dropped out of frame. It was cropped (x 235, y 150, 1640 x 922) and scaled to 1280 x 720;
  the crop is in `Saved/InfoTakeover/SpacedArmour/Previews/`.
- The flying plate renders flat grey, not in the camo. The death burst's plates in the same run (`Hero_5_Death.png`)
  are dark slabs too, and so is the plate in the Panther II pictures, so it is how the debris draws, not a capture
  fault. The run logged no material usage warning. This is not investigated yet.
- The render was taken before the Luchs spaced-armour Blueprint was converted, so the command above injects its
  hull and the panels component. It has been converted since (2026-11-11, `AddOnArmorPanels.md`); drop `INJECT=` to
  re-render.

**Assets:** `GameUI/InfoTakeover/SpacedArmour/WBP_SpacedArmourTakeover` (`UW_SpacedArmourTakeover`, a copy of
`WBP_VehicleInfoTakeover`) and the texture, both made by `Source/Tools/InfoTakeover/create_spaced_armour_takeover.py`
(headless, DebugGame editor). The texts are in `UW_SpacedArmourTakeover` (C++).

## Nomadic conversion takeover (field manual)

Two pages, with no kind label (`HideKindLabel`): "Nomadic Trucks" ends in CONTINUE, "Build Radius" in UNDERSTOOD
(`ENomadicConversionTakeoverPage` Conversion / BuildRadius, a `UWidgetSwitcher`, page lamps and PREVIOUS from the base).
Each page is laid out like the camera controls takeover's side pages: one looping illustration (520 x 292) on the left
and the steps beside it (label, title, body). Built 2026-11-12 for `Mission_Intro_BaseBuilding`: it shows right after
the HQ drives out of the glider, before the "select ze nomadic HQ and convert it" line. The build radius page was split
out of the first page the same day at the user's request ("explain the build radius and that the HQ is exempt").

Page 1, "Nomadic Trucks" (`WBP_NomCnv_Deploy`):

| Label | Title | Body (short) |
| --- | --- | --- |
| BOTTOM PANEL / CTRL + Q | Convert into a building | Select the truck and click the building picture in the bottom panel, or press Ctrl+Q. |
| LEFT CLICK / COMMA, PERIOD | Place it | Left click a free spot; comma and period rotate; right click cancels. The truck drives there and unfolds. |
| PACK UP | Move it again | Select the building and click the truck picture in the same place. |

Page 2, "Build Radius" (`WBP_NomCnv_Radius`):

| Label | Title | Body (short) |
| --- | --- | --- |
| YOUR HQ | Exempt: deploys anywhere | The HQ truck needs no build radius; any open spot it fits on, also far from your buildings. |
| OTHER TRUCKS | Inside a build radius | Every other truck must deploy inside one; outside it the preview cannot be placed. |
| BUILD RADIUS | Where it comes from | Your deployed HQ and some other buildings (such as the Gamma Facility) give one; it shows while placing. |

**Mission node:** `Show Nomadic Conversion Takeover`, with the pin **On Closed (Was Shown)**. It is a
`UAsyncShowInfoTakeoverBase` subclass with the same contract as the cover and spaced armour nodes. It is archived under
Game Mechanics ("Nomadic Trucks", key `InfoTakeover.NomadicConversion`), without a picture.

**What the text says** comes from the code and assets (checked 2026-11-12):
- **The button:** the truck's convert button is `CreateBuildingButton` in the bottom panel (`W_BuildingUI_ItemPanel`,
  Blueprint `BP_W_BuildingUI_ItemPanel`), not a command card ability. Its picture and tooltip come from
  `DT_TruckButtons` by nomadic subtype (`SetupTruckBrushes`). For the German HQ: the building picture `SB_Ger_HQ`
  (`T_Ger_HQ`) with the tooltip "Convert into HQ building". Once deployed, `ConvertToVehicleButton` shows `SB_GerHqTruck`
  (`T_Ger_HQTruck`) with "Convert back into HQ Truck"; other trucks say "Convert back into truck".
- **The hotkey:** `IA_NomadicExpansion`, chorded Ctrl + Q in `RTS_IMC_ChordedActions` (rebindable), calls
  `ACPPController::OnNomadicExpansionShortCut` → `SimulateClickNomadicExpandButton`. On a deployed building that
  function calls the *cancel vehicle conversion* handler, not *convert to vehicle*, so Ctrl + Q does not pack a
  building up. The page only names Ctrl + Q for converting, and the illustration hides the badge on the pack-up button.
- **Placing:** `ACPPController::ConstructBuilding` starts the nomadic preview. A primary (left) click places it through
  `TryPlaceBuilding` → `NomadicConvertToBuilding`; a blocked preview gives "cannot build here". A secondary (right)
  click stops the preview (`SecondaryClick` → `StopBuildingPreviewMode`). `IA_RotateLeft` / `IA_RotateRight` (comma,
  period in `IMC_RTSDefault`) turn it by `RotationDegrees` (10) while `GetIsRotationAllowed` (true for the nomadic
  preview).
- **Unfolding:** the truck drives to the spot, turns to it and unfolds (`ENomadStatus` CreatingBuildingRotating →
  CreatingBuildingTruckAnim → CreatingBuildingMeshAnim → Building), with a conversion progress bar.
- **Build radius:** `bUseBuildRadius = NomadicVehicle != M_PlayerHQ`, so only the player HQ skips it. The radii are the
  `UBuildRadiusComp`s of deployed nomadics (`SetRadiusComponentActive` on finishing the deploy, off when packing up). On
  2026-11-12 `Ger_HQ_Truck`, `Ger_HQ_Truck_Breakthrough`, `Ger_GammaFacility_Truck` and `Ger_Forge_Truck` have one. The
  text names only the HQ ("and some other buildings"), so it stays true when that list changes.
- **Packing up:** `ConvertToVehicleButton` → `ACPPController::ConvertBackToVehicle` → `ANomadicVehicle::ConvertToVehicle`.

**The first illustration:** `GameUI/InfoTakeover/NomadicConversion/WBP_NomCnv_Deploy`, animation `DeployLoop`, 10 s:
- 0.85 s: the cursor clicks the HQ truck (`T_Ger_HQTruck`) and its selection decal (`M_UI_SelectionDecal_Box`) lights.
  The bottom panel's button fills with `T_Ger_HQ` and the "CTRL Q" badge.
- 1.95 s: the cursor hovers the button (tooltip "Convert into HQ building") and clicks it.
- 2.1 to 3.6 s: a green footprint with a faint `T_Ger_HQ` follows the cursor. Two period presses turn the footprint
  10° each, and a left click at 5 s places it.
- 5.3 to 6.8 s: the truck drives there. From 6.9 s it shrinks away while `T_Ger_HQ` grows in, with a progress bar.
- From 7.95 s: the button shows `T_Ger_HQTruck` and the cursor hovers it ("Convert back into HQ Truck"). The loop then
  resets.

**The second illustration:** `GameUI/InfoTakeover/NomadicConversion/WBP_NomCnv_Radius`, animation `RadiusLoop`, 10 s.
Captions and click glows use the style's label red (option G):
- 0.8 s: the HQ truck is selected. Its green preview carries the build radius the HQ will give (a ring; the game's
  nomadic preview has the radius attached too), is dragged to open ground far from anything and placed at 2.85 s. The
  truck turns into `T_Ger_HQ`; the ring stays. Caption "YOUR HQ: ANYWHERE IT FITS".
- 5.35 s: a refinery truck (`T_Ger_RefineryTruck`) is selected. Its preview is red outside the ring, the click at
  6.6 s is refused ("Outside the build radius"); dragged inside, it turns green and is placed at 8.05 s
  (`T_Ger_Refinery`). Caption "OTHER TRUCKS: INSIDE A BUILD RADIUS".
- 9.3 to 10 s: everything resets; the ring leaves by scale (a rounded box's outline does not fade).
- `Source/Tools/InfoTakeover/nomadic_radius_illustration.py` (UECP `execute_uecp_script`, `allow_destructive`) builds
  the tree, the look, the opacity tracks and the RenderTransform tracks in one run.

The rules of the camera illustrations apply: no drawn art but the cursor (`T_CamCtl_Cursor`), fill-only shapes for
anything that fades, no widget named `Cursor`, and moving parts only through a RenderTransform track script.
- `Source/Tools/InfoTakeover/nomadic_conversion_illustration.py` (UECP `execute_uecp_script`, `allow_destructive`) builds
  the tree, the look and the opacity tracks.
- `Source/Tools/InfoTakeover/nomadic_conversion_transform_tracks.py` (editor Python) adds the translation, rotation and
  scale keys from its `SPECS`. Run it after every rebuild of the illustration.
- Neither needs the C++ class.

**Assets:** `GameUI/InfoTakeover/NomadicConversion/WBP_NomadicConversionTakeover` (`UW_NomadicConversionTakeover`, a copy
of `WBP_VehicleInfoTakeover`), made by `Source/Tools/InfoTakeover/create_nomadic_conversion_takeover.py` (needs the
class in the editor DLL). The texts are in `UW_NomadicConversionTakeover` (C++).

## Building expansion pack-up takeover (field manual)

One page titled "Packing Up Expansions", with no kind label (`HideKindLabel`), and UNDERSTOOD. Built 2026-11-12 at the
user's request ("show with an animation how building expansions are packed up with the nomadic when it moves and placed
again after re deployment"). It is laid out like the nomadic conversion takeover: a looping illustration (520 x 292)
on the left, four steps beside it. Under the illustration, a legend shows the three pictures an expansion slot takes
while it packs, is packed and goes back up.

| Label | Title | Body (short) |
| --- | --- | --- |
| PACK UP | Expansions pack up too | Every expansion folds away with the building, also one still being built; its slot keeps it, marked as packed. |
| ON THE ROAD | Carried with the truck | The slots are greyed out and packed expansions do nothing; cancelling the pack-up leaves them up. |
| AUTOMATIC | Attached ones come back by themselves | Expansions built onto the building (the HQ radar) rebuild in their old place once it has deployed, in their normal build time, at no cost. |
| SLOT / LEFT CLICK | Place the others again | Free-standing ones (flak guns) wait in their slot; click it (Place Packed Up Building) and place them near the building, also free; cancel and they stay packed. |

Slot legend (`MakeSlotLegend`): `T_IsPackingBXP_v002_0000` "Packing up with the building",
`T_PlacePackedUpBuilding_v001_0000` "Packed. Once deployed, click it to place the expansion again",
`T_CancelPackedUpBuilding_v001_0000` "Going back up. Click it to cancel; the expansion stays packed".

**Mission node:** `Show Building Expansion Pack Up Takeover`, with the pin **On Closed (Was Shown)**. It is a
`UAsyncShowInfoTakeoverBase` subclass (`UAsyncShowBxpPackUpTakeover`) with the same contract as the other field manual
nodes. It is archived under Game Mechanics ("Packing Up Expansions", key `InfoTakeover.BxpPackUp`), without a picture;
the archive text also says that a destroyed expansion is gone.

**What the text says** comes from the code (checked 2026-11-12):
- **Pack-up:** `ANomadicVehicle::ExecuteConvertToVehicleCommand` calls `StartPackUpAllExpansions` on every live
  expansion, also one under construction (`ABuildingExpansion::StartPackUpBuildingExpansion` keeps its progress for a
  cancel). When the truck is done, `FinishPackUpAllExpansions` → `FinishPackUpExpansion` removes the extra storage, the
  field defence discount and the ability cooldown bonus, sets `BXS_PackedUp` and destroys the actor. The slot keeps the
  type, construction rules and socket name (`ResetEntryAndUpdateUI(..., true)`; `EndPlay` skips packed ones).
- **Cancel:** `TerminateConvertToVehicleCommand` → `CancelPackUpExpansions` puts them back (built again, or construction
  resumed).
- **As a truck:** `UMainGameUI::OnTruckConverted` disables the slots (`W_BuildingItem::OnItemDisabled`: button
  disabled, tooltip "First convert the vehicle into a building"). The picture dims to `DisabledImageOpacity` (0.5) and
  `OnItemEnabled` restores `EnabledImageOpacity` (1.0); both were `SetOpacity(50/100)` on the 0 to 1 scale until the
  2026-11-12 fix. Not checked on screen.
- **Redeploy:** `OnFinishedConvertingToBuilding` → `OnConvertToBuilding_PlacePackedBxps` →
  `BatchAsyncLoadAttachedPackedBxps`: every packed `Socket` and `AtBuildingOrigin` expansion is spawned and placed at
  once (`ACPPController::OnBxpSpawnedAsync_InstantPlacement`), on its saved socket or at the building's origin, with
  status `BXS_BeingUnpacked`. It rebuilds over its normal `M_BuildingTime` (`StartExpansionConstructionAtLocation`).
- **Free expansions** stay `BXS_PackedUp`. Clicking the slot (`UMainGameUI::ClickedItemBuildingExpansion` →
  `PlacePackedUpBuildingExpansion` → `ACPPController::RebuildPackedExpansion`) starts the normal preview inside the
  building's expansion radius (`ExpansionRadius`). Clicking a packed attached slot re-places it instantly.
- **No cost:** `PlaceBxpIfAsyncLoaded` only calls `PayForBxp` for a new expansion. Cancelling a packed placement or an
  unpacking (`CancelBuildingExpansionConstruction(..., true)`) refunds nothing and returns the slot to packed.
- **Destroyed:** an expansion destroyed outright resets its slot to empty (`EndPlay` → `DestroyBuildingExpansion(this,
  false)`), so it is not kept.
- **UI names:** the slots sit in the bottom panel next to the convert button (`BP_W_BuildingUI_ItemPanel`,
  `W_BuildingItem`); they have no title. Status pictures and tooltips are in `W_BuildingItem::UpdateVisibleButtonState`:
  "Expansion is being packed up by the owner" (typo fixed 2026-11-12), "Place Packed Up Building", "Cancel placing packed expansion",
  "Cancel unpacking expansion". The option panel's columns are titled Tech, Economic and Defense.
- **Example choice:** the illustration uses the German HQ with the radar (`BTX_GerHQRadar`, Socket) and a 37 mm Flak
  (`BTX_37mmFlak`, Free). The refinery is not used: for every faction but Breakthrough,
  `AddStrikeDivisionBxpOptions` removes the refinery's 37 mm Flak, so it has no free expansion (Converter = Socket,
  Kaskade and Flak Stand = AtBuildingOrigin).

**The illustration:** `GameUI/InfoTakeover/BxpPackUp/WBP_BxpPack_Loop`, animation `PackLoop`, 12 s, with a caption line
in its top left per beat:
- 0 to 1.15 s: the HQ (`T_Ger_HQ`) with the radar (`T_GER_BXP_HQRadar`) on its corner and the flak
  (`T_BXP_37mmFlak_Render`) beside it. The bottom panel shows the pack-up button (`T_Ger_HQTruck`, tooltip "Convert back
  into HQ Truck") and both slots with their expansion pictures (DT_Items). Click at 1.15 s.
- 1.3 to 3.25 s: "PACK UP": both slots show `T_IsPackingBXP`, the radar and the flak shrink into the HQ, a progress bar
  fills and the HQ turns into the truck. From 3.25 s the slots show `T_PlacePackedUpBuilding`, greyed out, and the two
  expansions ride on the truck as small icons.
- 3.4 to 6.9 s: "MOVE AND DEPLOY AGAIN": the button shows `T_Ger_HQ` ("Convert into HQ building"), the green preview is
  dragged right and placed (5.1 s), the truck drives there and unfolds.
- 7.0 to 8.55 s: "ATTACHED: REBUILDS BY ITSELF": the radar grows back on the new HQ with a progress bar; its slot shows
  `T_CancelPackedUpBuilding`, then the radar. The flak slot stays packed.
- 8.6 to 11.05 s: "FREE-STANDING: PLACE IT FROM ITS SLOT": the cursor clicks the flak slot ("Place Packed Up Building"),
  a faint disc shows the expansion range, the flak preview is placed (9.85 s) and builds.
- 11.6 to 12 s: the new HQ fades into the first one for the next loop.

The camera illustrations' rules apply (cursor `T_CamCtl_Cursor` is the only drawn art, fill-only shapes for anything
that fades, no widget named `Cursor`, moving parts only through RenderTransform tracks):
- `Source/Tools/InfoTakeover/bxp_packup_illustration.py` (UECP `execute_uecp_script`, `allow_destructive`) builds the
  tree, the look and the opacity tracks. It was run in the shared editor, in the new folder.
- `Source/Tools/InfoTakeover/bxp_packup_transform_tracks.py` (editor Python) adds the translation and scale keys from its
  `SPECS`. Run it after every rebuild of the illustration.

**Assets:** `GameUI/InfoTakeover/BxpPackUp/WBP_BxpPackUpTakeover` (`UW_BxpPackUpTakeover`, a copy of
`WBP_VehicleInfoTakeover` taken 2026-11-12, so it carries the instrument panel `M_Style` of that day), made by
`Source/Tools/InfoTakeover/create_bxp_packup_takeover.py` (headless DebugGame-Cmd). The texts are in
`UW_BxpPackUpTakeover` (C++). `build_takeover_style.py` does not list it yet: add it there, or re-run the create script,
when the style changes.

## Vultures AT stick grenade takeover (field manual)

One page titled "AT Stick Grenades" under the kind label "VULTURES - SQUAD ABILITY", and UNDERSTOOD. Under the
description:
- **Left card:** a short looping in-game clip of a Vulture squad sticking its grenades onto an advancing T-26 and the
  tank blowing up, with a one-line caption.
- **Right card:** VOLLEY (all four throw, the grenades stick and go off a moment later, even on a driving tank),
  TARGETS (wrecks most light tanks, badly hurts medium ones; the blast is dealt against the thinnest plate,
  `DealDamageVsRearArmorInRadiusAsync`, so facing does not matter) and RELOAD (long cooldown; out of range the squad
  walks up first). The text is qualitative on purpose; the numbers are in `GrenadeQuality.md` and may change.

**Mission node:** `Show Vultures AT Grenade Takeover`, with the pin **On Closed (Was Shown)**. It is a
`UAsyncShowInfoTakeoverBase` subclass (`UAsyncShowVulturesAtTakeover`, kind `EInfoTakeoverKind::VulturesAt`) with the
same contract as the other field manual nodes. It is archived under Game Mechanics ("AT Stick Grenades", key
`InfoTakeover.VulturesAt`), without a picture.

**The clip** is a flipbook, not a video: `GameUI/InfoTakeover/VulturesAt/T_VulturesAt_ClipAtlas`, 80 frames of
480 x 270 in 8 x 10 cells (3840 x 2700, UI group, no mips, never streamed, BC1). `UW_ClipInfoTakeover` (the shared
base of the clip field manuals; `UW_VulturesAtTakeover` only adds the texts) moves the image brush's UV region to the next cell from `NativeTick` on real time (12 fps, the last frame held 1 s), so it plays
while the takeover pauses the game; no Media Framework and no material are involved. The cells are inset half a texel
so filtering never pulls in the next frame. The grid, frame count and rate are `M_Clip*` on the Blueprint's class
defaults.

**Re-recording the clip** (DebugGame, headless; the shared Development editor can stay open):

```
MAP=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall   Source/Tools/InfoTakeover/VulturesAt/capture_clip.sh -VulturesAtClipSettle=4 -VulturesAtClipAt=0,0
python Source/Tools/InfoTakeover/VulturesAt/make_clip_atlas.py --start 126 --step 2 --frames 80
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript   -script=Source/Tools/InfoTakeover/VulturesAt/create_vultures_at_takeover.py -unattended -nullrhi   -DisablePlugins=Fab,BpGeneratorUltimate
```

- `-VulturesAtClipCapture` (`UVulturesAtClipCaptureSubsystem`, `GameUI/InfoTakeover/Tests`) spawns a player
  Vulture squad and an enemy T-26 with its turret off, drives the tank in from the left to a stop in front of the
  squad, orders the AT stick grenade throw once it is within 790, and saves every 1/24 s tick from a three-quarter
  scene capture (1280 x 720) to `Saved/InfoTakeover/VulturesAt/Frames/` until 2.5 s after the tank died. Grep the
  log (`Saved/Logs/VulturesAtClip.log`) for `VULTURES_AT_CLIP`; it gives the frame of the throw order.
- The picked window (2026-11-15) starts at frame 126, 7 frames after the throw order (119), while the soldiers
  wind up, and keeps every second frame for 80 frames, to the burning wreck. `make_clip_atlas.py` also
  writes `clip_contact_sheet.png` and `clip_atlas.json`; the asset script reads the grid from the json.
- Ger_Trains was tried first (it looks better), but only at its default spot (13200,-33249, the unit-image anchor):
  there the T-26 hopped 50 units every few seconds, also after a 20 s warm-up. The cause was not found; other spots
  may drive fine (tanks have driven there in other benches). RTS_ToCopySmall at 0,0 drives cleanly; its sand ground
  gives the clip its orange tone.
- `create_vultures_at_takeover.py` imports the atlas and makes `WBP_VulturesAtTakeover` as a copy of
  `WBP_SpacedArmourTakeover` (already on the Rotsignal skin) reparented to `UW_VulturesAtTakeover`. It is also in
  `Style/build_takeover_style.py`'s list.

## Rattle-Feed takeover (field manual)

One page titled "Rattle-Feed" under the kind label "PANZER II - VEHICLE ABILITY", and UNDERSTOOD. Same layout as the
Vultures AT page (both are `UW_ClipInfoTakeover` children):
- **Left card:** a looping clip of a Panzer II F firing at a parked T-26. It shows a normal burst and the start of the 5 s
  reload, then a hard cut to the gun with Rattle-Feed on. Tags are stamped in the frames: NORMAL FIRE, RELOADING - 5 S,
  RATTLE-FEED.
- **Right card:** BURST (15 s; the gun barely pauses, twice the magazine, reload almost at once), WEAPONS (only the
  autocannon; machine guns as normal; same damage and penetration per hit) and COOLDOWN (well over a minute).

What Rattle-Feed is: the apply-behaviour ability `RattleFeedAbility` (`EBehaviourAbilityType::RapidFireAutoCannon`)
adds `BP_Beh_RapidFire_Autocannon`, a `UBehaviourWeapon`. Values read from the assets on 2026-11-15
(`Source/Tools/InfoTakeover/RattleFeed/dump_rattle_feed.py`):

| | Value |
| --- | --- |
| Duration | 15 s |
| `BaseCooldownMlt` | 0.33 (the pause between bursts, not the spacing of rounds inside a burst) |
| `ReloadSpeedMlt` | 0.1 |
| `MagSizeMlt` | 2 |
| `MinimalWeaponCalibreToApplyTo` | 20 mm |
| Ability cooldown | 100 s on Panzer II F, Luchs, Sd.Kfz. 231, Sd.Kfz. 140; 30 s on the Jaguar |

The Panzer II F's KwK 30 fires bursts of five rounds (`M_BurstCooldown` spacing) with a 0.8 s `BaseCooldown` between
bursts, a 10-round magazine and a 5 s reload. So Rattle-Feed does not make a burst faster: it removes the pauses. The
capture measured about 1 shot/s over a magazine and its reload, and 3.4 shots/s with Rattle-Feed. A reload that is
already running keeps its old length, which is why the capture switches it on right after a reload ends.

**Mission node:** `Show Rattle-Feed Takeover` (`UAsyncShowRattleFeedTakeover`, kind `EInfoTakeoverKind::RattleFeed`),
same contract as the other field manual nodes. Archived under Game Mechanics ("Rattle-Feed", key
`InfoTakeover.RattleFeed`), without a picture. The clip is `GameUI/InfoTakeover/RattleFeed/T_RattleFeed_ClipAtlas`, 93
frames of 480 x 270 in 8 x 12 cells (3840 x 3240), 12 fps, the last frame held 1 s. The tags are burned into the
texture, so they are English only, and "5 S" must be re-cut if the KwK 30's reload changes.

**Re-recording the clip** (DebugGame, headless):

```
Source/Tools/InfoTakeover/RattleFeed/capture_clip.sh
python Source/Tools/InfoTakeover/VulturesAt/make_clip_atlas.py --clip RattleFeed --segments 54-112,206-330   --labels "54-81:NORMAL FIRE,82-112:RELOADING  -  5 S,206-330:RATTLE-FEED"
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript   -script=Source/Tools/InfoTakeover/RattleFeed/create_rattle_feed_takeover.py -unattended -nullrhi   -DisablePlugins=Fab,BpGeneratorUltimate
```

- `-RattleFeedClipCapture` (`UBehaviourAbilityClipCaptureSubsystem`, `GameUI/InfoTakeover/Tests`; one scenario per
  ability, shared with Adrenaline Shot) spawns a player Panzer II F
  facing +X and an enemy T-26 1300 in front, turned 135° (turrets off). The Panzer's guns are held during a 4 s
  settle, then it is ordered to attack. Rattle-Feed is clicked (`ActivateBehaviourAbility(..., true)`, like the card
  without shift) the tick the first magazine has reloaded, and the run fails if the behaviour does not appear on the
  tank within 1 s. It stops 12 s later or 2.5 s after the T-26 dies. Frames go to `Saved/InfoTakeover/RattleFeed/Frames/`
  from a side camera on the Panzer's left flank. The log (`Saved/Logs/RattleFeedClip.log`, grep `ABILITY_CLIP`)
  lists the frame of every 20 mm shot (magazine drops), each finished reload and the gun's cooldown, magazine and
  reload in each phase. `-AbilityClipAt=X,Y`, `-AbilityClipDir=` and `-AbilityClipSettle=` work for every scenario.
- The 2026-11-15 recording: shots at 7-31 and 57-81, reload 82-206, Rattle-Feed on at 206, then shots every 6-7 frames
  with only short breaks. The clip keeps the second normal burst plus 1.3 s of the reload (54-112) and the first two
  Rattle-Feed magazines (206-330). The T-26 survived, so the clip has no kill. At 110-130° it died within 2-3 s of Rattle-Feed
  (too short to see the burst); at 140-150° it lived through 12 s of it.
- Every re-shoot times a little differently (the re-run on the shared capture code reloaded at 212, not 206), so
  re-pick the `--segments` frames from the new log before running `make_clip_atlas.py`. The saved frames are from that
  re-run now; the atlas in the game is from the 206 run.
- `make_clip_atlas.py` takes `--segments` (recorded frame ranges joined with a hard cut; a `Name:` prefix reads a
  second camera's `Frames<Name>` folder) and `--labels` (`first-last:TEXT`, stamped top left in the recording's
  pixels). It also writes `clip_preview.gif`.

## Adrenaline Shot takeover (field manual)

One page titled "Adrenaline Shot" under "PANZER 38(T) - VEHICLE ABILITY". Left: a clip of a Panzer 38(t) firing at a
T-26 head on, first at the normal pace (tag NORMAL FIRE - 3.3 S RELOAD) and then with Adrenaline Shot (tag ADRENALINE
SHOT), ending on the T-26 blowing up. Right: RELOAD (a third of the reload, about three times the shots), SHRAPNEL
(more shrapnel over a wider area) and THE CATCH (worse aim, a few minutes of cooldown).

The ability is `AdrenalineShot` (`EBehaviourAbilityType::AdrenalineShot`) on `Ger_Vehicle_Panzer38T`; it adds
`Beh_WeaponVFX_Pz38AdrenalineShot`, a `UBehWeaponOverwriteVFX`. Values read on 2026-11-15
(`Source/Tools/InfoTakeover/AdrenalineShot/dump_adrenaline_barrage.py`, which also reads the Wespe):

| | Value |
| --- | --- |
| Duration / ability cooldown | 12 s / 180 s |
| `ReloadSpeedMlt` | 0.33 (the KwK 38(t) holds one round; 3.3 s becomes 1.1 s) |
| `AccuracyMlt` | 0.67 |
| `WeaponCalibreMlt` | 2 (bigger shell visuals, decals and camera shake, more plate damage on bounces) |
| Added | +1 g TNT, +80 shrapnel range, +15 shrapnel damage, +20 shrapnel particles; artillery impact effects |

Node `Show Adrenaline Shot Takeover` (`UAsyncShowAdrenalineShotTakeover`, kind `AdrenalineShot`), archive key
`InfoTakeover.AdrenalineShot`. Clip `GameUI/InfoTakeover/AdrenalineShot/T_AdrenalineShot_ClipAtlas`, 87 frames in
8 x 11 cells.

```
Source/Tools/InfoTakeover/AdrenalineShot/capture_clip.sh
python Source/Tools/InfoTakeover/VulturesAt/make_clip_atlas.py --clip AdrenalineShot --segments 96-150,203-320   --labels "96-150:NORMAL FIRE  -  3.3 S RELOAD,203-320:ADRENALINE SHOT"
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript   -script=Source/Tools/InfoTakeover/AdrenalineShot/create_adrenaline_shot_takeover.py -unattended -nullrhi   -DisablePlugins=Fab,BpGeneratorUltimate
```

`-AdrenalineShotClipCapture` is the same capture as Rattle-Feed with the Panzer 38(t), the T-26 at 180° and the
ability clicked after the second reload. The 2026-11-15 recording: normal shots at frames 7 and 108 (4.2 s apart,
reload plus aiming), Adrenaline Shot on at 203, shots at 208, 244 and 280 (1.5 s apart), the T-26 died at 286.

## Wespe Barrage takeover (field manual)

One page titled "Barrage" under "WESPE - ARTILLERY ABILITY". Left: a clip spliced from two cameras: three Wespes firing
(tag THREE WESPES FIRE A BARRAGE), then the target area with the shells landing on a T-26 and a Mosin squad (tag TARGET
AREA). Right: AREA (20 s on the marked ground, even unseen), RATE (faster reload, worse aim, so the shells scatter;
several Wespes hit far harder) and THE CATCH (cannot move, two minutes of cooldown).

The ability is `BarrageAimAbility`, a `UTankAimAbilityComponent` (`EAimAbilityType::HeArtilleryBarrage`) on
`Ger_Vehicle_Wespe`: `BehaviourDuration` 20 s, behaviour `Beh_Aim_WespeBarrage` (`ReloadSpeedMlt` 0.6,
`AccuracyMlt` 0.5), `AbilitiesToRemove` Move and Reverse Move, radius 1500, cooldown 120 s. The leFH 18 reloads in 8 s,
so one Wespe fires a shell about every 5 s in a barrage (four to five per barrage).

Node `Show Wespe Barrage Takeover` (`UAsyncShowWespeBarrageTakeover`, kind `WespeBarrage`), archive key
`InfoTakeover.WespeBarrage`. Clip `GameUI/InfoTakeover/WespeBarrage/T_WespeBarrage_ClipAtlas`, 104 frames in 8 x 13
cells.

```
Source/Tools/InfoTakeover/WespeBarrage/capture_clip.sh
python Source/Tools/InfoTakeover/VulturesAt/make_clip_atlas.py --clip WespeBarrage --segments 0-48,Target:64-220   --labels "0-48:THREE WESPES FIRE A BARRAGE,64-220:TARGET AREA"
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript   -script=Source/Tools/InfoTakeover/WespeBarrage/create_wespe_barrage_takeover.py -unattended -nullrhi   -DisablePlugins=Fab,BpGeneratorUltimate
```

- `-WespeBarrageClipCapture` (`UWespeBarrageClipCaptureSubsystem`) spawns three player Wespes at the spot, an enemy
  T-26 (turrets off) and Mosin squad 4500 out, and a player Panzer II 2000 short of them (guns held) for vision: in the
  fog of war the targets were hidden and only flashed up in the blasts. After a 4 s settle with the battery's guns held
  it orders `AimAbility(target, true, HeArtilleryBarrage)` on each Wespe and records 24 s from two cameras every tick:
  `Frames/` (battery, 3500 behind its left at -35°; closer, the muzzle smoke cloud filled the picture) and
  `FramesTarget/` (target area). Grep `WESPE_BARRAGE_CLIP` for the battery's shot frames.
- The 2026-11-15 recording: volleys at frames 8-15, 145, 260-274, 375-411 and 522-546; the first shells land around
  frame 80. Both targets survived that run (the previous one lost one).

## Dig In takeover (field manual)

One page titled "Dig In" under "TANKS - VEHICLE ABILITY". Left: a clip spliced from two cameras: a Panzer IV H piling
its sandbags (close camera, every 8th frame so the 12 s build plays in about 3 s, tag DIGGING IN - 12 S), then a side
view of two T-26s firing at it (tag SHELLS HIT THE SANDBAGS). Right: THE WALL (sandbags take the hits until shot
away; heavier tanks build a stronger wall), BUILDING (no moving, turning or firing while it goes up; it starts weak) and
THE CATCH (stays put; Break Cover to move; a destroyed wall breaks cover on its own).

The ability is `IdDigIn` in the basic tank ability set, run by `UDigInComponent` on `ATrackedTankMaster`: it spawns
the BP wall (`BP_DiginWall_GerMedium` on the Panzer IV) around the hull, at a third of its health, growing once a second
(`DigInWallUpdateHealthDuringConstructionInterval`). Light / medium / heavy walls take 8 / 12 / 16 s and get
320 / 560 / 960 health (`DeveloperSettings`, after `OverallHealthMlt` 0.8). While building, Move, Reverse Move and
Rotate Towards are removed and the turrets are off; once built the turrets auto-engage but the tank does not aggro.

Node `Show Dig In Takeover` (`UAsyncShowDigInTakeover`, kind `DigIn`), archive key `InfoTakeover.DigIn`. Clip
`GameUI/InfoTakeover/DigIn/T_DigIn_ClipAtlas`, 111 frames in 8 x 14 cells.

```
Source/Tools/InfoTakeover/DigIn/capture_clip.sh
python Source/Tools/InfoTakeover/VulturesAt/make_clip_atlas.py --clip DigIn --segments "0-312/8,Wide:318-384,Wide:388-460"   --labels "0-312:DIGGING IN  -  12 S,318-460:SHELLS HIT THE SANDBAGS"
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript   -script=Source/Tools/InfoTakeover/DigIn/create_dig_in_takeover.py -unattended -nullrhi   -DisablePlugins=Fab,BpGeneratorUltimate
```

- `-DigInClipCapture` (`UDigInClipCaptureSubsystem`) spawns a player Panzer IV H facing +X and two enemy T-26s
  1500-1600 ahead facing it (turrets held). After a 4 s settle it orders `DigIn(true)`, waits until the wall's max
  health stops rising for 1.5 s, then orders both T-26s to attack and records until both are dead (+2.5 s) or 16 s. Two
  cameras every tick: `Frames/` (close, front three-quarter) and `FramesWide/` (side view, 2700 away; from 2100 the
  Panzer IV was cut off). Grep `DIGIN_CLIP` for every hit on the wall and on the tank. One T-26 died 1.4 s in after a
  single wall hit, so the clip uses two.
- The 2026-11-15 recording: wall done at frame 288 (560 hp), T-26s attack at 324, wall hits at 337 (both shells),
  446, 543 and 652 (down to 79 hp), no hit on the tank; T-26 1 died at 366, T-26 2 at 674.
- Frames 385-387 of `FramesWide/` show the first T-26's wreck swapping in with flat white quads, so the clip skips them
  (`/N` in `--segments` keeps every N-th frame of one range).

## Wall Breach takeover (field manual)

Two pages since 2026-11-25 (user request; `EWallBreachTakeoverPage`, CONTINUE / PREVIOUS / page lamps):

1. "Crushing Walls" under "HEAVY VEHICLES - SOVIET WALLS". Left: a clip of a player Tiger H1 driving through a
   Soviet `Rus_Wall_Concrete_Long`: its turret over the wall as it comes in, the wall bursting out towards the camera
   with debris and dust, the Tiger driving off and the gap left open. Right: exactly the user's two sections, title
   and line, no red labels: "Make a way through" (you can crush most walls with heavy vehicles; they are damaged by
   driving through them) and "Size dependent" (not all vehicles can crush all walls; some only the heaviest class of
   tanks). The wall rules are `UWallTankBreachComponent`'s, see `Environment/WallChunkDamage.md` "Tank breaches".
2. "Running Over Infantry" under "VEHICLES - ENEMY INFANTRY": a clip of a player Tiger driving through an enemy
   Soviet rifle squad standing in its path (crushed soldiers, the blood decal, the bodies left in the tracks; the
   clip is silent, so the scream is not in it). Right: one section, as the user asked: "Run them down". The run-over
   rules are `URTSVehicleCrushSubsystem`'s (memory/feature: vehicle crush infantry, 2026-11-25).

The clip base `UW_ClipInfoTakeover` takes pages: a child overrides `GetPageCount`, `GetPageClip` (page 1 plays the
`M_Clip*` class defaults, so the one-page manuals and their saved WBPs are unchanged), `MakePageFacts`,
`GetPageClipCaption` and `ApplyClipPageTexts`; each page is a child of a WidgetSwitcher and the shown page's clip
starts at frame 0. A fact with an empty Label shows only its title and body. The run-over clip's grid
(`M_RunOverClip`) comes from the `UW_WallBreachTakeover` constructor, so `WBP_WallBreachTakeover` was not re-saved.

Ger_IntroMission opens it right after the Okhotnik squad intel: `FSecureBaseAssaultResponse::
bShowWallBreachManualAfterIntelSquad` (on for `Mission_Intro_ClearSovietRig`) queues it behind the squad intel, also
when unit intel is turned off or the squad never showed up (`USecureBaseMission::QueueWallBreachManualAfterIntel`).

Node `Show Wall Breach Takeover` (`UAsyncShowWallBreachTakeover`, kind `WallBreach`), archive key
`InfoTakeover.WallBreach`. Clip `GameUI/InfoTakeover/WallBreach/T_WallBreach_ClipAtlas`, 85 frames in 8 x 11 cells;
run-over clip `T_RunOver_ClipAtlas` in the same folder, 80 frames in 8 x 10 cells.

```
sh Source/Tools/InfoTakeover/WallBreach/capture_clip.sh
python Source/Tools/InfoTakeover/VulturesAt/make_clip_atlas.py --clip WallBreach --segments "40-96/4,98-236/2"
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript \
  -script=Source/Tools/InfoTakeover/WallBreach/create_wall_breach_takeover.py -unattended -nullrhi \
  -DisablePlugins=Fab,BpGeneratorUltimate
```

- Unlike the other clips, the recording is pure Python (`capture_clip.py`), no C++ capture subsystem: a throwaway
  DebugGame editor (`-benchmark -fps=24`, CPU throttle off) on `RTS_ToCopySmall` places the wall (owner 2), the Tiger
  (owner 1) and three `SceneCapture2D` actors in the editor world, rebuilds the navmesh, starts PIE, and after a 3 s
  settle orders `MoveToLocation` to 26 m past the wall. Every 1/24 s of game time it exports `Frames/` (exit side,
  three-quarter), `FramesEntry/` (entry side) and `FramesWide/` (high view) at 1280 x 720 until the Tiger is 13 m
  past the wall plus 2 s. Nothing is saved. Grep `WALL_BREACH_CLIP` in `Saved/Logs/WallBreachClip.log`; the event
  frames are also in `Saved/InfoTakeover/WallBreach/capture.json`.
- On `RTS_ToCopySmall` the sun lights the wall's +Y face, so the Tiger starts on -Y (`WALL_BREACH_START=-1900`, the
  default) and bursts out of the lit face. The first run went the other way and the exit face was nearly black.
- The 2026-11-16 recording: 256 frames over 10.6 s, every tick exactly 1/24 s; first breach at frame 102 (hull 3.2 m
  short of the wall line), hull through at 152, clear at 208; the wall kept 1105 of 1400 health (21 % of its face
  gone). The clip uses the exit camera only: the approach at double speed (every 4th frame of 40-96), then every 2nd
  frame of 98-236.

Run-over clip (page 2), same pure-Python recipe:

```
sh Source/Tools/InfoTakeover/RunOver/capture_clip.sh
python Source/Tools/InfoTakeover/VulturesAt/make_clip_atlas.py --clip RunOver --segments "50-208/2"
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript \
  -script=Source/Tools/InfoTakeover/RunOver/import_run_over_atlas.py -unattended -nullrhi \
  -DisablePlugins=Fab,BpGeneratorUltimate
```

- `capture_clip.py` places the Tiger (owner 1) and a `Rus_Squad_Mosin` (owner 2, 18 m beside the lane) in the editor
  world; in PIE `MissionMapTestLibrary.map_test_place_squad_soldiers` puts the soldiers in two ranks in the lane and
  `map_test_hold_tank_fire` (every tick) keeps the Tiger from shooting them. `DodgeChance=0` comes from a
  `-ini:Game:[/Script/RTS_Survival.RTSVehicleCrushSettings]` override on the command line: the settings class is not
  exposed to Python by name, and the override never touches DefaultGame.ini. Cameras: `Frames/` (low, in front of the
  squad), `FramesSide/`, `FramesWide/`. Grep `RUN_OVER_CLIP` in `Saved/Logs/RunOverClip.log`.
- The 2026-11-25 recording: 234 frames over 9.7 s, every tick 1/24 s; hull at the squad at frame 104, Tiger clear at
  174. The clip is the front camera, every 2nd frame of 50-208. `import_run_over_atlas.py` logs whether the atlas grid
  matches the C++ constants (`RunOverClipColumns / Rows / FrameCount` in `W_WallBreachTakeover.cpp`).

## Capturing trucks takeover (field manual)

Built 2026-11-23 for the Ore Mountains mission (convoy 1, when the first escort dies; see
`Campaign/OreMountainsMission.md`, "Capturing convoy trucks", and `Missions/SurrenderingCargoTruck.md`). One page titled
"Capturing Trucks" under the kind label "CONVOYS - CAPTURE OR DESTROY", and UNDERSTOOD. The layout is the clip field
manuals' (description, a picture card on the left, three facts on the right), but the picture is a looping diagram
instead of a clip:

- **Left card, the diagram (plain UMG, built in C++, no texture):** a top-down road with a stopped olive truck, its
  20 m ring (red outline), its health bar with a red 60 % mark, and the Vultures squad icon
  (`T_Ger_Squad_VultureHazmats`) walking into the ring. Over the truck sits the **real scuttle plate**
  (`UW_SurrenderingTruckCountdown`, the class the trucks show in the world) counting down from SCUTTLING 12; when the
  squad is inside the ring (7 s) the plate is swapped for a green SURRENDERED tag for 2.5 s, then it loops. It runs on
  Slate's real-time delta from `NativeTick`, so it plays while the takeover pauses the game.
  - There is no clip because no truck Blueprint of `ASurrenderingCargoTruck` exists yet to film, and no Soviet truck
    portrait exists to show. When the truck Blueprint is in, a clip can replace the diagram (derive from
    `UW_ClipInfoTakeover`, keep `MakeFacts`).
- **Right card:** COUNTDOWN (12 seconds to scuttle: the last escort dies, every truck stops, its crew starts the
  countdown), CAPTURE (a squad within 20 m; vehicles cannot take it, under 60 % health it only scuttles) and CAPTURE OR
  DESTROY (a captured truck drives itself to the mine adit and pays 2.5 times its cargo; a destroyed one drops a cache
  to scavenge).
- **The numbers are hardcoded text** mirroring the `ASurrenderingCargoTruck` defaults: `CountdownSeconds` 12,
  `Captors` SquadsOnly, `CaptorSearchRadius` 2000 cm, `MinHealthFractionToSurrender` 0.6, `CapturedCargoMultiplier`
  2.5. Change `UW_CapturingTrucksTakeover::MakeFacts` and `GetCapturingTrucksSummary` when a truck Blueprint or the
  defaults change them.

**Mission node:** `Show Capturing Trucks Takeover` (`UAsyncShowCapturingTrucksTakeover`, kind
`EInfoTakeoverKind::CapturingTrucks`), with the pin **On Closed (Was Shown)**; self is filled in. Same contract as the
other field manual nodes (queues, waits for the start screen and cinematics, ignores "never show again", fires once
after close with the game unpaused). Archived under Game Mechanics ("Capturing Trucks", key
`InfoTakeover.CapturingTrucks`), without a picture.

**Assets:** `GameUI/InfoTakeover/CapturingTrucks/WBP_CapturingTrucksTakeover` (`UW_CapturingTrucksTakeover`, a copy of
`WBP_SpacedArmourTakeover`, so it is on the Rotsignal skin), made by
`Source/Tools/InfoTakeover/OreMission/create_ore_takeovers.py` (headless DebugGame-Cmd, `-run=pythonscript`). It is in
`Style/build_takeover_style.py`'s list.

## Direction Finding takeover (field manual)

Built 2026-11-25 for Oder-Warthe (`Campaign/OderWartheMissionFlow.md`, M03). It opens when the player takes DF 1, the
pump house (`Mission_OW_ListeningPost`).

- **Page:** one page titled "Direction Finding" under "ODER-WARTHE - RADIO DIRECTION FINDING".
- **Left, the clip:** filmed from the real tactical map of `Ger_OderWarthe`:
  1. the pump house's bearing wedge appears;
  2. an attack order narrows it;
  3. DF 2's wedge crosses it and the overlap shades in;
  4. a second attack order narrows both.
- **Tags:** small tags name each step.
- **Right, three facts:**
  - **LISTEN:** a held post draws a wedge; a lost one keeps it grey.
  - **NARROW:** when they attack, they talk; hitting an outpost makes the command post answer.
  - **CROSS:** two wedges give an area; three find the command post and reveal its targets.

Node `Show Direction Finding Takeover` (`UAsyncShowDirectionFindingTakeover`, kind `DirectionFinding`), archive key
`InfoTakeover.DirectionFinding`. Clip `GameUI/InfoTakeover/DirectionFinding/T_DirectionFinding_ClipAtlas`: 96 frames
in 8 x 12 cells. Each 1920 x 1080 frame's map square is centred in the 16:9 cell on the map's dark background.
Capture (`-DirectionFindingClip`), atlas and asset steps: `Source/Tools/InfoTakeover/DirectionFinding/README.md`.

## Document takeover (captured papers) and the Ore manifest

Built 2026-11-23. A generic takeover for a document the player finds: the typed kind label, title and intro on the
panel, then a **paper sheet** under them (aged cream paper with a thin edge, dark typewriter ink): a heading, an
optional subheading, a rule, the lines, optional small print, and an optional red **rubber stamp** tilted over the
sheet's top right. A line with `bIsMarked` is drawn in red ink on a faint red band. The sheet uses plain `UTextBlock`s
in ink colours, not the panel's bone-on-metal typewriter text (unreadable on paper, and it parses `<>` as tags), so the
sheet appears at once and only the title and intro type. It has no picture and no glass panes.

All text comes from `FDocumentTakeoverContent` (`InfoTakeoverTypes.h`): `KindLabel` (empty hides it), `Title`,
`Description` (the typed intro; rich text tags work), `PaperHeading`, `PaperSubheading`, `PaperLines`
(`FDocumentTakeoverLine`: `Text`, `bIsMarked`), `PaperFooter`, `StampText` (empty: no stamp) and `ArchiveKey`.

**Mission nodes** (both `UAsyncShowInfoTakeoverBase` subclasses, kind `EInfoTakeoverKind::Document`, pin **On Closed
(Was Shown)**, self filled in):

| Node | Input | Use |
| --- | --- | --- |
| `Show Document Takeover` | `Document` (`FDocumentTakeoverContent`; split the pin or make the struct) | Any found document; text written on the node. |
| `Show Ore Manifest Takeover` | none | The Ore Mountains radio outpost manifest; text fixed in C++ (`UAsyncShowOreManifestTakeover::MakeOreManifest`). |

- Documents are story content, not unit intel: they show while "never show again" is ticked, have no checkbox, and
  queue and wait like the field manuals.
- Archived under **Lore**, keyed `InfoTakeover.Document.<ArchiveKey>` (the title when the key is empty), once per key.
  The archive text is the intro, the heading and one paragraph per line and footer.

**The Ore manifest** (`MakeOreManifest`, 2026-11-23): kind label "RADIO OUTPOST - CAPTURED DOCUMENT", title "Convoy
Manifest", intro "Found on the radio operator's desk, translated from the Russian. Most of it is routine. One column is
not." The sheet: CONVOY MANIFEST, "Wismut yard, valley road west. Columns for the western outposts.", six columns
following the convoy line-up (fuel; supply with the weighbridge inspection; shells, ammo trucks turn off for the battery;
supply with rifle squads riding; **column 5 marked: supply and one armoured truck, cargo SEALED, do not inspect, do not
open, destination Berlin**; heavy escort for the knoll), the footer "Questions about column 5 are not to be sent over
the radio." and the stamp BERLIN. Archive key `OreManifest`.
- **Lore rules** (`OreMountainsMission.md`, "Act 1 clues" and the intercept rules): it never says why Berlin, mentions
  no cryo and nothing about where Radixite comes from. Keep it that way when editing the text.
- The lead wires it into `Mission_Ore_RadioOutpost` (guards dead -> manifest -> mark convoy 5 / start RecoverCases from
  On Closed).

**Assets:** `GameUI/InfoTakeover/Document/WBP_DocumentTakeover` (`UW_DocumentTakeover`, a copy of
`WBP_SpacedArmourTakeover`), made by the same `create_ore_takeovers.py`; in `build_takeover_style.py`'s list.

## The library

`Content/RTS_Survival/Blueprints/GameUI/InfoTakeover/DT_VehicleInfoTakeovers`, one row (`FVehicleInfoTakeoverEntry`)
per prefab:

| Field | Meaning |
| --- | --- |
| DisplayName | The typewritten title. Empty uses the unit's training title or subtype name. |
| Description | Text left of the picture. Any length: it wraps and scrolls inside the panel. |
| ProductionYear | Free text: `1931`, or `????` for fictional vehicles. Empty also shows `????`. |
| Image | The picture. Empty uses the unit's `DT_TrainingOptions` brush; with neither, no picture is shown. |
| ReferenceVehicleClass | The Blueprint the row was written for. Only the validation run uses it. |

The rows are authored by `Source/Tools/InfoTakeover/author_info_takeovers.py`. Edit rows in the editor, or change the
script and re-run it; it overwrites only its own rows.

Rows so far: T26, BT7, BT7_4, T34_76, T34E, KV1_L11. Their portraits are `UnitIcons/Rus/T_Rus_*`, made with the unit
portrait tool (`Source/Tools/UnitPortraits`, list `rus_info_takeover.txt`).

## The squad library

`Content/RTS_Survival/Blueprints/GameUI/InfoTakeover/DT_SquadInfoTakeovers`, one row (`FSquadInfoTakeoverEntry`) per
prefab. Fields as the vehicle library, with `ServiceYear` in place of `ProductionYear` (shown as "In service since:")
and `ReferenceSquadClass` (a squad controller or solo infantry Blueprint) in place of `ReferenceVehicleClass`.

The rows are authored by `Source/Tools/InfoTakeover/author_squad_info_takeovers.py` (headless DebugGame-Cmd, same as
the vehicle script). Rows so far:

| Row | Title | Year | Reference class | Picture |
|---|---|---|---|---|
| MosinBazooka | Bazooka Rifleman | 1942 | `BP_Solo_Rus_Bazooka` | `T_Rus_Squad_MosinBazooka` (portrait tool) |
| HazmatEngineers | Hazmat Engineers | ???? | `Rus_Squad_HazmatEngineers` | `T_Rus_Squad_HazmatEngineers` (portrait tool on `BP_Solo_Rus_Hazmat`, same suit + SVT) |
| Vultures | Hazmat Vultures | ???? | `Squad_Ger_HazmatVultures` | `T_Ger_Squad_VultureHazmats` (training card art) |
| Scavengers | Scavengers | ???? | `Squad_Ger_Scavengers` | `T_Ger_Squad_Scavengers` (training card art) |

A script row with `"Texture"` uses a texture already in the project; a row with `"Portrait"` imports the picked PNG.

Portraits: `UnitIcons/Rus/T_Rus_Squad_*`, from the unit portrait tool with list `rus_squad_info_takeover.txt` and picks
`rus_squad_info_takeover_picks.txt`. A solo infantry unit pictures on its own (`-UnitPortraitShots=2`; it does not fire,
so the idle pictures are the usable ones). A squad controller cannot be pictured: its soldiers are separate actors, so
list one soldier Blueprint (or a solo unit) instead.

Squads open in `WBP_VehicleInfoTakeover` (`UW_VehicleInfoTakeover::SetupSquad`), so the look stays in one Blueprint. The
label reads INFANTRY INTEL; the specifications are the service year, soldiers (living when the node ran) and health per
soldier (the toughest soldier's max health); weapons are every soldier's weapon, identical ones merged ("4x"). Archived
under Units, keyed `InfoTakeover.Squad.<name>`.

## What the widget shows

- **Title:** the vehicle name, typed in about one second with `Mission_Text_OneSecond`.
- **Description:** typed after the title. It opens with `Mission_Title_2Seconds` or `Mission_Title_3Seconds` (random),
  then chains `Mission_Title_Fast_OnGoing_3Sec` while the text is still typing. The sound fades out when typing ends.
  Long texts type faster, so typing never takes longer than `M_Style.DescriptionMaxTypingSeconds` (9 s).
- **Picture, then specifications:**
  - Production year.
  - Frontal armor: the lowest and highest front plate, without the cupola.
  - Weakest side armor and weakest rear armor: the thinnest plate, named without left or right.
  - Add-on panels are shown as `(+N add-on)`.
- **Weapons:** the variant selection screen's weapon icon and its damage, penetration, reload and magazine pip bars
  (shared code in `UnitVariantWeaponWidgets`).
- **Footer:** "Never show these info widgets again" (off by default) and UNDERSTOOD. Only unit takeovers (this widget)
  have the box; field manuals show UNDERSTOOD alone.
  - Enter, Space or Escape finishes the typing; pressed again, it acts as Understood.

## Look: "Rotsignal" (black oxide, signal red)

Every takeover, all six, has the same look. On 2026-11-12 it went through two rounds:
- **Round 1:** the user picked option D, a field-grey instrument panel with brass and cyan, from five candidates
  (`Saved/InfoTakeover/StyleOptions/`).
- **Round 2:** they found the neon cyan and the yellow-brown generic and asked for the black Frontstahl ("Frostyle")
  family instead. From F, G and H (`Saved/InfoTakeover/StyleOptions2/`) they picked **G**.
- **Colours:** every colour comes from the Frontstahl skin's own tones (`themes_coh.py`, `13_Frontstahl_Gunmetal`).
  There is no cyan and no brass. The one accent is a dull signal red (`danger` `#c8382a`, dulled to `#a8382c`;
  labels `#c24a3a`).

| Part | Brush (`M_Style`) | Asset (`GameUI/InfoTakeover/Style/`) |
| --- | --- | --- |
| Panel background | `Skin.Card.Normal` | `MI_InfoTakeover_Plate` |
| UNDERSTOOD / CONTINUE / PREVIOUS | `Skin.ConfirmButton` (n, h, p) | `MI_InfoTakeover_Button_n`, `_h`, `_p` |
| Body panes | `Skin.VariantSectionGlass` | `MI_InfoTakeover_PaneGlass` |
| Glass over every picture | `PictureGlass` | `MI_InfoTakeover_PictureGlass` |
| Page lamps | `MI_InfoTakeover_PageLamp` (project setting, see below); fallback `PageLampLit` / `PageLampUnlit` | `MI_InfoTakeover_PageLamp`; fallback `Textures/T_InfoTakeover_PageLamp_On`, `_Off` |

- **Panel background:** a black-oxide plate (`#0f1112`) with a chrome rim, dome bolts and a few bright nicks.
  - The title sits on a raised gunmetal plate (`#25282b`) with a knurled right end cap. One dull red index tab sits
    left of the title. The page lamps (below) sit at the plate's right end.
  - The footer sits on the same kind of raised plate, with knurled end caps.
  - The art (`Textures/T_InfoTakeover_Plate`) is 1240 x 920, one texel per unit.
- **Text colours** (`Skin.LabelColor` / `Skin.AccentColor`, set by the build script):
  - section labels, the kind label and the divider are dull red `#c24a3a`;
  - the vehicle's weapon pips are bone `#d8d4c6`;
  - titles and body text keep the skin's bone colours.
- **UNDERSTOOD / CONTINUE / PREVIOUS:** a Frontstahl gunmetal key with a chrome rim and a dull red lamp strip, under
  light glass (`GlassEnable` on, so the hover shows).
- **Body panes:** clear scratched glass with a neutral steel edge tint, an instance of the shared `M_UI_GlassPane`.
- **Glass over pictures:** gauge glass with a reflection streak, rim and scratches, also an instance of
  `M_UI_GlassPane`.
- **Hover tint:** a faint red, `HoverTint` (0.85, 0.30, 0.25, 0.4), on every glass instance.
- **Page lamps:** signal pilot lamps drawn by a shader (see "Page lamp shader" below): a knurled steel bezel around
  domed red fresnel glass. Lit, the glass glows with a hot filament core and a halo lights the title placard around
  it; unlit, it is dark red glass with a window reflection.
- **In-picture colours are not part of the look:** the camera illustrations' cyan HUD bits and the cover page's
  green / yellow / red labels stay as they are.
- **The master is unchanged.** Both glass instances only set parameters of the shared `M_UI_GlassPane`.
- **How the plate stretches:** `M_InfoTakeover_GlassOverArt` (plate and button) slices the art vertically into three.
  - The top 150 and bottom 124 rows stay where they are. The middle is cropped with a 40-row cross-fade, never
    stretched.
  - Every panel height from about 600 to 920 units therefore keeps its placard, rivets and footer rail.
  - The material can also draw one rectangle of the `M_UI_GlassPane` HLSL over the art (`GlassEnable` and
    `Glass*`). This look leaves it off.
- **Picture glass (C++):** `UW_InfoTakeover::MakePictureWithGlass` wraps a picture in an overlay with a
  hit-test-invisible Image on top, in the `PictureGlass` brush. A brush with no resource draws nothing. It covers:
  - the vehicle portrait (`UW_VehicleInfoTakeover::BuildPicture`);
  - every camera controls illustration (all three pages);
  - the nomadic illustration;
  - the three cover screenshots;
  - the spaced armour render.

### Pages: lamps and PREVIOUS

Every takeover has them, from the base class:
- **Page lamps:** one lamp per page in the title placard's top right (`UW_InfoTakeover::BuildPageLampRow`, 24 units
  each, 6 apart; the row's right edge is 50 units in from the plate's right, its top 47 down, so lamp centres sit at
  y 59 in the placard's lamp seat). The lamp of the page that shows is lit; the others are dark. A one-page takeover
  shows one lit lamp. The lamps were baked into the plate art before 2026-11-12; they are widgets now, so the art
  has none.

### Page lamp shader

Since 2026-11-16 every takeover's page lamps come from one project setting, not from the Blueprints:
- **Setting:** Project Settings > Game > Indicator Lamps > Info Takeover Page Lamps (`UIndicatorLampSettings`,
  `Config/DefaultGame.ini`): `PageLampMaterial` = `Style/MI_InfoTakeover_PageLamp`, `PageLampHaloScale` 3,
  `PageLampFadeSeconds` 0.15. Empty material = the old flat `PageLampLit` / `PageLampUnlit` brushes, no halo.
- **Material:** `/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/AnalogGauges/M_UI_SignalLamp`, a UI material in
  AlphaComposite (premultiplied) blend. One master, two passes (`Pass` scalar):
  - pass 0, the lamp: knurled steel bezel with a raised crest and a dark seat, domed red glass with fresnel rings
    (faded out where they would alias), a caustic and window streak when dark, a hot core and coiled filament when
    lit, and red spill on the inner bevel;
  - pass 1, the halo, on a quad `HaloScale` times the lamp: a soft contact shadow of the bezel (alpha) plus additive
    red light (rgb, alpha 0), so the plate and title around a lit lamp light up. It fades to zero before its edge.
  - `Power` 0..1 fades both; the clock ignores pause, so the filament flickers faintly on the paused takeover.
- **Widgets (C++):** `FPanelSignalLamp` (`GameUI/IndicatorLamps/PanelSignalLamp.h`) builds the lamp image and the
  halo image, each with its own dynamic instance, and ramps `Power` over `PageLampFadeSeconds` of real time.
  `UW_InfoTakeover` puts the halos in their own row under the lamp row (same boxes and padding), so a halo never
  depends on which neighbour draws first. The halo is scaled up by a render transform: it never moves the layout,
  and no ancestor clips it.
- **Build:** `Source/Tools/InfoTakeover/Style/build_page_lamps.sh` (headless Development commandlet running
  `build_page_lamps.py`; never run that script nested inside UECP `execute_python`, it crashed the editor).
- **Check:** `Source/Tools/InfoTakeover/Style/run_page_lamp_check.sh 1920x1080 2560x1080 ...` runs
  `-CameraControlsTakeoverValidate` per resolution (`-ForceRes` + `r.SetRes`, else the offscreen viewport drops to
  500p). Each page logs `CAMERA_TAKEOVER page lamps (<W>x<H>)` with the row rect and lit lamp centre in plate units
  against the expected seat, the halo flag and each lamp's power; shots land in `Saved/InfoTakeover/PageLamps/<WxH>/`.
- **PREVIOUS:** left of CONTINUE / UNDERSTOOD, in the same button style. It shows from page two on and is collapsed
  on page one. It goes back one page and never closes the takeover or fires On Closed. Enter, Space and Escape still
  act only as the footer button (`PressConfirmButton`).
- **For a multi-page takeover:**
  - override `GetPageCount`, `GetCurrentPageIndex` and `TryShowPage(PageIndex)`, which shows that page: switcher,
    texts, button label, illustration loops, `RestartTyping`;
  - the base's CONTINUE and PREVIOUS call `TryShowPage` and then `RefreshPageControls` (lamps, PREVIOUS);
  - `UW_CameraControlsTakeover` is the only one so far. Going back restarts that page's illustration loops.

### Glass hover

Every glass element of a takeover reacts to the mouse like the main UI's glass (control groups, commander rows):
- **The effect:** the shared `M_UI_GlassPane` scalars `Hover`, `HoverSweep` and `Pressed`.
  - Hover fades in over 0.12 s and out over 0.22 s.
  - One light band crosses the glass per mouse enter, over 0.26 s.
  - Press takes 0.05 s.
  - The timings are in `M_Style.GlassHoverTimings`.
- **The ramp code:** `FGlassHoverAnimator` (`GameUI/GlassButtons/GlassHoverAnimator.h`). New glass widgets should use
  it; the control group slot and the commander ability row still have their own copies.
- **Which elements, each with its own dynamic material instances so only the hovered one lights up:**
  - every body pane (`RegisterGlassPane`), quieter than a button because it carries text: `PaneHoverSweepIntensity`
    0.07, `PaneHoverScratchBoost` 0.25, `PaneHoverEdgeTint` 0.04;
  - every picture's glass (`MakePictureWithGlass`). Hovering the card / pane a picture sits in lights its glass too;
  - the footer buttons (n, h and p dynamic instances), which also darken while pressed.
- **Under pause:** `UW_InfoTakeover::NativeTick` drives it on real time, like the typewriter.
- **Tests and captures:** `SetGlassElementForcedHover(Index, bHovered, FrozenSweep)` hovers an element without a mouse,
  and `GetGlassElementHover` reads its `Hover`.

**Rebuilding** (scripts in `Source/Tools/InfoTakeover/Style/`; `OPTION` in `build_takeover_style.py` picks the look,
`'G'` live, `'D'` the brass instrument panel it replaced):
1. `python gen_style_art_dark.py G` draws the art into `Saved/InfoTakeover/Style/art/`.
   - It also draws options F and H.
   - `gen_style_art.py` draws D and the round-1 options A, B, C and E.
2. `options_spec.py` holds, per option, the glass parameters (with `HoverTint`), `label_color` and `accent_color`.
3. `build_takeover_style.py` imports the textures, builds the material and instances, and sets the brushes and
   colours on all six takeover Blueprints.
   - Run it with `UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<path>
     -unattended -nullrhi`.
   - It needs an editor DLL that has `FInfoTakeoverStyle::PictureGlass`.
   - It saves each Blueprint's previous `M_Style` once per option to
     `Saved/InfoTakeover/Style/style_backup_before_<OPTION>.json`; the script's
     `restore()` puts it back.
4. Each takeover Blueprint keeps its own copy of `M_Style`, so a new takeover copied from `WBP_VehicleInfoTakeover`
   inherits the look.

**Not done:** the art is 1x. Every size in `gen_style_art.py` is in texels, so making it 2x for 1440p and 4K means
scaling the whole script.

## Archive entry

Every takeover is also archived, with the same description and picture, under the Units tab. It shows the usual "New
Archive item!" pop-up with the picture:

- It is archived when Understood closes the takeover, so the pop-up shows on the unpaused HUD.
- With the takeovers turned off ("never show again"), the node still archives the vehicle and shows the pop-up at once.
  Takeovers still waiting in the queue when the box is ticked are archived too.
- Entries are keyed by vehicle name (`InfoTakeover.Vehicle.<name>`). A vehicle that was already archived, for example
  in a later mission, is not added again and shows no pop-up.
- **Mechanism:** `ERTSArchiveItem::CustomContent` with `FArchiveCustomContent`, added through
  `UMainGameUI::AddNewCustomArchiveItem`. It fills the existing `W_BP_ArchiveItem` (`ItemImage`, `RichTitleText`,
  `RichDescription`) and `W_BP_ArchivePushNotification` (`ArchiveButton`) from C++, so the Blueprints are unchanged.
- **Not saved to the player profile yet:** archive save data (`SetupArchiveWithPlayerProfileSaveData`) is still a todo
  for every archive item.

## The setting

- **Unit intel only:** `URTSGameUserSettings::bM_ShowInfoTakeovers` (config) covers vehicles, squads and (later)
  buildings: `InfoTakeoverKinds::GetIsUnitIntel`. While it is off, those calls archive the entry and return false.
- **Game mechanics always show:** every other kind (camera controls, cover, spaced armour, nomadic conversion, BXP
  pack-up, the ability clips, and any new field manual by default) ignores the setting. They have no checkbox
  (`UW_InfoTakeover::GetOffersNeverShowAgain` is false; only `UW_VehicleInfoTakeover` returns true), closing them never
  writes the setting, and ticking the box on a unit takeover drops only the unit intel waiting in the queue; field
  manuals queued behind it keep their place and still open.
- **The checkbox:** ticking "never show again" on a unit takeover and pressing Understood saves it off at once
  (`URTSSettingsMenuSubsystem::SetShowInfoTakeoversImmediately`).
- **Turning it back on:** Escape menu > Settings > Gameplay > "Show unit info pop-ups" (`M_CheckShowInfoTakeovers`
  in `BP_W_EscapeMenuSettings`).

## Code

`Source/RTS_Survival/GameUI/InfoTakeover/`:

| Class | Role |
| --- | --- |
| `UW_InfoTakeover` | Abstract base, a `UW_TutorialOvertake`, so the pause lock and the map smoke tests' `OnClickedContinue` work. Builds the frame in C++. Handles typewriter, sounds, checkbox and Understood. |
| `UW_CameraControlsTakeover` | The camera controls field manual: three cards, each with a looping illustration widget. Blueprint child `WBP_CameraControlsTakeover`. |
| `UAsyncShowCameraControlsTakeover` | The `Show Camera Controls Takeover` mission node. It waits on `UInfoTakeoverSubsystem::OnTakeoverRequestFinished` for its request id. |
| `UW_CoverTakeover`, `UAsyncShowCoverTakeover` | The cover field manual and its node. |
| `UW_SpacedArmourTakeover`, `UAsyncShowSpacedArmourTakeover` | The spaced armour field manual and its node. |
| `UW_NomadicConversionTakeover`, `UAsyncShowNomadicConversionTakeover` | The nomadic conversion field manual (two pages: conversion, build radius; a looping illustration each) and its node. |
| `UW_BxpPackUpTakeover`, `UAsyncShowBxpPackUpTakeover` | The building expansion pack-up field manual (one page, looping illustration, slot legend) and its node. |
| `UW_ClipInfoTakeover` | Abstract base of the clip field manuals: description, a flipbook clip card played on real time (`M_Clip*` on the class defaults) and a facts card. Children set the atlas and override `MakeFacts` and `GetClipCaption`. |
| `UW_VulturesAtTakeover`, `UAsyncShowVulturesAtTakeover` | The Vultures AT stick grenade field manual (clip, three facts) and its node. |
| `UW_RattleFeedTakeover`, `UAsyncShowRattleFeedTakeover` | The Rattle-Feed field manual (clip, three facts) and its node. |
| `UW_AdrenalineShotTakeover`, `UAsyncShowAdrenalineShotTakeover` | The Adrenaline Shot field manual (clip, three facts) and its node. |
| `UW_WespeBarrageTakeover`, `UAsyncShowWespeBarrageTakeover` | The Wespe Barrage field manual (two-camera clip, three facts) and its node. |
| `UW_DigInTakeover`, `UAsyncShowDigInTakeover` | The Dig In field manual (two-camera clip, three facts) and its node. |
| `UW_WallBreachTakeover`, `UAsyncShowWallBreachTakeover` | The crushing field manual (page 1: Tiger through a Soviet wall clip, two facts; page 2: running over infantry) and its node. |
| `UW_CapturingTrucksTakeover`, `UAsyncShowCapturingTrucksTakeover` | The capturing trucks field manual (looping UMG diagram with the real scuttle plate, three facts) and its node. |
| `UW_DocumentTakeover`, `UAsyncShowDocumentTakeover`, `UAsyncShowOreManifestTakeover` | The generic captured-document takeover (paper sheet, marked lines, stamp; text in `FDocumentTakeoverContent`), its node with the text on the node, and the Ore manifest node. |
| `UW_VehicleInfoTakeover` | The vehicle body, and the squad body (`SetupSquad`). Blueprint child `WBP_VehicleInfoTakeover` holds `M_Style`; its skin is copied from `BP_W_UnitVariantSelectionMenu`. |
| `UPauseSafeProtoRichTextBlock` | Proto's typewriter driven on real time. Proto uses world timers, which stop while the game is paused. |
| `UInfoTakeoverSubsystem` | Queue, setting check, library lookup. |
| `UInfoTakeoverSettings` | Project Settings > Game > Info Takeovers: widget class and library. |
| `VehicleInfoTakeoverContent` | Reads name, picture, weapons and armour off the live vehicle. |
| `SquadInfoTakeoverContent` | Reads name, picture, soldiers, health and weapons off a live squad controller or solo infantry unit. |

The Proto text styles are `DT_InfoTakeoverTitleText` and `DT_InfoTakeoverBodyText`. Both are copies of
`DT_AnimatedRichText` with the animated font material.

### Adding buildings or squads later

1. Add a row struct.
2. Add a `UW_<Kind>InfoTakeover` subclass that fills `GetBodyColumn()`.
3. Add a content builder.
4. Add a queue branch in `UInfoTakeoverSubsystem::CreateTakeoverWidget` (keyed by `EInfoTakeoverKind`).
5. Add a mission node.

The pause, typewriter and sounds come from the base. A new unit kind must also return true from
`GetOffersNeverShowAgain` and be listed in `InfoTakeoverKinds::GetIsUnitIntel`; anything else is a field manual that
always shows.

## Validation

Run with `-InfoTakeoverValidate`, best with `-game -RenderOffscreen`. Grep the log for `INFO_TAKEOVER`. The run:

- Spawns every row's reference vehicle and logs the content.
- Opens all rows, plus a very long and a very short custom takeover.
- Screenshots the title, the typing and the finished state to `Saved/InfoTakeover/<height>p/`.
- Checks that unit takeovers show the "never show again" box and the field manual does not.
- Ticks "never show again" on the last unit takeover, with the cover field manual and one more unit takeover queued
  behind it. The field manual must still open; the unit takeover must be dropped. Checks that the setting turned off
  and that the settings menu follows it.
- With the setting off, queues another cover field manual and checks that it opens and that closing it leaves the
  setting off.
- Restores the setting and quits.

Run with `-CameraControlsTakeoverValidate` for the camera controls takeover. Grep the log for `CAMERA_TAKEOVER`. The run:

- Calls the node while the start-game screen waits and checks that nothing opens before start is pressed.
- Screenshots each page's typing and eight frames of its illustrations, plus the HUD after closing, to
  `Saved/InfoTakeover/CameraControls/<height>p/`.
- Checks, from the Slate widgets, that every illustration changes between frames while the game is paused.
- Checks the illustration counts: 3 on the camera page, 1 on the panning page, 1 on the marquee page.
- Checks the page lamps and PREVIOUS on every page: 3 lamps with the shown page lit, and PREVIOUS only from page 2
  on (`CAMERA_TAKEOVER page controls`).
- **Glass hover:** after the page-1 frames it hovers the first card pane, with the light band held at 0.45, and
  UNDERSTOOD/CONTINUE. It checks that both `Hover` values reach 0.9 or more within 0.8 s and drop back to 0.05 or
  less 0.8 s after release. Shots: `CameraControls_hover_midsweep` and `CameraControls_hover`.
- **PREVIOUS:** on page 3 it presses PREVIOUS once. Page 2 must show on the still paused, still open takeover, with
  lamp 2 lit and On Closed not fired (shot `CameraControls_previous_back_to_page2`). It then continues to page 3 and
  UNDERSTOOD.
- Presses CONTINUE on each page: the next page must show on the still paused takeover, and On Closed must not fire
  yet. Shots of later pages are named `CameraControls_page<n>_*` (page 2 is panning, page 3 is marquee).
- Checks that On Closed fires once, with the game unpaused, after UNDERSTOOD on the last page.
- Result on 2026-11-12, with the panning page, Development build, 1080p: PASS.
- With unit intel turned off, calls the node again and checks that it still opens without the never-show box, then
  queues two more behind it: all three must open in turn and close as shown (`CAMERA_TAKEOVER kept queue`).
- Restores the setting and quits.

Run with `-CoverTakeoverValidate` for the cover takeover. Grep the log for `COVER_TAKEOVER`. The run:

- Calls the node before the start button and checks that nothing opens until start is pressed.
- Checks that the three preview screenshots loaded.
- Screenshots the typing, the finished panel and the HUD after closing to `Saved/InfoTakeover/Cover/<height>p/`.
- Presses UNDERSTOOD and checks that On Closed fires once, with the game unpaused.
- With unit intel turned off, calls the node again and checks that it still opens without the never-show box and
  closes once as shown.

Run with `-SpacedArmourTakeoverValidate` for the spaced armour takeover. Grep the log for `SPACED_ARMOUR_TAKEOVER`. It
runs the same checks as the cover run, and also checks that the Blueprint child (not the unstyled C++ widget) opened
and that the plate render loaded. The finished shot waits for the typewriter (up to 120 s), because a DebugGame run
on a busy GPU types slower than the clock. Screenshots go to `Saved/InfoTakeover/SpacedArmour/<height>p/`
(`SpacedArmour_0_typing`, `_1_done`, `_2_after_close`).

Run with `-NomadicConversionTakeoverValidate` for the nomadic conversion takeover. Grep the log for `NOMADIC_TAKEOVER`.
It runs the cover run's checks, and also:
- checks that the Blueprint child (not the unstyled C++ widget) opened and that `WBP_NomCnv_Deploy` loaded;
- takes eight frames of the illustration 1.2 s apart (`NomadicConversion_frame_<n>`), and checks from the Slate widgets
  that it changes between them while the game is paused;
- waits for the typewriter before the finished shot;
- presses CONTINUE, checks the build radius page shows (page lamps 2 of 2 with the second lit, PREVIOUS visible,
  `WBP_NomCnv_Radius` loaded), and watches that page the same way before UNDERSTOOD.

Screenshots go to `Saved/InfoTakeover/NomadicConversion/<height>p/` (`NomadicConversion_0_typing`, `_frame_1..8`,
`_1_done`, the same with `NomadicConversion_radius_` for page 2, and `NomadicConversion_2_after_close`). PASS at 1080p
and 720p on 2026-11-12 with both pages (Development).

Run with `-BxpPackUpTakeoverValidate` for the building expansion pack-up takeover. Grep the log for
`BXP_PACKUP_TAKEOVER`. It runs the nomadic run's checks (start-screen wait, Blueprint child opened, illustration
`WBP_BxpPack_Loop` loaded, game paused, eight frames 1.5 s apart that must differ while paused, typewriter wait, On
Closed once and unpaused, still opening while unit intel is off), and also:
- checks that the three slot legend pictures loaded;
- then restarts the loop at each beat (0.4, 2, 3, 4.6, 5.6, 7.8, 9.3, 10.6 s) for a still, `BxpPackUp_beat_<s>s`.
  The shot is taken three frames after the jump, so the widgets show the new time.

Screenshots go to `Saved/InfoTakeover/BxpPackUp/<height>p/`. Result on 2026-11-12, DebugGame, `RTS_ToCopySmall`, 1080p
and 720p: PASS.

Run with `-VulturesAtTakeoverValidate` for the Vultures AT takeover. Grep the log for `VULTURES_AT_TAKEOVER`. It runs
the spaced armour run's checks (start-screen wait, Blueprint child opened, game paused, typewriter wait, On Closed once
and unpaused, still opening while unit intel is off), and also checks that the clip atlas loaded and that the clip changes frame at
least 20 times while the game is paused, with two clip shots at 2 s and 3.6 s (`VulturesAt_clip_<n>_frame<cell>`).
Screenshots go to `Saved/InfoTakeover/VulturesAt/<height>p/`. Result on 2026-11-15, DebugGame, `RTS_ToCopySmall`: PASS
at 720p (96 frame changes in 10 s) and with `-ResX=1920 -ResY=1080`, which got a 500p offscreen viewport on this
machine (30 changes in 10 s: the slow run skips frames, the clip keeps real time). After the move to
`UW_ClipInfoTakeover` it passed again at 720p (99 changes in 10 s).

Run with `-RattleFeedTakeoverValidate` for the Rattle-Feed takeover. Grep the log for `RATTLE_FEED_TAKEOVER`. Same
checks as the Vultures AT run. Screenshots go to `Saved/InfoTakeover/RattleFeed/<height>p/`. Result on 2026-11-15,
DebugGame, `RTS_ToCopySmall`, 720p: PASS (100 frame changes in 10 s while paused).

Run with `-AdrenalineShotTakeoverValidate` (`ADRENALINE_SHOT_TAKEOVER`) and `-WespeBarrageTakeoverValidate`
(`WESPE_BARRAGE_TAKEOVER`) for the two newest field manuals; same checks as the Vultures AT run. Result on 2026-11-15,
DebugGame, `RTS_ToCopySmall`, 720p: both PASS (66 and 57 frame changes in 10 s while paused).

Run with `-DigInTakeoverValidate` (`DIGIN_TAKEOVER`) for the Dig In takeover; same checks. Result on 2026-11-15,
DebugGame, `RTS_ToCopySmall`, 720p: PASS (104 frame changes in 10 s while paused).

Run with `-WallBreachTakeoverValidate` (`WALL_BREACH_TAKEOVER`, `UWallBreachTakeoverValidateSubsystem`) for the Wall
Breach takeover; same checks as the Dig In run, plus both pages since 2026-11-25: two lamps, CONTINUE shows page 2
(lamp 2 lit, PREVIOUS shown, its clip from frame 0, still open), the run-over clip plays while paused, PREVIOUS goes
back, CONTINUE again, then UNDERSTOOD closes it once. Result on 2026-11-25, DebugGame, `RTS_ToCopySmall`, 720p: PASS
(walls 104 frame changes in 10 s, run-over 79 in 7 s, paused). Screenshots in `Saved/InfoTakeover/WallBreach/720p/`
(`WallBreach_1_done`, `WallBreach_3_runover_done`, `WallBreach_runover_clip_frame*`).

Run with `-OreTakeoversValidate` (`ORE_TAKEOVERS`, `UOreTakeoversValidateSubsystem`) for the two Ore Mountains
takeovers, best through `Source/Tools/InfoTakeover/OreMission/run_validate.sh` (DebugGame, `-game -RenderOffscreen`,
`RTS_ToCopySmall`, 1280 x 720, log `Saved/Logs/OreTakeoversValidate.log`). The run:
- calls `Show Capturing Trucks Takeover` while the start-game screen waits and checks nothing opens before start;
- checks the Blueprint child (not the unstyled C++ widget) opened, paused, without the never-show box, with its
  diagram built; samples the diagram's countdown every tick while paused (at least 4 different values and the
  SURRENDERED tag must show); shots `CapturingTrucks_0_typing`, `_diagram_countdown` (3 s), `_diagram_surrendered`
  (8 s), `_1_done` (after the typewriter), `_2_after_close`;
- presses UNDERSTOOD: On Closed once, shown, unpaused;
- turns unit intel off and calls `Show Ore Manifest Takeover`: it must still open (Blueprint child, paused, no box)
  with 6 sheet lines and the stamp; shots `OreManifest_0_typing`, `_1_done`, `_2_after_close`; On Closed once,
  unpaused;
- restores the setting and quits.

Screenshots go to `Saved/InfoTakeover/OreTakeovers/<height>p/`. Result on 2026-11-23, DebugGame, `RTS_ToCopySmall`,
720p: PASS (6 countdown values and the surrendered tag seen while paused; manifest 6 lines + stamp). The 16-unit gap
under the paper sheet was added after that run (layout only, compiled, not re-shot).
