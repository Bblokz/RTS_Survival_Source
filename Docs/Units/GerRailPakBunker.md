# German rail Pak bunker (breakthrough doctrine)

A concrete pillbox whose sandbagged roof is the gun pit of a Pak 40 refitted with rails: a smaller version of the Tiger H1 rail gun.

| | |
|---|---|
| Expansion type | `BTX_GerRailPakBunker` ("Rail Pak Bunker") |
| Blueprint | `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/RailPakBunker/Ger_BXP_RailPakBunker` |
| Weapon | `RailGun50MM` through `SetupRailgunWeapon` (rail projectile, over-penetration), 50 mm rail launch sound |
| Health | `BxpHeavyBunkerHealth` × 1.8, reinforced resistances (the Pak 40 pit is × 0.9) |
| Cost | 850 radixite, 420 metal |
| Build time | `GerRailPakBunkerBuildTime` (40 s × game time) |
| Footprint | 1250 × 1250 |
| Where | Field defense of the German HQ, **breakthrough doctrine only** (`InitDefaultBreakthroughLayout` adds the slot to the shared HQ layout). It needs the communication center, as the howitzer pits do. |

## Assets (`GerPits/RailPakBunker`)

| Asset | |
|---|---|
| `Mesh/SK_Pak40Rail` | the Pak 40 with the Tiger rails, on `Sk_FullAnimPak40TW_Skeleton`; materials `M_Pak40`, `Pak_40_Wheel_D_Mat`, `MI_Tiger1H_PostApo_Rail` |
| `Meshes/SM_GerRailPakBunker` | the pillbox, 20.3k triangles (cut), Nanite off, complex collision |
| `RTS_BP_EmbTurret_GerRailPak` | a copy of the German Pak 40 pit turret: the rail Pak, `SetupRailgunWeapon` in place of `SetupProjectileWeapon` |
| `Ger_BXP_RailPakBunker` | a copy of the Pak 40 pit expansion; the turret stands on the roof deck (z 254) |
| `Meshes/SM_RailSlug`, `Materials/M_RailSlug_Metal`, `Materials/M_RailSlug_Glow` | the rail slug the crew hand along and the spares lying about (see [Rail slugs](#rail-slugs)) |
| `GerPits/UI/T_BxpOption_GerRailPakBunker`, `SB_OptionImage_GerRailPakBunker` | the build option picture; row `Train_BTX_GerRailPakBunker` in `DT_TrainingOptions` |

The spawner (`RTS_BP_RTSAsyncSpawner`) maps the type to the expansion class and to the bunker mesh as its placement preview.

## The gun

The rail Pak keeps everything of the Pak 40 except its barrel tube:
- the breech and recoil sleeve stay up to just ahead of the shield (x 40 cm);
- the Tiger rail turret's two toothed rails and their stub replace the tube. They are scaled to 0.4, so they reach the old muzzle at x 248 cm and measure 27 × 13 cm in section. They are skinned to `B_Gun`, so they recoil and elevate with the breech.

It shares the Pak 40 skeleton, so these are the German Pak 40 pit's, unchanged:
- `AnimBp_GerPit_Pak40` (yaw, pitch, fire kick);
- the fire montage;
- `PitOrigin`;
- the whole crew drill: four crew, a handler handing each round to the loader, the ready crate.

**Build it from Unreal's own export of the Pak 40** (`GerDefenses/Assets/Sk_FullAnimPak40TW.FBX`, written by the Unreal FBX exporter). Do not use the KADO export in `(KADO-3D)/(EXPORTS)/Pak40`.
- The game's asset does not match the KADO file: every bone's axes are permuted and the cradle is bound 1.7° apart.
- A mesh built on the KADO file imported fine and passed every pit test, but under the Pak 40 animations its trails stood on end. The tests read only `PitOrigin`, so they cannot see this. Look at the pictures.
- Unreal's export is read by Blender with the root bone as the armature object. It must be written back with:
  - `armature_nodetype='ROOT'`;
  - the file's axes (`axis_forward='Y'`, `axis_up='Z'`).

  Then the bone nodes match the asset exactly. Check with an FBX node dump.

## Rail slugs

The crew handle 50 mm rail slugs, not the Pak's 75 mm rounds.

**The slug** (`rail_slug_build.py`, 84 × 9 cm, 336 triangles):
- a dark steel sabot body with three raised coil bands;
- a glowing dart tip and a glowing tail cap.

It has the 75 mm round's frame (axis +X, origin at its middle), so the drill's hand poses close on it unchanged.

**The glow** (`M_RailSlug_Glow`):
- The emissive is purple (0.45, 0, 1) at a strength of 1.5, pulsing ±0.5 three times a second.
- It is kept that low on purpose. At strength 4 or more, the tone mapper turned the bands lavender-white (median capture pixel 227, 159, 244). At 1.5 they read purple (197, 65, 212).
- `M_RailSlug_Metal` is plain dark metallic steel.

**Where the slugs are:**
- `Round1` / `Round2`: the two rounds the handler fetches from the ready crate and hands to the loader.
- `SpareSlug1`, `SpareSlug2`: lying on the ready crate either side of the one the handler picks.
- `SpareSlug3`, `SpareSlug4`: on the deck between the trails, by the spotter.
- The four spares are tagged `PitSocket:PitOrigin`, so they turn with the gun like the crate.
- The bunker mesh has a plank pallet with two layers of slugs (6 and 5) by the door. It uses slots `RailSlug_Metal` / `RailSlug_Glow` with the same materials.

New materials have no shaders for the game's renderer until a run compiles them. The first `-RailPakBunkerValidate` run after a material change captures grey slugs; the second one shows the glow.

## The bunker

Built by `railpak_bunker_build.py` on `gerpit_build.py`'s pieces:

- **The block:** an octagon, battered from 10.8 m across at the foot to 9.6 m at the 2.5 m roof.
  - It has a 22 cm concrete lip round the roof.
  - Its walls are poured lifts of tiling bunker concrete: slot `GerBunker_Concrete`, `MI_Bunker_Concrete_Wall_2x2` in Unreal, one UV unit per 2 m.
  - The German pit atlas's concrete quarter is not usable here. Its material lerps metallic toward 0.6 and darkens the colour there, so the walls went black away from reflections.
- **The roof:** the Pak 40 pit, lifted onto the roof. That is its plank deck and its ring of German loose sandbags: inner radius 4 m, 3 courses, the crew gap at the back.
- **Around it:**
  - a steel ladder up the back wall to the crew gap;
  - an entrance vestibule with a steel door and frame;
  - steel embrasure plates (two in front, one in each side);
  - a radio mast with guy wires on the roof;
  - fuel drums, ammo boxes, and sandbag cheeks at the door.

The block, ladder, vestibule, plates, mast and drums are wound outwards by hand. They skip `gerpit_build`'s normal recalculation, which turns the open concrete shell inside out. The pit, bags and ammo boxes go through it as in the pits.

The sandbags and ammo props are cut for non-Nanite rendering (see [GunPitMeshes.md](GunPitMeshes.md)): from 47.4k to 20.3k triangles.

## Rebuilding

**Blender** (`CustomMapItems/GerDefenses/RailPakBunker/Scripts`, Blender 4.0):
1. `pak40rail_build.py` writes `Exports/SK_Pak40Rail.fbx`.
2. `railpak_bunker_build.py` writes `Exports/SM_GerRailPakBunker.fbx`.
3. `Marketplace/Scritps/PitOptimize/pit_mesh_optimize.py -- <export>/SM_GerRailPakBunker.fbx GerPit_Sandbag=0.4,GerPit_Ammo=0.5` writes `SM_GerRailPakBunker_Opt.fbx`.
4. `railpak_bunker_icon.py` writes `GerDefenses/Exports/Icons/T_BxpOption_GerRailPakBunker.png`.

**Unreal:** `Tools/GerPits/build_railpak_bunker.py [gun,mesh,turret,slug,bxp,register,icon]`.
- Export the slug first with `rail_slug_build.py` (Blender 4.0, writes `Exports/SM_RailSlug.fbx`).
- The `slug` step builds the two slug materials and imports the slug. It turns the drill rounds into slugs and adds the four spares.
- The `turret` step swaps the copied Pak pit turret's `SetupProjectileWeapon` for `SetupRailgunWeapon`. It splits the `RailgunWeaponParameters` pin and feeds its `ProjectileWeaponParameters` from the turret's existing projectile struct.
- `register` and `icon` save the spawner and `DT_TrainingOptions`. The user's editor must be closed for those saves.

## Testing

- Command: `-RailPakBunkerValidate` on TestCover (pictures: `-GerPitCapture=<dir> -ExecCmds="r.TextureStreaming 0"`).
- It runs the Pak 40 pit checks on the bunker roof: the crew and crate on `PitOrigin`, the gun and crew turning to a ground target 60° off, two shots, the gun bucking and the crew bracing, and the full handover drill.
- All 25 checks pass.
