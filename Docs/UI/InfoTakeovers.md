# Info takeovers (vehicle info widget)

A paused, full-screen panel that introduces a unit the first time a mission shows it. Built 2026-11-11 for vehicles; the
base is ready for buildings and squads.

## Using it in a mission graph

| Node (on `UMissionBase`) | What it does |
| --- | --- |
| `Show Vehicle Info Takeover (Vehicle, Library Entry)` | Opens the prefab row `Library Entry` of `DT_VehicleInfoTakeovers` for the live `Vehicle`. The pin is a dropdown of the row names. |
| `Show Custom Vehicle Info Takeover (Vehicle, Info)` | Same widget, with the text written on the node (split the `Info` pin). |

- The first parameter is the tank master in the world. Its weapons and armour are read from it when the node runs, so
  they always match the current balance. A queued takeover still shows them if the tank dies before it opens.
- Both nodes return false when the player turned info takeovers off, or when the vehicle or row is missing.
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
Escape act like the button. "Never show these info widgets again" is the same footer checkbox on every page, and it is
read when the last page closes. Pages follow `ECameraControlsTakeoverPage`; `TryShowNextPage` steps to the next one
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
- Was Shown is false when the panel never opened:
  - the player turned info takeovers off, so On Closed fires in the same frame;
  - "never show again" was ticked on an earlier takeover while this one waited in the queue;
  - the widget could not be created.
- If the world ends first, On Closed does not fire.
- It queues with the vehicle takeovers and waits for cinematics, like they do. All info takeovers also wait while the
  start-game screen (`UPlayerStartGameControl`) waits for the player, so the node can be called on the mission's
  first frame.
- It is archived under Game Mechanics ("Camera Controls") when it closes, or when the node runs while the takeovers are
  off.

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
- it respects "never show";
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
- **Footer:** "Never show these info widgets again" (off by default) and UNDERSTOOD.
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
| Page lamps | `PageLampLit`, `PageLampUnlit` | `Textures/T_InfoTakeover_PageLamp_On`, `_Off` |

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
- **Page lamps:** a steel ring with a dull red pilot lamp when lit, and dark red glass when unlit.
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
  each, 6 apart). The lamp of the page that shows is lit (cyan, `PageLampLit`); the others are dark amber glass
  (`PageLampUnlit`). A one-page takeover shows one lit lamp. The lamps were baked into the plate art before
  2026-11-12; they are widgets now, so the art has none.
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

- **One switch for every kind:** `URTSGameUserSettings::bM_ShowInfoTakeovers` (config) covers vehicles, buildings and
  squads.
- **The checkbox:** ticking "never show again" and pressing Understood saves it off at once
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
| `UW_VehicleInfoTakeover` | The vehicle body. Blueprint child `WBP_VehicleInfoTakeover` holds `M_Style`; its skin is copied from `BP_W_UnitVariantSelectionMenu`. |
| `UPauseSafeProtoRichTextBlock` | Proto's typewriter driven on real time. Proto uses world timers, which stop while the game is paused. |
| `UInfoTakeoverSubsystem` | Queue, setting check, library lookup. |
| `UInfoTakeoverSettings` | Project Settings > Game > Info Takeovers: widget class and library. |
| `VehicleInfoTakeoverContent` | Reads name, picture, weapons and armour off the live vehicle. |

The Proto text styles are `DT_InfoTakeoverTitleText` and `DT_InfoTakeoverBodyText`. Both are copies of
`DT_AnimatedRichText` with the animated font material.

### Adding buildings or squads later

1. Add a row struct.
2. Add a `UW_<Kind>InfoTakeover` subclass that fills `GetBodyColumn()`.
3. Add a content builder.
4. Add a queue branch in `UInfoTakeoverSubsystem::CreateTakeoverWidget` (keyed by `EInfoTakeoverKind`).
5. Add a mission node.

The pause, typewriter, sounds and the shared "never show" switch come from the base.

## Validation

Run with `-InfoTakeoverValidate`, best with `-game -RenderOffscreen`. Grep the log for `INFO_TAKEOVER`. The run:

- Spawns every row's reference vehicle and logs the content.
- Opens all rows, plus a very long and a very short custom takeover.
- Screenshots the title, the typing and the finished state to `Saved/InfoTakeover/<height>p/`.
- Ticks "never show again" on the last takeover and checks that the setting turned off and that the settings menu
  follows it.
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
- Checks that On Closed fires at once while the takeovers are off.
- Checks the queue-drop path: ticking "never show again" on a takeover releases a second one waiting behind it.
- Restores the setting and quits.

Run with `-CoverTakeoverValidate` for the cover takeover. Grep the log for `COVER_TAKEOVER`. The run:

- Calls the node before the start button and checks that nothing opens until start is pressed.
- Checks that the three preview screenshots loaded.
- Screenshots the typing, the finished panel and the HUD after closing to `Saved/InfoTakeover/Cover/<height>p/`.
- Presses UNDERSTOOD and checks that On Closed fires once, with the game unpaused.
- Checks that On Closed fires at once while the takeovers are off.

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
Closed once and unpaused, at once while turned off), and also:
- checks that the three slot legend pictures loaded;
- then restarts the loop at each beat (0.4, 2, 3, 4.6, 5.6, 7.8, 9.3, 10.6 s) for a still, `BxpPackUp_beat_<s>s`.
  The shot is taken three frames after the jump, so the widgets show the new time.

Screenshots go to `Saved/InfoTakeover/BxpPackUp/<height>p/`. Result on 2026-11-12, DebugGame, `RTS_ToCopySmall`, 1080p
and 720p: PASS.
