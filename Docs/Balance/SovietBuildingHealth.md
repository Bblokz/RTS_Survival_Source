# Soviet building health (medium tank shots)

Changed 2026-11-16. Soviet base buildings take their health from a number of medium tank shots instead of the old
nomadic-building multiples (the cooling towers needed 72 Pz IV shots, the factory 308).

## Reference shot

Pz IV KwK 40 L48, APHE (default shell), direct hit: `1.2 x 75 mm + 1.8 x 73 g TNT = 221.4` damage.

- Buildings carry no `UArmorCalculation`, so a hit goes to `AProjectile::HandleHitActorAndClearTimer` at full damage.
- Projectile damage has no flux (`M_FullDamage = WeaponData.BaseDamage`), so the count is exact.
- Shell splash (`AProjectile::HandleAoe`) ignores the directly hit actor.
- Armor reduction (`UHealthComponent::TakeDamage`): damage x multiplier, then minus flat points.
  - Basic building armor (20 points, x1.0): 201.4 per shot.
  - Reinforced armor (25 points, x0.7): 130.0 per shot.
- HE fires at x1.6 damage and needs fewer shots. Heavier guns need fewer shots too.

Health is `(shots - 0.5) x damage per shot`, rounded to 10, so it sits in the middle of the shot bracket and small
damage changes do not move the count. The helper and shot counts live in
`DeveloperSettings::GameBalance::SovietBuildingShotHealth`.

## Shots to kill

| Building (enum) | Blueprints | Armor | Shots | Health |
|---|---|---|---|---|
| FuelStorage1 | FuelStorage, FuelStorageLarge, FuelStorageTankSmall | Basic | 3 | 500 |
| FuelStorage1 (BP scales to x0.5) | FuelStorageSMall | Basic | 2 | 250 |
| FuelStorage1 (BP scales to x1.4) | FuelStorageTank | Basic | 4 | 700 |
| AmmoStorage | AmmoStorage | Basic | 3 | 500 |
| Barracks | Barracks (+ _Concrete), LargeTent (+ _Soviet) | Basic | 4 | 700 |
| Antenna | (industrial set) | Basic | 4 | 700 |
| FuelStorage2 / SuppliesStorage | FuelStorage2, Supplies | Basic | 5 | 910 |
| Warehouse | (industrial set) | Basic | 5 | 910 |
| CoolingTowers | CoolingTowers (+ _Soviet, _Concrete) | Basic | 6 | 1110 |
| AquirfierVertical | (industrial set) | Reinforced | 8 | 970 |
| Aquirfier | (industrial set) | Reinforced | 10 | 1230 |
| ResearchCenter | ResearchCenter, AdminBunker | Reinforced | 8 | 970 |
| IndustrialRefinery | - | Reinforced | 10 | 1230 |
| ChemicalPlant | (industrial set) | Reinforced | 12 | 1490 |
| Factory | Factory | Reinforced | 12 | 1490 |
| PlatformFactory / AssemblyYard | PlatformFactory, AssemblyYard | Reinforced | 16 | 2010 |

Not changed: defences (walls, gates, MG bunker, guard tower, Bofors, camo/dome bunkers, Bunker 02/05, mega bunker, gun
pits, turret pillbox). On Reinforced armor a wall segment is about 5 shots and the MG bunker about 8, so the barracks is
now weaker than a wall.

## Verifying

`-SovietBuildingShotBench` (`Buildings/BuildingExpansion/Tests/SovietBuildingShotBench`) spawns every Blueprint above as
player 2 on `RTS_ToCopySmall`, hits each with the KwK 40 damage read from the game state, and logs
`RTS_SBSB RESULT <bp> max_health=<hp> shots=<n>`. Run from PowerShell (Git Bash rewrites `/Game` paths):

```
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game
  -unattended -nop4 -nosound -RenderOffscreen -windowed -ResX=640 -ResY=360 -DisablePlugins=Fab,BpGeneratorUltimate
  -SovietBuildingShotBench -abslog=<log>
```
