# Soviet WW2 grenades (2026-10-19)

Four grenade subtypes modelled on grenades the Red Army used in WW2, so the Soviet (enemy) squads are no longer limited
to the molotov. They run through `UGrenadeComponent` like every other grenade (see `GrenadeQuality.md`, `Molotov.md`).

| Type (`EGrenadeAbilityType`, card key) | Real grenade | Role | Range | Radius | Damage | Exp. | Fuse | Sticks | Throwers x each | Cooldown |
|---|---|---|---|---|---|---|---|---|---|---|
| `RusRGD33` (7) | RGD-33 stick grenade with fragmentation sleeve | anti-infantry, assault | 1000 | 250 | 90 | 1.0 | 1.5 s | no | 3 x 1 | 15 |
| `RusF1` (8) | F-1 "limonka" defensive grenade | anti-infantry, wide blast | 900 | 350 | 65 | 1.0 | 2.0 s | no | 3 x 1 | 20 |
| `RusRG42` (9) | RG-42 offensive tin-can grenade | anti-infantry, cheap | 1000 | 200 | 55 | 1.0 | 1.0 s | no | 2 x 2 | 15 |
| `RusRPG43` (10) | RPG-43 HEAT hand grenade | anti-armor | 700 | 125 | 300 | 1.5 | 0.75 s | yes | 2 x 1 | 20 |

Armor pen (full / falloff / max): RGD-33 15/5/25, F-1 10/5/20, RG-42 10/5/19, RPG-43 75/5/125 (the German AT
grenade's values: full damage below 75 rear armor, none above 125).

## Squads

| Squad Blueprint | Grenade |
|---|---|
| `Rus_T2Squad_RedHammer` (PPSh assault squad) | RGD-33 |
| `Rus_T2Squad_RedHammerPTRS` (PPSh + PTRS) | RPG-43 |
| `Rus_Squad_LargeMixBasic` (large mixed squad) | F-1 |
| `Rus_Squad_LargePTRSSquad` | RPG-43 |
| `Rus_Squad_SVT` | RG-42 |
| `Rus_Squad_MosinPTRS` | RG-42 |

| `Rus_T2Squad_ToxicGuard` (hazmat flamethrowers) | Molotov (`RusMolotov`, a copy of the Mosin squad's component) |

Each gets one `GrenadeComponent` (card slot 7; a squad holds a single grenade component). The molotov also stays on
`Rus_Squad_Mosin` / `Rus_Squad_Mosin_Soviet`.

## Behaviour abilities (2026-10-20)

Timed weapon buffs through `UApplyBehaviourAbilityComponent` + a `UBehaviourWeapon` Blueprint
(`Blueprints/Behaviours/Weapon/Soviet/`). `UBehaviourWeapon` reaches squad weapons through
`ASquadController::GetWeaponsOfSquad`, which on a team-weapon controller also returns the team weapon itself; the
calibre window decides which weapons a buff touches.

| Type (`EBehaviourAbilityType`, card key) | Behaviour Blueprint | Effect | Calibre window | Squads (card slot, cooldown) |
|---|---|---|---|---|
| `ArmorPiercingBullets` (18, existed without users) | `BP_Beh_Squad_APBullets_Soviet` | 15 s: +50 % armor pen, +15 % damage, +20 % reload time | <= 20 mm (rifles, PTRS; not the bazooka) | `Rus_Squad_PTRS` (+ `_Soviet` child), `Rus_Squad_LargePTRSSquad`, `Rus_Squad_MosinPTRS` (6, 40 s) |
| `RusCloseAssault` (21, new) | `BP_Beh_Squad_CloseAssault_Soviet` | 12 s: -33 % time between shots, -50 % reload time, +15 % damage, -15 % accuracy | <= 20 mm | `Rus_T2Squad_RedHammer`, `Rus_T2Squad_RedHammerPTRS`, `Rus_Squad_LargeMixBasic` (6, 35 s) |
| `RusMortarBarrage` (22, new) | `BP_Beh_MortarBarrage_Soviet` | 15 s: -60 % reload time, -50 % time between shots, -20 % accuracy | >= 50 mm (the mortar only, not the crew's rifles) | `Rus_SquadTW_80mmMortar`, `Rus_SquadTW_120mmMortar` (+ `_Soviet` children) (7, 60 s) |

Multipliers scale the weapon value (`UBehaviourWeapon::CalculateAttributes...`: value * (mult - 1) is added; 0 = unchanged).
Enemy AI (`FEnemyAbilityUsageProfiles::GetBehaviourUsage`): AP bullets in combat vs armor, Close Assault at close
range, Rapid Barrage in any combat (default). Card icons reuse existing art: `T_Beh_ArmorPiercing`, `T_Beh_MGBoost`,
`T_Beh_ExplosiveShells`; buff-bar icons `EBehaviourIcon::ArmorPiercingBullets`, `VultureRapidFire`, `ExplosiveShells`.

The mortar barrage is a fire-rate buff on whatever the mortar is shooting (attack or attack-ground), not a targeted
barrage: `UAimAbilityComponent` has squad (infantry weapons) and tank variants only. A targeted version needs a
team-weapon aim component (collect `ATeamWeapon` weapons, fire through the controller's attack-ground/deploy flow).

## Code

- `EGrenadeAbilityType`: the four values are appended after `RusMolotov`; the values are the `CustomTypeMapping` keys of
  the card rows, so never insert in the middle.
- `FEnemyAbilityUsageProfiles::GetGrenadeUsage`: `RusRPG43` is anti-armor (the enemy AI throws it only at vehicles); the
  three frag grenades fall in the anti-infantry default.
- `APlayerAimAbility::GetAimTypeFromGrenadeAbility`: `RusRPG43` uses the anti-vehicle crosshair.
- `UGrenadeBenchmarkSubsystem`: scenarios `RGD33_vs_Infantry`, `F1_vs_Infantry`, `RG42_vs_Infantry`, `RPG43_vs_PzIV`,
  `RPG43_vs_PzIV_Driving` (`run_bench.sh <name> -GrenadeBenchOnly=RGD33,F1,RG42,RPG43`).

## Assets

| Asset | Notes |
|---|---|
| `Meshes/Weapons/Grenades/Soviet/SM_Rus_RGD33`, `SM_Rus_F1`, `SM_Rus_RG42`, `SM_Rus_RPG43` | No new textures: UV-mapped into patches of the existing grenade/mine atlas (`M_GrenadesMinesAtlas`, `T_MinesGrenadesAtlas_*`): olive paint, worn olive, the Cyrillic "ВНИМАНИЕ ВЗРЫВООПАСНО" stencil band (RG-42, RPG-43), steel and brass fuse parts. Pivot at the base / handle end, along +Z like `SM_M24StickGrenade`. Sizes 55 / 27 / 28 / 59 cm (about 2.2-2.9x real). Nanite off. |
| `Textures/GameUI/ActionUI/T_AB_<RGD33,F1,RG42,RPG43>Grenade`, `T_AB_Cancel<...>Grenade` | Card icons: model renders on the red grenade hex plate (same process as the molotov icons). |
| `Buttons_DataTableb` rows `IdThrowGrenade` / `IdCancelThrowGrenade` | Keys 7-10 in `CustomTypeMapping`: title, description, icon. |

Effects and sounds are reused: frag `NS_SmallCaliber_AirExplosion_1` + `Cue_Grenade_small` (RGD-33, RG-42), large
`NS_MediumCaliber_HighExplosive_DefaultImpact` + `Cue_Grenade_large` (F-1, RPG-43); attenuation, concurrency, effect
scale and montage time are copied from the German stick (frag) and HEAT (AT) components.

## Rebuilding

- Models, previews, icon renders (Blender 5, headless), in
  `D:/Blender/BlenderProjects/BlenderTanks/Infantry/Infweapons/SovietGrenades`:
  `Blender5.0.exe -b --factory-startup --python build_grenades.py` (needs `Saved/GrenadeAtlas/T_MinesGrenadesAtlas_Diffuse.png`,
  exported from the in-engine texture), then `preview_grenades.py` and `render_icons.py` on `SovietGrenades.blend`.
- Icons: `python Source/Tools/SovietGrenades/make_soviet_grenade_icons.py <T_AB_* refs> <icon renders> <out>`.
- Editor (UECP `execute_python`): `ue_import_soviet_grenades.py` (meshes), `ue_import_soviet_grenade_icons.py`,
  `ue_add_soviet_grenade_cards.py` (`execute_uecp_script`; card entries staged in `Saved/sovietgrenade_cards.json`),
  `ue_setup_soviet_grenade_squads.py` (components and tuning; needs the build with the new enum values),
  `ue_create_soviet_behaviours.py` (behaviour Blueprints), `ue_setup_soviet_behaviour_squads.py` (behaviour
  components). Both setup scripts take `ONLY = [...]` before `exec` to run a subset. Card entries for
  `IdApplyBehaviour` keys 18/21/22 are added by `ue_add_soviet_grenade_cards.py` as well.
- Editor Python gotcha: a free-standing `unreal.GrenadeComponentSettings()` refuses `EditDefaultsOnly` fields
  ("cannot be edited on instances"); edit the component template's own `M_Settings` struct and set it back.
