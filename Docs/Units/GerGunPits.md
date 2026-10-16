# German gun pits (Pak 40, leFH 18, sFH 18, Flak 37)

Four player-built defences of the German faction. Each is a sandbagged pit holding a team-weapon gun on a plank deck, with four crew. The gun and its crew turn together to face any direction. A shell handler passes every round to the loader, then fetches the next one from the ready crate.

They are built from the German HQ's defence options, after the captured B-4 pit. The B-4 pit was the model; its story is in [B4Howitzer/B4Pit_LessonsLearned.md](B4Howitzer/B4Pit_LessonsLearned.md) (section 8 covers what these pits added).

| Pit | Expansion type | Gun / weapon | Pit | Cost (radixite / metal) | Footprint |
|---|---|---|---|---|---|
| Pak 40 Pit | `BTX_GerPak40Pit` | `Pak40_3_L46_75MM_TW`, direct fire | low ring (3 courses), inner radius 400, fires over the rim | 380 / 200 | 1000 |
| leFH 18 Pit | `BTX_GerLeFH18Pit` | `LeFH_18_105MM_TW`, artillery | high ring (5 courses), inner radius 520, posts and camo canvas | 620 / 320 | 1400 |
| sFH 18 Pit | `BTX_GerSFH18Pit` | `sFH18_150MM_TW`, artillery | as the leFH pit | 950 / 520 | 1400 |
| Flak 37 Pit | `BTX_GerFlak37Pit` | `Flak36_37MM_Pit`: anti-aircraft and ground, 6-round clips, 4.5 s clip change | ring like the Pak's (4 courses), inner radius 430, camo drapes | 420 / 220 | 1100 |

Game data (build time, vision, health ×0.9 / 1.1 / 1.25 / 0.9 of the heavy bunker, resistances, abilities) lives in `BxpHelpers::GerBxpData::InitGerGunPitBxpData` ([BxpDataHelpers.h](../../RTS_Survival/Game/GameState/BxpDataHelpers/BxpDataHelpers.h)). Costs are in `DeveloperSettings::GameBalance::UnitCosts::Bxps`.

## Assets

All under `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/`:

| Folder | Content |
|---|---|
| (root) | `Ger_BXP_GerPit_{Pak40,LeFH18,SFH18,Flak37}`: copies of the German leFH expansion (`Childs/LeFH105/RTS_BP_BXP_LeFH105mm`) |
| `Turrets` | `RTS_BP_EmbTurret_GerPit_*`: the child turret with gun, crew, crate and rounds |
| `Anims` | `AnimBp_GerPit_*` (parent `UGunPitAnimInstance`) and the guns' fire montages |
| `Crew` | handler drill and idle animations, crew montages |
| `Meshes` | `SM_GerPit_Pak`, `SM_GerPit_Howitzer`, `SM_GerPit_Flak`, rounds `SM_GerPit_Shell_75/105/150` and the clip `SM_GerPit_Clip_37` |
| `UI` | option pictures and slate brushes (`DT_Items` rows of the same names as the types) |

The pits use three materials:
- `M_GerCamoConcretePalet_Atlas` for the deck, posts and camo canvas drapes. Every deck board shows a window of the same weathered board, with a thin dark seam along one edge.
- `M_GerSandBagScaffold_Atlas`, the material of every German sandbag wall in the game. The bags are the loose sandbag meshes with their own UVs.
- `M_Ger_AmmoBox` for the crates and rounds.

## How a pit works

- **Expansion:** the `Howitzer` child actor component holds the turret, raised 4 cm onto the deck. The gun is hidden while the pit is built, as on every BXP.
- **Gun mesh:** the turret's mesh is the team weapon's own skeletal mesh. Its anim blueprint plays the deployed pose, then the `DefaultSlot` (fire montage), then two additive component-space bone rotations:
  - pitch on the elevation bone (`B_Mantlet`, `Cannon_Elevate` on the sFH);
  - yaw on the **root bone**. The whole gun turns, and so do the crew attached to it.
- **`PitOrigin`:** a socket on each gun's root bone that cancels the bone's import scale and rotation (leFH ×71.66, sFH ×100 and yaw −90, Pak rotated). The crew, crate and rounds hang on it at 1:1 scale, in plain gun space (+X = muzzle).
- **The sFH mesh had no fire sockets.** `BulletLaunch0` and `BulletOut0` were added on `SFH18_Gun`; its team weapon uses them too.
- **Crew** (`UGunPitCrewComponent`, components tagged `PitCrew`, kept at their Blueprint offset from `PitOrigin`):

| Crew | Pak 40 | leFH / sFH | Animation |
|---|---|---|---|
| Crew1 loader | (−148.6, 76) | (−234.5, 39.7) | crouched idle; the team weapon squads' own loading animation (`AN_Infantry_LoadPAK` / `AN_Infantry_LoadHowitzer`) |
| Crew2 handler | at `PitOrigin` (drill carries his spot) | same | baked drill `Anim_GerPit{Pak,Howitzer}_Handler_{Idle,Reload}` |
| Crew3 spotter | (−163.4, −67.5) | (−241.3, −54.4) | `SpotPAK_Crouched` / `SpotHowitzer_Standing` |
| Crew4 commander | (−300, −150) | (−430, −230) | `CommandoCrouched` / `Commando` |

  The loader, spotter and commander stand at the team weapon's crew positions, with mesh yaw −90 so they face the muzzle (as a squad soldier's mesh does).
- **Drill:** every reload stretches the crew montages to the reload time minus 0.25 s. The rounds (`PitRound`, two of them) change hands at fixed fractions of the drill, taken from the Blender bake (`gerpit_*_carry.json`):

| | Handover to loader | Handler picks next round | Round rammed (hidden) |
|---|---|---|---|
| Pak 40 | 0.218 | 0.510 | 0.797 |
| leFH / sFH | 0.210 | 0.476 | 0.675 |

  A round is attached with KeepWorld to the hand that takes it, so it never jumps. Reload times are 5.9 s (Pak), 5 s (leFH) and 14.4 s (sFH), so the drill plays at about 1.4×, 1.9× and 0.64×. The B-4 pit uses the same component (`UB4HowitzerPitCrewComponent` is a thin subclass).
- **Ready crate** (`ReadyCrate`, `SM_TankAmmoCrates_A`): it lies under the round the handler picks up, at the drill's crate spot (`CrateRound`).
  - Scaled ×1.173, so its top is at the drill's 44 cm and the round lies on it. It is then 51 × 69 cm.
  - Its long side runs along the round and its narrow side faces the handler. The near edge is 20 cm in front of his feet (he picks up from 46 cm behind the round).
  - The crate turns with the gun, so its far corner must stay inside the wall: 389 cm in the Pak pit (ring 400), 509 cm in the howitzer pits (ring 520; the posts' inner face is at 508, so it may just touch one) and 355 cm in the Flak pit (ring 430).
  - [pit_crate.py](../../Tools/GunPits/pit_crate.py) places it. It reads the mesh bounds through the bridge, because a mesh pivot is seldom the box centre.
  - **Fixed 2026-10-15.** The pits used `SM_Ger_Ammobox` at half size, and the scripts assumed the wrong centre for it. The crate sat 65 cm off its spot, so the handler picked the next round out of the air beside it. That box (83 × 109 cm) also cannot cover the round inside the Pak pit's 4 m ring, so the pits now use the smaller crate of the 53-K sector pit.

## The Flak 37 pit

The 3.7 cm Flak 37 team weapon gun (`Squads/TeamWeapons/Flak37mm/Mesh/SK_TW_Flak37MM`) is the fourth gun. Its turret `RTS_BP_EmbTurret_GerPit_Flak37` is built from the existing 37 mm Flak turret (`Turrets/EmbeddedTurrets/37mmFlak`), so it engages aircraft as well as ground targets:
- traverse 45°/s;
- pitch −5° to +85°;
- fires from the team weapon mesh's `Launch` and `Out` sockets.

**Gun:**
- The deployed pose `Anim_TW_Flak37_Base` is additive on `Anim_TW_Flak37_TPose`, as on the leFH. The fire animation is the deployed pose with 28 cm of barrel recoil.
- `PitOrigin` sits on the root bone `37mmFlak` at scale 1.25, which cancels the root's 0.8 import scale.

**Crew:**

| Crew | Spot | Animation |
|---|---|---|
| Crew1 loader | (−97.9, 85.1), the team weapon's loader spot | idle `Anim_GerPitFlak_Loader_Idle` (the last second of his animation); reload `AN_Infantry_Flak_Load_Standing` |
| Crew2 handler | at `PitOrigin` (drill carries his spot) | baked drill `Anim_GerPitFlak_Handler_{Idle,Reload}` |
| Crew3 spotter | (−240, −90), behind the left leg | `AN_Infantry_Flak_CrouchedScan` |
| Crew4 commander | (−320, 60), at the back | `AN_Infantry_Commando` |

**The loader's animation stands him on the gun's platform beside the feed.** His feet are 58 cm up and his hands meet at 174 cm. So:
- the handler reaches each clip up to him from the deck, from (−80, 150);
- the next clip comes from a crate behind, at (−230, 210), clear of the right leg;
- the loader's idle is cut from the rest pose that ends his animation, so the drill starts without a pop.

**The clip.** The rounds that change hands are `SM_GerPit_Clip_37`: six rounds side by side on a strip, held by its ends.

**Drill timing:**

| | Handover to loader | Clip pushed into the feed (hidden) | Handler picks next clip |
|---|---|---|---|
| Flak 37 | 0.125 | 0.311 | 0.403 |

The loader pushes the clip in before the handler reaches the crate. `UGunPitCrewComponent` therefore allows the pick after the ram: the pick then takes the clip now in flight.

**Fire feedback.** A light, quick kick on `B_Root`: 2.2 cm at 9 Hz. The kicks of a burst add up. The crew brace at half strength (`CrewFireFlinchStrength` 0.5) and hold the brace through the burst; see [GunPitFireFeedback.md](GunPitFireFeedback.md).

**Rebuilding the Flak pit:**
1. Export the sources (commandlet, `-AllowCommandletRendering`): `Tools/GerPits/ue_flakpit_export_sources.py`.
2. Blender:
   - `gerpit_build.py -- <Exports> SM_GerPit_Flak,SM_GerPit_Clip_37`;
   - `gerpit_crew.py -- <Source> <Exports> flak`;
   - `gerpit_icons.py -- <Exports/Icons> Flak37` (Blender 5).
3. Unreal:
   - `ue_flakpit_import.py`, then add the `PitOrigin` socket with the printed scale;
   - `build_gerpit_animbps.py guns Flak37` and `build_gerpit_animbps.py crew`;
   - `build_gerpit_turrets.py Flak37`;
   - `build_gerpit_bxps.py Flak37`;
   - `GunPits/build_fire_feedback.py guns,flak Flak37`;
   - `ue_flakpit_register.py` (spawner) and `ue_gerpit_icons.py` (build menu).

## Rebuilding

Scripts, in the order they run. The Blender ones are in `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Scripts`; the UE ones are in [Tools/GerPits](../../Tools/GerPits) and run through the editor bridge.

1. `ue_gerpit_export_sources.py` (commandlet, `-AllowCommandletRendering`): gun, soldier and animation FBXs into `GerDefenses/Source`.
2. `gerpit_build.py` (Blender 5): pit meshes and rounds into `GerDefenses/Exports`.
3. `gerpit_crew.py` (Blender 4.0, uses `b4_pit_crew`): handler drills plus the carry JSONs into `Exports/Crew`.
4. `ue_gerpit_import.py`: meshes and handler animations.
5. `build_gerpit_animbps.py`: anim blueprints and montages.
6. `build_gerpit_turrets.py`: turrets. Reads the carry JSONs for the drill fractions and round poses. `build_gerpit_turrets.py Pak40,LeFH18,SFH18,Flak37 crate` only re-places the ready crates. It keeps later edits, such as the fire feedback.
7. `build_gerpit_bxps.py`: expansions. The spawner entries in `RTS_BP_RTSAsyncSpawner` were added by hand-run Python.
8. `gerpit_icons.py` (Blender 5), then `ue_gerpit_icons.py`: option pictures, brushes and `DT_Items` rows.

## Testing

`-GerPitValidate` on TestCover spawns the three pits as the player's and orders each to attack the ground 60° off its forward. It then checks that:
- the crew, crate and rounds sit on `PitOrigin` at scale 1;
- the gun turns to the target and the loader stays at his spot in gun space;
- the gun fires twice;
- one drill passes the round handler → loader → breech while the handler picks the next one, with each hand within 70 cm of the round's centre.

```bash
MSYS_NO_PATHCONV=1 UnrealEditor-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/TestCover/TestCover -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -benchmark -fps=30 -DisablePlugins=Fab -GerPitValidate -GerPitCapture=<folder> -CaptureExposureBias=1 -ExecCmds="r.TextureStreaming 0"
```

Grep the log for `RTS_GERPIT_TEST`. With `-GerPitCapture` it saves four views per pit: idle, every second of the first drill, and at the handover, pick and ram. Turn texture streaming off for pictures, or the atlas shows its lowest mips (flat dark bags, blurred planks). The scenario is in [GunPitTestScenario.cpp](../../RTS_Survival/Buildings/BuildingExpansion/GunPit/Tests/GunPitTestScenario.cpp).

## Not done

- The pits keep the leFH expansion's death behaviour (no pit-specific wrecks).
- The enemy AI does not build them (no `SatteliteBuildingsForBase` entry).

## Fire feedback

On every shot the gun bucks and the crew brace. See [GunPitFireFeedback.md](GunPitFireFeedback.md).

## Rail Pak bunker

The breakthrough doctrine's concrete pillbox with a rail Pak 40 in this pit on its roof: see [GerRailPakBunker.md](GerRailPakBunker.md).
