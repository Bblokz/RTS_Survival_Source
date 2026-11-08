# Commander ability preview

On the commander selection screen every kit ability is its own button: a pane of the faction buttons' glass
(`MI_FactionButton_Glass` on the shared `M_UI_GlassPane`) lights up and sweeps on hover, a small PREVIEW label
brightens, and a click cuts the camera to a desert next to the hangar, where the ability is executed in the middle of
the view. Only the ability card (icon, title, tier, explanation) stays at the top of the screen and a BACK button at
the bottom; the commander screen is hidden behind them. Combat music plays during the preview; the transport loop of
the commander screen comes back with the hangar. When the ability is done (units landed, bombs down, area strafed) the
preview waits six seconds, counting down under the explanation, and returns to the hangar by itself.

## Code

`Source/RTS_Survival/FactionSystem/FactionSelection/AbilityPreview/`

- `AAbilityPreviewDirector` runs a preview. It copies the template from `DA_GlobalAbilitySystem` as the mission
  manager does, owns a `UGlobalAbilitiesManager` in preview mode (`SetupForAbilityPreview`: no panel, resources,
  requirements, cooldowns or aim; aircraft come in from `M_AircraftApproachDirection` at `M_AircraftSpawnDistance`) and
  calls `ExecuteAbilityAtLocation` on the ground under its actor, or `ExecuteAbilityAlongLine` across the view for
  strip abilities. It listens to `OnGlobalAbilityEnded`; instant abilities (the spies) count as finished after
  `M_SecondsUntilInstantAbilityFinished`, and `M_MaxPreviewSeconds` (90 s) is the safety net. Strafing copies strafe at
  most `M_MaxPreviewStrafeSeconds` (25 s; the templates strafe 120 s).
- While a preview runs it makes the sun the atmosphere light and thins and warms the height fog; both are put back on
  stop. `bM_SpawnSkyAtmosphere` (off) would also spawn a sky atmosphere: the camera looks down and shows no sky, and a
  runtime atmosphere without baked data counts as one unbuilt object next to the hangar's stationary sky light
  ("LIGHTING NEEDS TO BE REBUILT" in development builds).
- Every actor spawned during a preview within `M_CleanupRadius` of the centre (and every aircraft) is destroyed on
  stop; spawns of a stopped preview that arrive later (async loads) are destroyed on arrival. Pools, managers and
  projectiles are never touched (`KeptClassNameParts`).
- `UW_AbilityPreviewOverlay` is the preview's whole UI (Escape, Backspace and gamepad B go back too).
- `AFactionPlayerController::HandleAbilityPreviewRequested` / `HandleAbilityPreviewBack` swap the view target (hard cut
  with `SetGameCameraCutThisFrame`), hide and restore the commander screen, focus and music.
- `UW_CommanderAbilityRow` is the kit row button; `UW_CommanderSelectionMenu::PreviewAbilitySlot` opens a preview
  from code.

Shared code got only additive changes: `UGlobalAbilitiesManager::SetupForAbilityPreview`, a silent
`UGlobalAbility::EnsureIsValidPlayerController` for preview managers, `FRTS_Statics::GetIsInPreGameHangar` (the
"no ACPPController" reports of selectables, turrets, infantry weapons, the optimizer and command data stay quiet in the
hangar) and an async spawner fallback to the hangar controller's spawner in `FRTS_Statics::GetAsyncSpawner`.

## The desert

`Source/Tools/AbilityPreview/ability_preview_desert.py` builds it in `RTS_FactionSelection` at (200000, 0): a 441 m
landscape on `MI_RTS_Landscape_DesertPaintable` with five paint layers (Sand, DesertGravel, DesertRockDark,
ScorchSandyStone, Weedy), sculpted dunes, a rocky ridge behind the view and two mesas, a pond with a weedy, darker
shore and `M_SM_OasisWater`, red and orange stone decals (`FactionSelection/AbilityPreview/MI_AP_Decal_*`, tinted
children of the stone debris decals), dead trees, cacti, cactus bushes, rocks and dead shrubs, a nav bounds volume and
the placed `AbilityPreview_Director` (its camera component frames the view). The desert is lit dynamically, so its
primitives cast no static shadows (`ABILITY_PREVIEW_PATCH_SHADOWS=1` reapplies only that). Everything is labelled
`AbilityPreview_`; the script replaces it on a rerun. Run it in a throwaway full editor (commandlets crash saving
landscape maps); it rebuilds the navmesh, waits for it and saves.

## Screen sizes

The project scales UI by the shortest screen side (`UIScaleRule=ShortestSide`, 1080 = 1.0), so every screen is
1080 units high and a 4:3 screen such as 1920x1440 is only 1440 units wide. Without help the commander screen's
1920-unit layout overlapped there (the description plate covered both side columns).

`MenuLayout/FMenuDesignFrame` fixes this for the commander screen and the preview overlay: they build into its
canvas, which is never narrower than 1920 units. On narrower screens it is scaled down to fit and gets the extra
height. The commander screen hangs its header, side panels, portrait and description from the vertical centre, so the
extra height is shared above and below them. BACK and CONFIRM stay in the bottom corners. The overlay keeps its plate
at the top and BACK at the bottom. At 16:9 and wider nothing moves. The side panel art (`MI9_..._392x848`,
`_480x848`) is drawn for its height, so the panels keep that height rather than stretching.

Kit rows: `MI_FactionButton_Glass` draws its rim about 13 units in from the sides and 10 from the top and bottom, so
the row content sits 10 units inside that.

Check a resolution offscreen (the monitors are 1080 high):
`UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/FactionSelection/RTS_FactionSelection -game -RenderOffscreen
-ResX=1920 -ResY=1440 -ForceRes -AbilityPreviewTest -AbilityPreviewTestOnly=GA_2xBf110Strafing`.
Verified 2026-11-08 at 1920x1080, 1920x1440 and 2560x1080.

## Targets

Attack aircraft get Soviet units (player 2) under their run, so the strafing and bombing have something to hit
(`AbilityPreviewTargets.h`, `UAbilityPreviewTargetsComponent` on the director). They spawn when the preview starts,
are tracked spawns (cleaned up with the rest) and are listed per ability in `M_AbilityTargetKinds`:

| Kind | Units | Abilities |
|---|---|---|
| Infantry | 5 squads (Mosin, SVT) | Bf 110 x2, Bf 109 (machine guns), incendiary Ju 87 ("deadly to infantry, harmless to tanks") |
| Light vehicles | T-70, BA-11, T-26 | Horten 229 (cannons), Ju 87 G (37 mm pods) |
| Medium vehicles | T-34/76, T-34/85, T-34/76 | Ju 87, Ju 87 x3, Me 410 (bombs), Ju 390 carpet |

Unlisted abilities fall back by class: carpet bombing gets medium vehicles, strafing and incendiary get infantry.

- Placement: strafing runs all start at the target (later runs head wherever the aircraft comes from), so the
  ability executes at the centre and the targets stand from 0.2 run before it to 0.55 run along the first run.
  Carpet bombing executes half a carpet before the centre. Strip abilities use the strip.
- Aircraft spawn along a 2.4 km line, not half the map (`M_AircraftSpawnLineHalfLength`,
  `UGlobalAbilitiesManager::SetupForAbilityPreview`), so every run follows the approach line.
- Carpet runs (fixed for missions too, 2026-11-08): `UGA_AircraftBombing::BuildCarpetEndLocation` measured the end
  from the bomber's spawn, so the bomber turned over the target and bombed back toward its spawn edge. Now the
  carpet runs straight on past the target for its length. `-CarpetBombingValidate` on UT_Nomadics checks it with the
  player's own manager: bombs past the target, none back toward the spawn, T-34s on the run hit.
- The targets keep out of the fog, hide their health bars and soldiers take the squad's owner
  (`FRTSUnitOwnerSpawning`). `ASquadController::SetAutomaticCoverBlocked` keeps the squads from walking to the
  pond's rocks for cover.
- Avoided units: the ZIS-5 truck (convoy prop with no unit subtype, logs errors) and the large mixed squad (more
  soldiers than its spawn grid).
- Machine gun strafing barely hurts infantry, in missions too: the MG 15's traces are about 32 m long and the
  fighters fire a burst at the start of each dive, about 30 m out and 25 m up, so most traces end in the air. The
  test logs hits for the two machine-gun-only abilities but doesn't require them.
- Test: `targets_match_ability`, `targets_spawned`, and `targets_hit` (the most damage seen during the case).
  `-AbilityPreviewTestBurst` takes a screenshot every 0.4 s.

## Ability endings

The return countdown (6 s) starts when the ability has visibly finished, which is not always when it reports its
end:
- **Nuke and V2** (`UGA_RadixiteNuke`) report their end at launch. The director waits until every `ARadixiteNuke` it
  spawned has detonated (reason `blast`).
- **Glider drops** (`UGA_GliderTankDrop`, 10 abilities) report their end when the tank drives out of the nose. The
  director waits until the tank is 12 m from the glider (or 6 s), hides the glider (no collision), pans the camera
  right over 2 s until the tank's stopping point (`AGliderTankDelivery::GetTankExitLocation`) is centred, and starts
  the countdown after the pan (reason `glider_pan`). The camera goes back when the preview stops.
- The test checks `warhead_landed_before_finish` and `glider_hidden_before_finish`.
- Burst screenshots slow an offscreen run to about 5 fps, and preview durations are measured in real time, so don't
  judge durations or countdowns from a burst run.

## Test

`-AbilityPreviewTest` on `RTS_FactionSelection` (non-shipping): walks to the commander screen, hovers a kit row, then
clicks every kit ability's row and checks the cut, the overlay, the combat music, the auto return six seconds after the
end (or BACK with `-AbilityPreviewTestManualBack`), the camera and music back in the hangar, leftovers and logged
errors. `-AbilityPreviewTestOnly=GA_A+GA_B` limits it; `-AbilityPreviewTestRepeat` runs every case twice in a row.
Screenshots and `results.txt` go to `Saved/AbilityPreviewTest`. Errors of known Blueprint content gaps of the dropped
units (they fire the same way in missions) are counted apart in the summary, not as failures.
