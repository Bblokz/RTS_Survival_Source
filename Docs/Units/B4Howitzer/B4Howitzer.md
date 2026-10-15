# B-4 203mm howitzer (team weapon + Soviet gun pit)

How-to and pitfalls for building a crewed emplacement like this one: [B4Pit_LessonsLearned.md](B4Pit_LessonsLearned.md).

The B-4 exists twice in the game:

- **Team weapon** `Squad_Rus_B4_203mm`: a crewed, towable heavy howitzer that packs, moves on its wheels, deploys
  and fires like the other team weapons (weapon `B4_203MM_TW`, reload 14 s).
- **Building expansion** `BTX_RusB4HowitzerPit`: the same gun on a turntable in a concrete pit, with four animated
  crew (custom animation players, not squad units) who load every shell with the crane. Weapon `B4_203MM_Pit`
  (reload 24 s, 25 % more range, less accurate). Only the gun fights; the crew is decoration.

## Assets

Everything lives under `/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/`.

| Asset | Purpose |
|---|---|
| `Mesh/SK_TW_B4_Howitzer` | Skeletal mesh (LODs, sockets `Launch`, `Out`, `PitOrigin`, `ShellMount`, `CartMount`, `RammerMount`). |
| `AnimBp_TW_B4` | Team weapon anim BP: deployed/packed base, aim offset `AO_B4`, `Gun` slot for the recoil. |
| `BP_TW_B4`, `Rus_SquadTW_B4`, `DestroyedTW_B4` | Team weapon actor, its squad and wreck. |
| `Pit/AnimBp_B4Pit_Gun` | Pit gun anim BP: yaw on `B_Root` (turntable), pitch on `B_Mantlet`, faded out by curve `B4_LoadingElevation` while loading. |
| `Pit/AnimBp_B4Pit_Crew` | Crew anim BP (`UB4PitCrewAnimInstance`): idle loop bound to `IdleAnimation`, reload montage in `DefaultSlot`. |
| `Pit/Anims/Montage_B4Pit_Reload` | 24 s crane loading drill on the gun (recoil, crane lift, tray, ramming, back to aim). |
| `Pit/Crew/Montage_B4PitCrew{1-4}_Reload` | Crew drills authored frame by frame against the gun drill. |
| `Pit/RTS_BP_EmbTurret_B4Pit` | Embedded turret: gun, `PitCrew` component, props and four crew meshes. |
| `Pit/Rus_BXP_B4HowitzerPit` | The building expansion (emplacement mesh, child actor = the turret above). |
| `Pit/Meshes/SM_B4Pit_Sandbags` | Sandbag parapet (2 rows, 3 courses) on the revetment, entrance walls and spare piles; a no-collision component on the pit and its wreck, textured with `MI_Rus_SandBag_01`, LOD group LargeProp. |

**Placing the pit.** Like the other Soviet emplacements (Bofors, MG and mega bunkers) the pit is not built by a
player building: designers place `Rus_BXP_B4HowitzerPit` in a level or in an enemy base (bunker PCG lists). Its
defaults match those emplacements: owned by the enemy (player 2), turret set to auto-engage at BeginPlay, and the
enemy AI counts it as a base satellite (`SatteliteBuildingsForBase` on `RTS_BP_EnemyController`). When it dies it
disables its weapon, collapses with an explosion and leaves `Pit/BP_Obstacle_B4Pit_Destroyed` (the scorched
emplacement, permanent: the 12 s lifespan copied from the MG-bunker wreck was removed) plus the scavengeable
`DestroyedTW_B4` wreck turned the way the gun last pointed.

**Showcase map.** `/Game/RTS_Survival/Maps/UnitTests/UT_B4Howitzer` (a copy of Test_TrainingUI) has the enemy pit
(`B4_HowitzerPit_Enemy`) and the enemy B-4 squad (`B4_Squad_Enemy`) placed in front of the player's base, next to
the camera start: press Start and both open fire on the base, and the pit's crew loads with the crane after the shot.
**Player version.** `BTX_GerCapturedB4Pit` ("Captured B-4 Pit", defense section of the German HQ's build options)
is the same pit for the player: `Pit/Ger_BXP_CapturedB4Pit`, a child of the enemy pit with owner 1,
`PitExpansionType = BTX_GerCapturedB4Pit` and `bEngageWhenPlaced = false`, so its gun stays silent until the
construction finishes (the enemy pit, placed in levels, engages at once). Its option icon is the `DT_Items` row
`BTX_GerCapturedB4Pit` (`Pit/UI/SB_OptionImage_B4Pit`). To drop the player version, remove the
`AsDefense(BTX_GerCapturedB4Pit)` entry from the German HQ options in `ACPPGameState`.

The spawner (`RTS_BP_RTSAsyncSpawner`) maps `Squad_Rus_B4_203mm` to `Rus_SquadTW_B4` and the pit type to
`Rus_BXP_B4HowitzerPit` (preview mesh `SM_B4Pit_Emplacement`). Game data (cost, health, weapon stats) is in
`ACPPGameState` next to the other Soviet entries.

## How the pit loading drill works

`UB4HowitzerPitCrewComponent` (C++) drives the drill:

- `InitPitCrew(Turret)` in the turret's BeginPlay attaches the skeletal meshes tagged `B4PitCrew` (sorted by name,
  Crew1..Crew4) to socket `PitOrigin` and every component tagged `B4PitSocket:<Socket>` to that gun socket with
  its Blueprint relative transform. Blueprint components cannot be parented to sockets of the inherited turret
  mesh, which is why this happens at runtime.
- `OnReloadStarted(ReloadTime)` (from the turret's `ReloadWeapon` event) plays the gun reload montage and every crew
  montage at `length / ReloadTime`, so the breech closes exactly when the weapon can fire again. Reloads shorter
  than `MinReloadTimeForLoadingSequence` only recoil.
- `OnGunFired()` (from `PlaySingleFireAnimation`) plays the recoil when no drill is running.

Crew roles: Crew1 rams the shell, Crew2 pushes the ammo cart, Crew3 hooks the shell to the crane, Crew4 works the
crane winch.

## Rebuilding the art

Blender scripts: `D:\Blender\BlenderProjects\Marketplace\Scritps\AnimTeamWeapons\B4_203mm\`
(`b4_bake_textures.py`, `b4_build_team_weapon.py`, `b4_pit_animations.py`, `b4_pit_crew.py`, `b4_pit_props.py`,
`b4_destroyed.py`), run headless with Blender 4.0. They export to
`D:\Blender\BlenderProjects\BlenderTanks\(KADO-3D)\(EXPORTS)\B4_203mm\`.

Unreal import: `Tools/B4Howitzer/ue_b4_import.py` (team weapon) and `Tools/B4Howitzer/ue_b4_pit_import.py`
(pit gun animations, crew animations, props), run inside the editor. Re-importing an animation deletes the old
asset first, because a 30 fps import over a 25 fps asset fails.

## Testing

`-B4HowitzerValidate` (Development target, map `/Game/RTS_Survival/Maps/TestCover/TestCover`) spawns the squad and
the pit away from the map's fighting and checks: default enemy owner, deploy, pack-move-redeploy, an attack-ground
order 25 degrees outside the yaw arc (pack, turn, redeploy, fire), the reload interval, crew and prop attachment in
the pit, the loading drill length against the reload, how close the crew's hands come to the shell, and the pit's
own reload interval. It logs `RTS_B4_TEST` lines (`RESULT PASS failed_checks=0`) and quits after about 2 minutes.
`-B4HowitzerValidate -B4HowitzerObserve` on `UT_B4Howitzer` spawns nothing (Observe alone starts no scenario): it checks the placed pit and squad (enemy owner, crew and
props attached) and that both engage the base and the pit completes a loading drill.
`-B4HowitzerPlayerBuild` on `Test_TrainingUI` builds the captured pit like a player: waits for the German HQ to
become a building, checks the option is offered, clicks it, places and pays for the pit, then checks it is the
player's, holds fire while it is built and fires and reloads with the crane once it is.
`-B4HowitzerSquad=<squad class path>` runs another team weapon through the squad half (its reload check then uses
the B-4's 14 s).

```
UnrealEditor-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/TestCover/TestCover -game -unattended -NullRHI -nosound -benchmark -fps=60 -B4HowitzerValidate
```

`-B4HowitzerCapture=<folder>` (drop `-NullRHI`, add `-RenderOffscreen -windowed -ResX=1280 -ResY=720`) also saves
pictures of every phase from scene-capture cameras: no window opens. `B4Pit_LoadingDrill.png` next to this file is
a contact sheet of the pit's loading drill made that way.

Under `-NullRHI` nothing is rendered, so the test forces the meshes to animate and resets the soldiers' movement
tick every frame; the numbers are real; use the capture run to see how it looks.

## Fire feedback

On every shot the gun bucks (top carriage only) and the crew brace. See [../GunPitFireFeedback.md](../GunPitFireFeedback.md).
