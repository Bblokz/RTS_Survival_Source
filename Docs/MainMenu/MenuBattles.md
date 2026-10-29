# Main menu battles

The start map (`/Game/RTS_Survival/Blueprints/GameUI/StartGame/StartGame`) stages endless random German versus Soviet
battles behind the menu UI. The player only interacts with the UI (`RTS_BP_StartController` sets UI-only input); a
camera owned by the director films the battles and fades from one landscape to the next.

## How a battle plays

1. **Travel.** Every cut goes to a landscape (arena `Region`) picked at random among the ones the battle is not on,
   each equally likely however many arenas it holds: the desert (five arenas, the start view among them, with a
   refinery yard), the dry steppe, the forest meadow (sawmill yard), the winter ridge (depot yard) or the farm
   (crop fields, fences, hay, farm buildings, wandering herds). Within the landscape the arena is a `PickWeight` pick.
2. **Flavour.** Each battle is combined arms, armoured, infantry or an air raid, which weights what the waves bring.
3. **Drive-in.** The opening skirmish (one or two vehicles and maybe a squad per side) starts `ApproachDistance`
   behind its line and drives up to it for `ApproachSeconds`; the camera opens alongside a moving column. The tracks
   leave ruts on the desert and steppe landscapes (tank trail render target, `TankTrailStrength` on their materials).
4. **Escalation.** Three to five reinforcement waves arrive every ~15 s from further back (`ReinforcementDepth`) and
   drive into the fight, up to `MaxGroundUnitsPerSide`; aircraft join from the second wave on and climb to
   `AircraftFlightHeight` (3200 cm) before their runs. A side that is wiped out pulls the next wave forward.
5. **End.** After the last wave, the battle is decided when one side has no living ground units (or
   `MaxBattleSeconds` after that wave), the camera holds on the victors, fades out, and the next battle starts.

Vehicles and squads are drawn from the base pools, or at `VariantChance` (35%) from the variant pools, which the
setup script harvests from `DA_UnitVariants` (combat variants only). The camera pulls back as the fighting spreads.

The desert landscape ends at ±25200 cm, so its arenas use shorter approach and reinforcement depths and sit far
enough in for their deepest slots; past any edge the camera would see the sky atmosphere's bare ground. As a safety
net the director moves a slot without ground under it towards the middle of the battle until it has
(`GetSlotOverGround`), and the dressing script skips props past every landscape's edge.

## Pieces

| Class | Role |
| --- | --- |
| `ARTSMainMenuGameMode` (`MainMenu/`) | Parent of `BP_GM_StartGame`. Primes the game instance's player faction (units query it on init; faction selection overwrites it later) and spawns the start controller flagged as the main menu controller. |
| `ACPPController::bM_IsMainMenuController` | Set by that game mode. Skips the in-game controller setup the menu has no data for: start-game pause, formations, rotation arrow, aim ability, key bindings, hover info and previews. |
| `ARTSMenuBattleArena` (`MainMenu/MenuBattles/`) | Placed per battle spot. Germans line up behind the origin facing along the arrow, Soviets face them `LineSeparation` away. Vehicles in front, infantry one rank back, aircraft at the back. Settings: approach and reinforcement depth, opening and wave sizes, wave count and interval, aircraft chance, unit cap, camera distance/height, region, pick weight, `bKeepUnitsOutOfFog` (set on the four landscapes outside the fog manager's map extent). |
| `ARTSMenuBattleDirector` | Placed once. Holds the German and Soviet unit pools (vehicles, squads, aircraft). Picks an arena and a random roster, async loads it during the previous battle, spawns Germans as player 1 and Soviets as player 2 (`FRTSUnitOwnerSpawning`), orders every unit to attack the nearest living enemy, decides the battle when one side has no living ground units (or after `MaxBattleSeconds`), holds, fades out, cleans up units, wrecks and team weapons, and starts the next. |
| `ARTSMenuBattleCamera` | Spawned by the director; cuts every 5 to 8.5 s between column track, orbit, flank dolly, behind-a-line, chase and high overview shots around the eased centre of the living ground units, standing on the outer side of the arena so it looks across the landscape under it. |
| `FRTSUnitOwnerSpawning` (`Utils/UnitSpawning/`) | Shipping-safe spawn of any unit Blueprint for a chosen player (overrides the Blueprint RTS component templates' owner before spawn); also used by the grenade map tests. |

Battle units leave the fog of war and are kept visible (`bKeepBattleUnitsOutOfFog`, the Shorts approach): in the
fog, unexplored ground around each battle turned black on camera and units could not find hidden targets. The fog
manager's capture is widened to all five landscapes so none of them falls outside it (outside it renders black).
`-MenuBattleGameplayFog` plays the battles in the fog like a mission instead. Health bars are made invisible and unit voice lines are suppressed. The placed `RTS_BP_EnemyController` is quieted (thinking starts only
on a mission trigger, no training, building or direct control) so it never takes the Soviet units over.

`RTS_BP_GameState`'s ReferenceCast macro initializes the explosion and decal managers also when the controller is not
`RTS_BP_PlayerController` (cast-failed path), so explosions and scorch decals work on the menu.

## Rebuilding the level setup

`menu_battles_setup.py` places the nine arenas and the director with its pools (and the variant pools from
`DA_UnitVariants`), quiets the enemy controller, removes the old test tanks, reparents `BP_GM_StartGame`, rebuilds the
navmesh and saves. It calls `menu_battles_world.py`, which (re)builds everything labelled `MenuWorld_`: the sky
atmosphere and real-time sky light, the Steppe, Meadow, Farm and Winter landscapes (generated bowls with ridged rims, painted
layers, the desert landscape's collision profile copied over), their nav bounds, and the dressing of every region: trees, rocks, wrecks,
craters, industrial yards (a grid of `BP_IndustrialZone_Ground02` tiles on ground flattened for them, hangars,
containers, scorch powerlines and trees), the farm's crop fields (outline splines, as on Ger_Trains) with gated
fences, farm buildings, tractors and `BP_Wanderer_*` animals, and bird flocks over every landscape. It needs a DebugGame (or freshly
built Development) editor that has the menu battle classes:

```
UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject -ExecutePythonScript=Source/Docs/MainMenu/menu_battles_setup.py -unattended -RenderOffscreen
```

Close StartGame in any other editor first: the save fails while another process has the map open. To try a change
while it is open, set `MENU_BATTLES_SAVE_AS=/Game/.../StartGame_MenuTest` and the script saves a copy there instead.

The `-run=pythonscript` and `ResavePackages -BuildNavigationData` commandlets crash while saving this map (landscape
collision PreSave compiles shaders), so the script runs in a full editor, which quits once it returns.

## Testing headlessly

```
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Blueprints/GameUI/StartGame/StartGame -game -windowed -resx=1280 -resy=720 -RenderOffscreen -unattended -nosound -MenuBattleShots=6 -MenuBattleQuitAfter=170
```

`-MenuBattleShots=<s>` saves screenshots to `Saved/MenuBattleShots`, `-MenuBattleQuitAfter=<s>` exits,
`-MenuBattleGameplayFog` keeps the units in the fog of war. The log prints `RTS_MENU_BATTLE_*` lines for every
roster, spawn, start, decision and end.
