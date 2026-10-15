# Soviet BS-3 100 mm field gun (team weapon and pit)

The BS-3 comes in two forms:
- **the team weapon squad** `Squad_Rus_BS3_100MM`: a crew pushes the gun, deploys it and fires;
- **the BS-3 pit** `BTX_RusBS3Pit`: an enemy emplacement (owner 2) that holds the same gun inside a square sandbag wall open at the rear, with four crew and a round handover drill.

The model is KADO-3D's (`D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/BS3_100mmAT`). It is built the same way as the 53-K ([Rus53K.md](Rus53K.md)).

## The team weapon

| | |
|---|---|
| Squad | `Squad_Rus_BS3_100MM` ("BS-3 100mm"), in the anti-tank list; 550 radixite / 240 metal (`RusBS3_100mm_*`), enemy cost 22 |
| Weapon | `BS3_100MM_TW` ("BS-3"): the SU-100's D-10S data with a 9 s reload |
| Training | `HeavyAntiTankGunTeamWeaponTrainingTime` |
| Blueprint helper | `URTSBlueprintFunctionLibrary::GetTeamWeapon_BS3_100mm()` |

### Assets (`/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/BS3_100MM/`)

| Asset | |
|---|---|
| `Mesh/SK_TW_BS3` | on the **ZiS-3 skeleton** `SK_TW_Zis3_Skeleton`; materials `MI_BS3` (M_MasterPainter1) and `MI_BS3_Burnt` |
| `Mesh/SM_Destroyed_BS3` | the wreck |
| `DeployPacked`, `WeaponAnims`, `WheelsAnims` | `Anim_BS3_*`: Deployed, Packed, DeployedToPacked (3 s), 9 aim poses, Fire (45 cm recoil, 1.6 s), WheelsFWD; `AO_BS3`; the three montages |
| `AnimBp_TW_BS3` | a copy of the ZiS-3 anim BP with every ZiS-3 asset swapped |
| `Rus_TW_BS3_100mm` | the gun: fires from `LaunchBS3`, casings from `OutBS3`, the LB-1 100 mm sound, launch effects ×1.3, loader at (−165, 72) and spotter at (−225, −82), turret height 107 |
| `Rus_TWSquad_BS3_100mm` | the squad (`TeamWeaponClass` = the gun); registered in the spawner's `SetupRusSquads` |
| `DestroyedTW_BS3` | the scavengeable wreck |

### Rig notes

- **Scale and frame:**
  - One source unit is 82 cm (the real gun is 2.15 m wide and 1.5 m high, and game guns are about 0.95 of real size).
  - The frame origin is on the ground under the axle.
- **Measurements:**
  - bore height 107 cm;
  - muzzle at +366 cm, breech at −87 cm;
  - spades at (−339, ±257) cm, 4.31 m from the pivot.
- **Recoiling parts:** the barrel's two mesh segments meet hidden inside the cradle's front collar. The collar therefore recoils with the barrel as a recoil sleeve; left on the cradle, it would show a 26 cm gap in the barrel on every shot.
- **Packed trails:** `bs3_build.py` computes how far the trails swing in from the spade positions (38.8°), so the spades meet behind the gun.

## The pit

| | |
|---|---|
| Expansion type | `BTX_RusBS3Pit` ("BS-3 Pit") |
| Blueprint | `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Rus_BXP_BS3Pit` |
| Health | `T3BxpBunkerHealth` × 1.25, reinforced resistances |
| Cost | 650 radixite, 320 metal (`RusBS3Pit_*`) |
| Footprint | 1300 × 1300; the wall is 4.8 m inside the centre and 6.2 m outside |

| Folder | Content |
|---|---|
| `Meshes/SM_BS3Pit` | the square sandbag wall, `MI_Rus_SandBag_01`, complex collision |
| `Anims` | `AnimBp_BS3Pit` (parent `UGunPitAnimInstance`: yaw on `37mmFlak`, pitch on `B_Mantlet`) and `Montage_BS3Pit_Fire` |
| `Crew` | `Anim_BS3Pit_Handler_Idle/Reload` and `Montage_BS3Pit_Handler_Reload` (Infantry Master skeleton) |
| `Turrets/RTS_BP_EmbTurret_RusBS3Pit` | the embedded turret, a copy of the 53-K pit's with the BS-3, its crew layout, crate, carry poses, 100 mm rounds (the 75 mm round at 1.15 scale), 18°/s traverse, −5° to +30° pitch |

### The square wall

The wall is sandbags only, three rows thick:
- **Height:** the rows step down outwards (4, 3 and 2 courses, about 62 cm high), so the bore fires over the wall.
- **Corners:** they are bonded. In each course one pair of opposite walls runs through the corner and the other pair stops against it, alternating course by course.
- **Entrance:** the rear wall (−X) has a 2 m gap. Bags turned across the wall close each row's end, and the outer rows stop a little short of the gap, so its sides step down like a stair.

### The crew drill, independent of the pit

Every crew spot is in gun space on `PitOriginBS3`, an identity socket on `37mmFlak`. The ready crate and both rounds ride on the gun, and the crew stand on flat ground. Nothing in the drill refers to the pit.

**The rule for reusing the drill in another pit:** keep a clear radius of about **4.6 m** around the gun's pivot. That covers the spades (4.31 m) and the crate's far corner (about 4.4 m). Then copy the turret.

| Crew | Spot (gun space, cm) | Mesh yaw | Animation |
|---|---|---|---|
| Crew1 loader | (−184.75, 45.22) | −70 | `AN_Infantry_MortarCrew_Crouched`, reload `Montage_GerPit_LoadPak` |
| Crew2 handler | at `PitOriginBS3` (the drill carries his spots) | 0 | `Anim_BS3Pit_Handler_Idle`, reload `Montage_BS3Pit_Handler_Reload` |
| Crew3 gun layer | (−115, −62) | −90 | `AN_Infantry_SpotPAK_Crouched` |
| Crew4 commander | (−330, −110) | −90 | `AN_Infantry_CommandoCrouched` |
| Ready crate | (−344, 143), turned 110° | | |

**Why the layout is turned.** The drill is the Pak 40 pit's (the PaK loader's own animation and the handler's handover, ram and pick-up), but on the BS-3 the Pak's handover spot and crate would sit on the left trail. `bs3_crew.py` therefore turns the whole Pak layout 20° about the breech and moves it to the BS-3's breech:
- the loader, the handler's walk and the crate then all stand between the trails;
- nobody steps over a trail;
- the loader rams 20° off the bore axis, at the breech position.

The handler's drill was then baked fresh with the B-4 crew solver. Its worst hand miss is 19 cm.

## Rebuilding

**Blender** (`D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/BS3_100mmAT/Scripts`, Blender 4.0):
1. `bs3_build.py`: the rigged mesh, its animations and wreck, the textures and `bs3_rig_info.json`.
2. `bs3_pit_build.py`: the square wall, built with the DShK pit's sandbag generator.
3. `bs3_crew.py`: the handler's drill, plus `Exports/Crew/bs3pit_carry.json` and `bs3_crew_layout.json`. It reads the breech from `bs3_rig_info.json` and the German pit sources from `GerDefenses/Source`.

**Unreal** ([Tools/BS3](../../Tools/BS3)). The scripts talk to the editor bridge through scratchpad `uecp.py`:
1. `ue_bs3_import.py` (bridge `pyfile`) imports the materials, mesh, animations and wreck, and prints the `LaunchBS3` / `OutBS3` socket transforms. Add those sockets with `add_skeleton_socket` and `set_skeleton_socket_transform` on `B_Gun`.
2. `build_bs3_animbp.py`, then `build_bs3_teamweapon.py`.
3. `build_bs3_pit.py [crew,mesh,socket,animbp,turret,bxp]`.

**A second editor:** when another editor already owns the bridge port 9855, a second editor on the project still serves its own endpoint, usually `http://127.0.0.1:30001/mcp`; its log names it. Set `UECP_PORT=30001` and `uecp.py` talks to that editor directly.

## Testing

- **Squad:**
  - Command: `-B4HowitzerValidate -B4HowitzerSquad=/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/BS3_100MM/Rus_TWSquad_BS3_100mm.Rus_TWSquad_BS3_100mm_C -B4HowitzerSquadReload=9`.
  - It checks deploy, pack–move–redeploy, attack-ground and the reload interval (9.6 s measured).
- **Pit:**
  - Command: `-BS3PitValidate` on TestCover. For pictures add `-GerPitCapture=<dir> -CaptureExposureBias=3 -ExecCmds="r.TextureStreaming 0"`.
  - Setup: the enemy pit is ordered to attack the ground 60° off its forward.
  - It checks:
    - the crew and crate on `PitOriginBS3`;
    - the rounds;
    - the gun turned before the first shot;
    - the crew turned with the gun;
    - the handover → ram → pick-up drill, with the hands within 34 cm of the round.
