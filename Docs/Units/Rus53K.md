# Soviet 53-K 45 mm anti-tank gun (team weapon and pit)

The 53-K comes in three forms:
- **the team weapon squad** `Squad_Rus_53K_45MM`: a crew pushes the gun, deploys it and fires;
- **the 53-K pit** `BTX_Rus53KPit`: an enemy emplacement (owner 2) that holds the same gun in a sandbag ring with four crew and the German Pak 40 pit's round handover drill;
- **the 53-K sector pit** `BTX_Rus53KSectorPit`: the same gun and crew in a small square pit, traversing only 45° either side of its forward (see [The sector pit](#the-sector-pit)).

The model is KADO-3D's (`D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/45mmAT_53K`).

## The team weapon

| | |
|---|---|
| Squad | `Squad_Rus_53K_45MM` ("53-K 45mm"), in the anti-tank list, costs the same as the PaK 38 squad (enemy cost 8) |
| Weapon | `M53K_45MM_TW` ("53-K"), the T-26's 45 mm data with a 4.5 s reload |
| Training | `MediumAntiTankGunTeamWeaponTrainingTime` |
| Blueprint helper | `URTSBlueprintFunctionLibrary::GetTeamWeapon_53K_45mm()` |

### Assets (`/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/53K_45MM/`)

| Asset | |
|---|---|
| `Mesh/SK_TW_53K` | on the **ZiS-3 skeleton** `SK_TW_Zis3_Skeleton`; materials `MI_53K` (M_MasterPainter1) and `MI_53K_Burnt` |
| `Mesh/SM_Destroyed_53K` | the wreck |
| `DeployPacked`, `WeaponAnims`, `WheelsAnims` | `Anim_53K_*`: Deployed, Packed, DeployedToPacked, 9 aim poses, Fire, WheelsFWD; `AO_53K`; the three montages |
| `AnimBp_TW_53K` | a copy of the ZiS-3 anim BP with every ZiS-3 asset swapped |
| `Rus_TW_53K_45mm` | the gun: fires from `Launch53K`, casings from `Out53K`, the T-26's 45 mm sound, loader at (−150, 72) and spotter at (−210, −82), turret height 76 |
| `Rus_TWSquad_53K_45mm` | the squad (`TeamWeaponClass` = the gun); registered in the spawner's `SetupRusSquads` |
| `DestroyedTW_53K` | the scavengeable wreck |

### Why the ZiS-3 skeleton

The 53-K has the same parts as the ZiS-3: wheels, split trails, an upper carriage and a recoiling barrel. The Blender rig uses the ZiS-3's bone names and hierarchy, so the mesh imports onto `SK_TW_Zis3_Skeleton`:

`37mmFlak` (armature root) → `Wheels` → `B_Root` → {`B_Turret` → `B_Mantlet` → `B_Gun`, `B_RightWheel`, `B_LeftWheel`, `B_RightLeg`, `B_LeftLeg`}

Each mesh keeps its own reference pose, so the bone positions may differ. As a result:
- the ZiS-3 anim BP, aim offset layout, montage slots and team weapon logic are reused as they are;
- the crew needs no setup, because the squad unit's crew montages fall back to the base anti-tank gun loader and spotter for a subtype without an override.

## The pit

| | |
|---|---|
| Expansion type | `BTX_Rus53KPit` ("53-K Pit") |
| Blueprint | `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit` |
| Health | `T3BxpBunkerHealth`, reinforced resistances |
| Cost | 320 radixite, 180 metal (`DeveloperSettings::GameBalance::UnitCosts::Bxps`) |
| Footprint | 1150 × 1150; the ring is 4.4 m inside and 5.7 m outside |

| Folder | Content |
|---|---|
| `Meshes/SM_53KPit` | the sandbag ring, `MI_Rus_SandBag_01`, complex collision |
| `Anims` | `AnimBp_53KPit` (parent `UGunPitAnimInstance`: yaw on `37mmFlak`, pitch on `B_Mantlet`) and `Montage_53KPit_Fire` |
| `Turrets/RTS_BP_EmbTurret_Rus53KPit` | the embedded turret: a copy of the Pak 40 pit's turret with the 53-K, Soviet crew, 45 mm rounds (the Pak rounds at 0.7 scale) and its fire montage in `ReloadSettings` |

### The sandbag ring

The ring is sandbags only: three rows thick and stepped down outwards (4, 3 and 2 courses, about 62 cm high), so the 76 cm bore fires over the rim. A 30° gap at the back is the crew's way in; turned bags close the ends of the rows.

The inner radius is set by the **ready crate**, not by the gun:
- the crate hangs on the gun's pit origin, so it turns with the gun;
- its far corner reaches about 3.95 m from the pivot (the trail spades reach 2.8 m).

At a 3.8 m inner radius the crate went into the bags; at 4.4 m it clears them at every yaw.

### The reload drill (reused from the Pak 40 pit)

The 53-K's breech is at (−72, 0, 74) cm in gun space and the Pak 40's at (−72, 0, 94), so the Pak pit's drill fits the 53-K as it is:
- the same loader and handler spots, handover spot and crate;
- the same baked handler animations (`Anim_GerPitPak_Handler_Idle/Reload`);
- the loader's `AN_Infantry_LoadPAK`.

Only these change:
- the soldiers are Soviet (`AnimBp_B4Pit_Crew`);
- the rounds are scaled to 45 mm;
- everything hangs on `PitOrigin53K`, an identity socket on `37mmFlak`.

The 53-K needs its own socket because the ZiS-3 skeleton is shared and the 53-K's root bone has no import scale to cancel. The ready crate's tag must name it: `PitSocket:PitOrigin53K`.

## Rebuilding

**Blender** (`D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/45mmAT_53K/Scripts`):
- `at53k_build.py` (Blender 4.0) does the following:
  - bakes the 8 source pieces (35 cm per source unit, turned +90°);
  - sorts loose parts into the ZiS-3 bones with bounds rules;
  - builds the actions and the wreck;
  - writes `T_53K_D` / `T_53K_ORM` and `at53k_rig_info.json`.
  - Import the FBX at scene scale 1.0, then switch to 0.01; importing straight into a 0.01 scene makes the mesh 100× too big.
- `at53k_pit_build.py` (Blender 4.0) lays the ring with the DShK pit's sandbag generator (`SovietsStuff/MGPit/Scripts/dshk_pit_build.py`) and writes `Exports/SM_53KPit.fbx`.

**Unreal** ([Tools/AT53K](../../Tools/AT53K)). Each script runs with the editor bridge on 127.0.0.1:9855; `uecp.py` must be importable.
1. `ue_53k_import.py` (copy `at53k_rig_info.json` next to it first) imports the following and prints the socket transforms:
   - the materials;
   - the mesh, onto the ZiS-3 skeleton;
   - the animations, with additive settings copied from their ZiS-3 twins;
   - the wreck.
2. Add the `Launch53K` and `Out53K` sockets with `add_skeleton_socket` and `set_skeleton_socket_transform`.
3. `build_53k_animbp.py` builds the montages, `AO_53K` and `AnimBp_TW_53K`.
4. `build_53k_teamweapon.py` builds the wreck, gun, squad and spawner row.
5. `build_53k_pit.py [mesh,socket,animbp,turret,bxp]` builds the pit.

The scripts reuse existing copies instead of deleting them. A windowless editor cannot show the "delete loaded Blueprint" dialog and dies on it.

## Testing

- **Squad:**
  - Command: `-B4HowitzerValidate -B4HowitzerSquad=/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/53K_45MM/Rus_TWSquad_53K_45mm.Rus_TWSquad_53K_45mm_C -B4HowitzerSquadReload=4.5`.
  - It checks deploy, pack–move–redeploy, attack-ground and the reload interval.
- **Pit:**
  - Command: `-At53KPitValidate` on TestCover. For pictures add `-GerPitCapture=<dir> -ExecCmds="r.TextureStreaming 0"`.
  - Setup: an enemy pit is ordered to attack the ground 60° off its forward.
  - It checks:
    - the crew and crate on `PitOrigin53K`;
    - the rounds;
    - the gun turned to the target before the first shot;
    - the crew turned with the gun;
    - the handler → loader → breech drill, with the hands within 34 cm of the round.
- **Turret fix found by this test:**
  - `ACPPTurretsMaster::SetEngageGroundLocation` did not set the yaw target. A turret still settling on its idle yaw could then report "rotated to engage" against the old yaw, and it fired once before turning.
  - The ground order now sets the yaw target at once, as `SetTarget` does.
  - `-GerPitValidate` and `-DShKPitValidate` still pass.

## Fire feedback

On every shot the gun bucks and the crew brace. See [GunPitFireFeedback.md](GunPitFireFeedback.md).

## The sector pit

A smaller, square pit for the same gun. Its turret only traverses **45° either side of the pit's forward**, so the pit covers one sector and must be placed facing it.

| | |
|---|---|
| Expansion type | `BTX_Rus53KSectorPit` ("53-K Sector Pit") |
| Blueprint | `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KSectorPit` |
| Health | the 53-K pit's (`T3BxpBunkerHealth`, reinforced resistances) |
| Cost | 260 radixite, 140 metal |
| Footprint | 920 × 920; the wall is 7.2 m deep (x −4.46 to +2.73 m from the pivot) and 9.0 m wide |

| Asset (in `Rus53KPit`) | |
|---|---|
| `Meshes/SM_53KSectorPit` | the square wall, `MI_Rus_SandBag_01`, complex collision |
| `Crew/Anim_53KSectorPit_Handler_Reload` / `_Idle`, `Crew/Montage_53KSectorPit_Handler_Reload` | the handler's compact drill |
| `Turrets/RTS_BP_EmbTurret_Rus53KSectorPit` | a copy of the 53-K pit turret with the arc, the new drill, crate and commander spot |

The gun, `AnimBp_53KPit`, `Montage_53KPit_Fire` and `PitOrigin53K` are shared with the round pit.

### The traverse arc

The arc uses the embedded turret's existing yaw limits plus one new flag:
- `InitEmbeddedTurret` is called with `bIsAssaultTurret` true and limits −45 / 45. The gun clamps to them and idles inside them.
- `AEmbeddedTurretsMaster::bIsFixedEmplacement` (class default, true on this turret) makes the limits a fixed arc:
  - no base turn is requested (a building has no hull to turn);
  - `GetCanTraverseToLocation` rejects targets outside the arc. `ACPPTurretsMaster` uses it to skip them in the target search and to drop an auto-engage target that leaves the arc.
  - With a fixed arc, the target search asks the game unit manager for the 8 closest targets instead of 3, so one inside the arc is found when the closest are behind the pit.
- An order onto a target outside the arc turns the gun to the nearer limit; it holds there and does not fire.
- The arc is relative to the gun mesh's yaw 0, which is the expansion's forward. Rotate the expansion to aim the sector.
- The weapon range arc shown when hovering the weapon uses the same 45° (`GetTurretYawLimit`).

### Why it can be small

With the gun held to ±45°, only that wedge is swept. The round pit's ready crate (far corner about 4 m out) sets its size, so the sector pit pulls the back of the drill in:
- the loader, the handover spot and the loader's animation stay the Pak pit's, so the handover is unchanged;
- the handler picks from (−200, 170) and waits at (−185, 160), just outside the left trail;
- the crate is `SM_TankAmmoCrates_A` at 1.15 scale (51 × 68 cm, top at 44 cm), centred on (−200, 216) with its narrow side to the handler;
- the commander crouches behind the spades at (−265, −70).

Everything on the gun then stays within 3.4 m of the pivot. Over ±45° it sweeps x −3.36 to +0.93 m (the muzzle reaches 1.46 m) and y −3.0 to +3.36 m. The wall's inner faces are 3.55 m behind and to the sides and 1.9 m in front.

The wall is two bag rows, stepped down outwards:
- the sides and back are 4 then 3 courses (72 cm at the top);
- the front, which the gun fires over, is 3 then 2 courses (46 cm), beyond the muzzle at every yaw;
- the lowest pitch is −5° (the round pit's −8°) so the shot clears the front wall;
- the rear entrance is 1.1 m wide, closed by turned bags.

### Rebuilding

**Blender** (`45mmAT_53K/Scripts`, Blender 4.0):
- `at53k_sector_pit_build.py` writes `Exports/SM_53KSectorPit.fbx` (the BS-3 pit's square-wall generator with separate front, back and side distances).
- `at53k_sector_crew.py` bakes the handler with the German pit solver (`gerpit_crew.py`, the Pak config with the new spots). It writes `Exports/Crew/`: the two animations, `at53k_sector_carry.json` and `at53k_sector_crew_layout.json`.

**Unreal:** `Tools/AT53K/build_53k_sector_pit.py [crew,mesh,turret,bxp]` (needs a C++ build with `bIsFixedEmplacement` and `BTX_Rus53KSectorPit`).

### Testing

- Command: `-At53KSectorPitValidate` on TestCover (pictures: `-GerPitCapture=<dir> -ExecCmds="r.TextureStreaming 0"`).
- First, an attack-ground order 80° off the pit's forward. The gun must stop at 45° (±2°), never pass it and not fire for 8 s. `GetCanTraverseToLocation` must reject that target and accept the next one.
- Then an order 30° off forward. The usual pit checks run: the gun and crew turn, it fires, the gun bucks and the crew brace, and the handover drill completes with the hands on the round.
