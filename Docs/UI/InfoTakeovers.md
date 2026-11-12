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
