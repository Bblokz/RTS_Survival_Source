# Team weapons shorts

Select **Team weapons** in Shorts Studio and choose `TeamWeapons_Sdkfz_Pak40_Flak88_vs_T3476_SU76` with the `TeamWeaponsBattle` setup. Each half-track has a crew selector. Vehicle armies, camera settings, battle pacing and captions use the existing Studio controls. Keep the transport list and crew list identical across the two shots.

The first preset uses four tow-capable Sd.Kfz. 250 half-tracks, three PaK 40 crews and one wheeled FlaK 88 crew against three regular T-34-76s and one regular SU-76. The armed Sd.Kfz. 251/22 blueprint has no tow component and is unsuitable for this preset.

## Capture contract

- `TeamWeapons.Shot = TowConvoy`: one `SquadClassPaths` entry per vehicle, ordered as the lead followed by `BattleAllyClassPaths`. Native squad spawning creates each crew and its gun. During recorded warm-up, the director invokes the actual vehicle tow command, which loads the crew and attaches the packed weapon to the tow socket. All four hooks must complete before movement begins.
- Exactly two seconds of the moving convoy are exported. `CutLeadFrames` stays zero. The ten-second preparation slot is discarded by the existing post tool.
- `TeamWeapons.Shot = DeployBattle`: the cut uses fresh instances at the industrial stage. Each crew spawns beside its corresponding half-track at `DeploymentOffset`. The squad's native spawn/deploy state machine plays its deployment animation. Attack commands use the nearest opposing vehicle once the crew has left its spawning state. The battle director waits for the crew's deployed state before issuing subsequent target orders. Crew tracking continues independently if the transport is destroyed during deployment. No deployment montage or gun attachment is faked.
- The preset allows up to 118 seconds of battle. With the two-second convoy, total maximum duration is 120 seconds. The existing battle-ending logic may finish earlier.
- Special-round vehicle captions continue through the shared `forge_rounds` annotation in `run_shorts.py`.

## Stage

`/Game/RTS_Survival/Maps/Shorts/RTS_Shorts_TeamWeapons` is a dedicated copy of the shorts map. Its added actors use `ShortsOnly_TeamWeapons`; original short sets remain unchanged.

The route uses the paintable desert landscape material with Sand, AridSoil, DesertGravel, DesertRockDark, ScorchSandyStone and Weedy layers. Cacti, desert foliage, dead trees and destroyed vehicles border its clear driving lanes. Its `GroundNoBuilding` collision profile blocks the custom Landscape trace channel used by tracked vehicle physics; ordinary visibility collision alone leaves the transports stationary. The industrial yard uses `BP_Airport_Ground1`, whose `SM_Ground_1` tiles already use `M_RTSGround_WithSand`, plus rusty destructible hangars, fences, airport barrels and supply-crate stacks.

`build_stage.py` authors this dedicated map from the original and saves baked navigation. It refuses to regenerate a previously dressed stage. Run it in Unreal's editor Python environment; `EditorPythonScripting.set_keep_python_script_alive(True)` allows its save callback to complete. The source map and shared Blueprints/materials are not modified.

## Verification

Run `python Source/Tools/TeamWeaponShorts/test_team_weapons.py` and `python ShortsCreator/Tools/test_dashboard.py`. Capture with `python ShortsCreator/Tools/run_shorts.py --job ShortsCreator/Jobs/TeamWeapons_Sdkfz_Pak40_Flak88_vs_T3476_SU76.json --post --keep 0`.

Before accepting a take, check the native log for four `TEAM_WEAPON_TOWED` entries, attached weapons moving during the exported intro, four `TEAM_WEAPON_DEPLOYED` entries, the requested enemy roster with owner 2, and `RTS_SHORTS_JOB_DONE ... success=true`. Run `python Source/Tools/TeamWeaponShorts/audit_render.py <take-folder>` to verify the completed take, then inspect both shots visually. Packed gun collision and distance optimization are disabled only on this short's cinematic instances; towing, vehicle movement and deployment continue through the native gameplay systems.

The engine can report its existing AudioStreamingCache loads-in-flight assertion during process shutdown after RTS_SHORTS_JOB_DONE success=true. The completed take must still pass the manifest, native-motion/deployment and MP4/audio audit before accepting it. The accepted first take is take_06: 45.233 seconds at 1080 x 1920 / 60 fps, stereo, measured -14.23 LUFS.

