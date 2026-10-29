# Horten Ho 229

German flying-wing jet, trained at the German airbase (`EAircraftSubtype::Aircraft_Horten229`; cost, health and the
training slot already existed in C++). It is the first jet in the game: the two jets are its "engines", and it takes off
vertically on four lift fans in the wings like the other aircraft do on their wing rotors.

## Weapons

Two wing-root **MK 108 30 mm** cannons, one weapon state that fires both at once (`UWeaponStateMultiProjectile`, the
Blueprint variable `HortenGunSockets` = `B_Gun_L`, `B_Gun_R`). Weapon entry `EWeaponName::MK108_30MM_Horten` (228),
data next to the MG 151 in `ACPPGameState::InitAllGameLightWeapons`:

| | MK 108 (per gun) | MG 151 for comparison |
|---|---|---|
| Calibre | 30 mm, 25 g TNT | 20 mm, 10 g |
| Damage (stored) | 81 | 42 |
| Cooldown | 0.45 s | 0.2 s |
| Magazine / reload | 60 / 6 s | 600 / 6 s |
| Pen (close / max range) | 32 / 20 | 24 / 14 |
| Accuracy | 55 | 60 |
| Shell speed | 0.75 x base | 1.0 x base |

AP is loaded by default, HE can be loaded. All numbers are a first balance pass. Bursts in the Blueprint: 3-6 rounds.

## Assets

`/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/HO229/`

- `Ger_Aircraft_Horten229` (parked, trained, the spawner row) and `Ger_Aircraft_Horten229_Airborne` (starts in the
  air), copies of the Me 410 Blueprints without bombs. Max speed 4750, acceleration 650 (Me 410: 4250 / 550); VTO
  settings as the Me 410.
- `AnimBp_Ho229`: `AnimBp_Bf109` retargeted; every animation is set in its class defaults. Sounds are the Me 410's.
- `Mesh/SK_Ho229` (+ skeleton, physics asset), `Mesh/Anims/` (sequences, `AO_Ho229_Jet`, four montages; the hover
  montage loops), `Materials/` (own copies of the Fab textures: `/Game/Fab` is not in git). The `Anim_Ho229_Jet_*`
  aim-offset samples use the skeleton reference pose as additive base: with the one-frame T-pose as base
  (`ABPT_AnimFrame`, as the Bf 109 does) every bone came out applied twice at runtime and the jets sat metres off.
- `CrashCollections/GC_Crash_SK_Ho229` (built with `RTS.Aircraft.BuildCrashCollections Horten Overwrite`).
- Training: `DT_TrainingOptions` row `Train_Aircraft_Horten229`, icon `T_Ger_Aircraft_Horten229(_Brush)` (a Blender
  render, source `D:/UE_Assets/Icons/TrainingUI/Aircraft/T_Ger_Aircraft_Horten229.png`), spawner row in
  `RTS_BP_RTSAsyncSpawner` `SetupGerAircraft`, airbase slot 4 in `DA_TrainingLayout` (both German factions) and in
  `UTrainingLayoutDataAsset::MakeDefaultAirbaseLayout`.

## Rig

Built from the Fab static mesh by `D:/Blender/BlenderProjects/BlenderTanks/AA_LaptopWork/GerAircraft/HO229/build_ho229.py`
(see the README there). Nose on +X, left wing on UE -Y.

- `B_RMaster` > `B_Rotor1/2`: the intake compressor faces, spun by the prop aim offset. Two `B_Rotor` bones make the
  death component burn a wing engine.
- `B_WRMaster` > `B_WingRotor_L1/L2/R1/R2` (lift fans) and `B_WheelsMaster` > `B_NoseWheel`, `B_LeftWheel`,
  `B_RightWheel`. The gear sits under the wing-rotor branch, so the idle/flying sequences and the VTO montages (which
  only drive that branch in the AnimBP) also fold it: gear down on the ground, up in flight. Retracting gear swings in
  and then shrinks to 1 % inside the wing.
- `B_HorizontalStabilizer_L1/L2/R1/R2`: the four elevons, driven by the stabilizer sequences.
- `B_Gun_L/R`: muzzles (bones double as sockets).

Take-off and landing animations are 20 s: take-off plays at its own speed until the aircraft is airborne (about 16 s
for 18.5 m), landing is rescaled by `UAircraftAnimInstance::OnStartLanding` using the take-off montage's length.

## Strafing global ability

`EGlobalAbility::GA_2xHortentrafing` (the enum's spelling): template `GA_AircraftStrafing_Horten229` in
`DA_GlobalAbilitySystem`, two `Ger_Aircraft_Horten229_Airborne`, icon `T_Ger_Aircraft_Horten229`. In the
`BalancedCommander` loadout (`Config/DefaultGame.ini`, `[/Script/RTS_Survival.CommanderSettings]`) after the Ju 87
strafe. Costs, cooldown (5 s), requirement (`Tech_HarvesterSpeed`), aim (3000) and the strafe pattern are copied from
`GA_3xJu87Strafing`, so both are balanced alike; tune both together. The Bf 109 and Pe-2 strafing enums still have no
template.

## Tests

- `RTS.Weapons.Aircraft.HortenMk108WeaponData`, `RTS.Units.Aircraft.HortenAircraftData` (`HortenWeaponTests.cpp`).
- `-AircraftVtoValidate` (new, `AircraftVtoTest.cpp`): parks the Ho 229 on `UT_Nomadics`, checks the anim instance,
  gear down, the MK 108 weapon with two muzzles, orders an attack on the ground (it takes off by itself), checks the
  take-off state and montage, the climb, that it fires, and that the gear is folded in flight.
  `-AircraftVtoClass=<path under Blueprints/Aircraft/>` and `-AircraftVtoGearBone=<bone|none>` test another aircraft.
- `-AircraftDeathValidate -AircraftDeathOnly=Ho229` / `Ho229Parked`: the crash in flight and on the ground.
- `-AircraftStrafeValidate` (`AircraftStrafeTest.cpp`): the `BalancedCommander` loadout lists the strafe; it is built
  from its template (a map started directly has no commander, so the player's manager loads nothing), runs on the
  anchor (bypassing requirement and cooldown), and two Ho 229s arrive and fire.

```
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_Nomadics -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -AircraftVtoValidate -log
```

## Not done

- Landing and return to base are not driven by any test.
- The training icon is a Blender render, not an in-game screenshot like the other aircraft icons.
- Jet engine sounds: it uses the Me 410's propeller sounds.
