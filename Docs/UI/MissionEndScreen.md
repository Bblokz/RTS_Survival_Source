# Mission end screen (debrief)

When a mission is won or lost, `AMissionManager::TriggerVictory` / `TriggerDefeat` pause and lock the game as before,
hide the HUD and the mission list (`OnCinematicTakeOver(true)`), and show the end screen:

- **`AMissionDebriefStage`** (`Missions/MissionDebrief/`) is spawned high above the map (Z 250000) inside its own closed
  black room and takes the player's view. A steel seal lifts out of its ink tin and stamps the operational order on a
  worn field desk (the game's MP 40 and StG 44, rounds, desk lamp, wall map). It tips up towards the camera and spins;
  the face is swapped for a readable one while it points away, so VICTORY / DEFEAT reads correctly. On defeat the order
  then burns in from the edges down to the stamped letters, whose edges keep burning (flames, embers, smoke).
- **`UW_MissionEnd`** (pure C++ widget) fades in from black and, when the stage fires `OnButtonsDue`, raises the outcome
  line and its buttons: Continue and (optionally) Main Menu on victory, Restart and Quit on defeat. The defeat line comes
  from `ERTSDefeatType` (headquarters destroyed / command vehicle lost / main objective failed).

The old `UW_Victory` / `UW_Defeat` classes and `BP_W_Victory` / `BP_W_Defeat` are gone; `AMissionManager` now has
`M_MissionEndWidgetClass` (defaults to `UW_MissionEnd`) and `M_DebriefStageClass` (defaults to `AMissionDebriefStage`).
Without a stage (class cleared or assets missing) the buttons show at once over the paused game.

## How it runs while paused

The world is truly paused, so the stage ticks when paused and runs its own timeline on real time: stamp keyframes,
camera, paper material parameters (ink, ink drying, burn), lights, and the burn's flames/embers/smoke, which are CPU
particles drawn as camera-facing quads on three instanced meshes with RGBA custom data (Niagara does not tick while the
world is paused). The flames are placed on the burning edge with a CPU copy of the paper's burn field
(`DA_Debrief_BurnField`). While the debrief shows, the game viewport's post process materials are switched off (the fog
of war would black out the stage, which lies outside the map); `EndPlay` turns them back on.

## Assets and pipeline

Everything comes from the approved browser mock-up (`Source/Tools/MissionDebrief/steel_seal_mockup.html`):

1. `make_export_page.py` + `export_boot.js` turn the mock-up into an export page; `run_export.py` (Playwright + Chrome)
   writes the meshes as glTF (displacement baked into vertices), the paper/ink/burn textures as PNG and `timeline.json`.
2. `import_debrief_assets.py` (headless, `-AllowCommandletRendering`) imports them with Interchange into
   `/Game/RTS_Survival/Blueprints/Missions/MissionDebrief/` (one combined mesh per file); `DEBRIEF_ONLY=paper,...` limits it.
3. `disable_debrief_nanite.py` turns Nanite off on the imported meshes (lighting channel, translucent pieces).
4. `build_debrief_materials.py` builds `M_Debrief_Paper` (ink + burn in one Custom node), soot, sprite and StG 44 PBR
   materials; rerun it after re-importing the paper (it holds the paper texture).
5. `setup_debrief_assets.py` makes the sprite material instances and `DA_Debrief_BurnField`.

Layout and timing numbers in `MissionDebriefStage.cpp` are the mock-up's (stage units: 1 unit = 10 cm, x right, y up,
z towards the camera; Unreal gets X = x, Y = z, Z = y).

## Look-dev and tests

- Stage defaults (`ExposureBias`, light intensities, ink bump) are `EditDefaultsOnly` on the stage class.
- Console: `RTS.Debrief.LightScale`, `RTS.Debrief.DefaultChannels`, `RTS.Debrief.KeepLevelExposure`.
- `-MissionDebriefValidate` (add `-MissionDebriefDefeat`) on a unit test map ends the mission, screenshots the debrief to
  `Saved/MissionDebrief/<outcome>/` and checks the stage runs while paused, takes the view and shows the buttons.
  Runner: `Source/Tools/MissionDebrief/run_debrief_check.sh [victory|defeat] [WxH]`.
