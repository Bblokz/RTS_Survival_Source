# Global ability target areas

Recon flight, flare curtain, air-dropped minefield and radixite nuke. They differ from the older global abilities in
two ways: a strip is aimed with two clicks, and the target area is a procedural shader that shows what will really
happen rather than a crosshair texture.

## Abilities

| Enum | Class | Aircraft / delivery |
| --- | --- | --- |
| `GA_GerReconFlight`, `GA_SovietReconFlight` | `UGA_ReconFlight` | `Ger_Aircraft_FW189_Recon`, `Rus_PE2_Aircraft_Recon` (no Po-2 exists in Content) |
| `GA_GerFlareCurtain` | `UGA_FlareCurtain` | `Ger_Aircraft_JU390B_AirborneUnarmed` |
| `GA_GerAirMinefield`, `GA_SovietAirMinefield` | `UGA_AirMinefield` | `Ger_Aircraft_ME5_Airbrone_Transport`, `Rus_PE8_Aircraft_Transport` |
| `GA_GerRadixiteNuke`, `GA_SovietRadixiteNuke` | `UGA_RadixiteNuke` | V2 mesh falling on the target (`ARadixiteNuke`) |

The first three derive from `UGA_StripFlight`: it spawns the aircraft on the line of the strip at the map edge, orders
`OrderStraightFlightRun` and reports progress along the strip. A shot-down aircraft ends the run, so what it had not
reached does not happen. Enemy AI and missions only give a point; the strip is then centred on it at its longest length.

The aim settings are the single source of the sizes: `AreaRadius` is the reveal radius, the flare vision radius, the
half width of the minefield or the blast radius, and `ShaderMarkerCount` is the number of flares.

## Aiming

- `FGlobalAbilityAimSettings::AreaAim.Shape`: `Reticle` is the old crosshair path, `Circle` one click, `Strip` two clicks.
- `ACPPController::TryHandleActiveGlobalAbilityPrimaryClick`: the first click anchors the strip in `APlayerAimAbility`,
  the second calls `UGlobalAbility::OnClickedAbilityLine`. Right click cancels at any time.
- `APlayerAimAbility` shows areas on its own plane (`M_AreaMeshComponent`), with the actor at scale 1. Drop points come
  from `UGlobalAbility::GetStripDropPoints` and are drawn as instanced ghosts, so the preview cannot disagree with the
  drop: the mine scatter is seeded, and spots off the navmesh are skipped in both.

## Shaders

`Source/Tools/GlobalAbilityTargetAreas/build_target_area_materials.py` rebuilds every material in
`/Game/RTS_Survival/Materials/AimMaterials/TargetAreas/`; the HLSL lives in that script. Parameters are in cm:
`PlaneLength`, `PlaneWidth`, `StripLength`, `AreaRadius`, `InnerRadius`, `MarkerCount`, `IsDrawing`. They are unlit,
alpha composite and depth test off like `M_AimCrosshair`. `M_RadixiteNukeShockwave` reads `Progress`; the
`M_RadixiteNukeScar` decal reads `Glow`. `M_DropPointGhost` needs Used With Instanced Static Meshes.

`NS_RadixiteNuke` is a copy of `/Game/BigExplosions/Niagara/NS_Nuke_3`, turned green with Scale Color on each emitter.

## Data

`Source/Tools/GlobalAbilityTargetAreas/setup_target_area_abilities.py` writes the templates in
`DA_GlobalAbilitySystem`. Run it on a build that has these classes; `Source/Tools/GlobalAbilityIcons` has their icons.
Costs are empty like every other template. The first balance is: nuke core 4000, blast 1400 out to 45 m, radiation 60
per 2 s for 30 s, hitting both sides (`AffectedUnits`), after an 8 s warning.

The player gets the German four through `CommanderSettingsByType` in `DefaultGame.ini`; the panel has 8 slots. The enemy
loadout is per placed `AEnemyController` (`M_EnemyGlobalAbilityLoadout`). `UEnemyStrategicAIComponent` already picks the
Soviet versions when they are in it: the minefield with the reinforcement abilities, recon with the light air strikes,
the nuke with heavy carpet bombing.

## Testing

```bash
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_Nomadics -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -GlobalAbilityAreaValidate -GlobalAbilityAreaCapture=<folder> -CaptureExposureBias=1.5
```

`UGlobalAbilityAreaTestSubsystem` logs `RTS_ABILITY_TEST`. It clicks through the controller, kills a recon aircraft
halfway, compares mines with the preview, checks enemy mines are hidden and that the nuke kills its core and spares
what is outside. It holds each aim stage two seconds and finishes shader compilation first: a picture in the frame the
area appears shows nothing. Flares and mines of earlier steps stay in later pictures.

`cmd.exe /c Build.bat` can report exit 0 after compile errors. Check the log and the DLL time.
