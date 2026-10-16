# Molotov (Soviet grenade subtype)

The molotov is a grenade subtype: `EGrenadeAbilityType::RusMolotov` (CustomType 6) on the normal `IdThrowGrenade` /
`IdCancelThrowGrenade` card entries. It runs through `UGrenadeComponent` like every other grenade; three opt-in grenade
settings make it a molotov. Existing grenades keep their behaviour because all three default to off.

| Setting (`FGrenadeComponentSettings`) | Molotov value | What it does |
|---|---|---|
| `DamageType` | Fire | Damage type of the impact and of the burn (default Kinetic). |
| `AttachedGrenadeEffect` / `AttachedGrenadeEffectScale` | `NS_Molotov_RagFlame`, 2.5 | Niagara on the grenade mesh's `GrenadeEffect` socket while held and in flight. It always points up (absolute rotation). |
| `BurnArea` (`FGrenadeBurnAreaSettings`) | 6 s, 1 s ticks, 20 damage, radius 200 | After the impact the area keeps taking damage (rear-armor AOE, shield source Flamethrower). When the burn ends, the impact Niagara is deactivated. |

A grenade actor whose fire is still burning is not reused from the pool (`AGrenadeActor::GetIsAvailableForThrow`).

## Where it is set up

- `Rus_Squad_Mosin` has a `MolotovGrenade` component (2 throwers, 1 each, range 900, impact 30 fire in radius 200,
  armor pen 10/5/20, cooldown 25, card slot 7). `Rus_Squad_Mosin_Soviet` inherits it. Any other squad gets molotovs the
  same way: add a Grenade component and copy these settings.
- Card text and icons: `Buttons_DataTableb`, key 6 in the `CustomTypeMapping` of `IdThrowGrenade` and
  `IdCancelThrowGrenade` (`T_AB_Molotov`, `T_AB_CancelMolotov`).
- Fire on infantry first fills their fire threshold (10 % of max HP, recovers 5/s). The 30 impact fills it, so the
  burn ticks deal real damage.

## Assets

| Asset | Notes |
|---|---|
| `Meshes/Weapons/Grenades/Molotov/SM_Molotov` + `M_Molotov`, `T_Molotov_BaseColor/Normal/ORM` | 65 cm (grenade scale, about 2.2x real), base at the origin along +Z, `GrenadeEffect` socket on the rag. |
| `SpecialEffects/Molotov/NS_MolotovImpact` | Fireball burst (`NFire_Exp_03` cores and sparks), then a burning pool with dark smoke (`NS_SmokeyFire1`). Flames end around 6 s, smoke around 8 s. |
| `SpecialEffects/Molotov/NS_Molotov_RagFlame` | Copy of the M5 torch fire; sized by the component scale through the pack's `NM_Scale` module. |
| `Sounds/Explosions/Grenades/Molotov/Cue_MolotovImpact` | Random glass break mixed with a random fireball whoosh. |

None of the materials use DitherTemporalAA. The `*_DitherAA` fire systems and `Niagara_Splash_Flame_01` were left out
because they do.

**Niagara timing:** a copied emitter only obeys its own `Loop Behavior` / `Loop Duration` when its `EmitterState`
`Life Cycle Mode` is **Self** (`NewEnumerator1`). With System it follows the system and the copied fire loops forever.
In `ENiagara_EmitterStateOptions`, Loop Behavior `NewEnumerator1` is Once.

## Rebuilding the art

- Model, UVs, baked textures, previews and the icon render, headless:
  `Blender5.0.exe -b --factory-startup --python Source/Tools/Molotov/blender_build_molotov.py -- <out dir>`
- Icons on the red grenade hex plate: `python Source/Tools/Molotov/make_icons.py <exported T_AB_*.png dir> <icon_render.png> <out dir>`

## Play test

`-MolotovValidate` (`GrenadeComponent/Tests/MolotovTestSubsystem`, not in shipping) spawns an enemy-owned
`Rus_Squad_Mosin` and a player `Squad_Ger_Mauser` on the nearest navmesh. It orders the molotov and checks five things:
the rag burns in hand and in flight, the impact lands on the target, the target keeps losing health after the impact,
and the effect is over after the burn. It also saves pictures (`-MolotovDir=<folder>`).

```
UnrealEditor-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nosound
  -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab -benchmark -fps=30 -MolotovValidate
  -MolotovDir=<folder>
```

The map needs a navmesh: the squad walks into throw range, and a squad that cannot path never throws.
`UT_B4Howitzer` has no navmesh. The thrower squad is the enemy one because soldiers keep their own Blueprint's owner
(player 1) and the grenade takes its targets from the squad's owner.
