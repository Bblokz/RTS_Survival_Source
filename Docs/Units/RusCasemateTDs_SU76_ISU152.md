# SU-76 and ISU-152 (Soviet casemate tank destroyers)

Both are `RTS_BP_AssaultTank` children built from the SU-85 setup: a static hull, a static turret base (the fixed
gun mount) and an embedded turret child actor whose skeletal weapon system yaws inside the limits the vehicle passes
to `InitEmbeddedTurret`.

| | SU-76 | ISU-152 |
|---|---|---|
| Subtype | `Tank_SU_76` (existing) | `Tank_ISU_152` (appended to `ETankSubtype`) |
| Weapon | `ZIS_3_76MM_SU76` (existing) | `ML_20S_152MM_ISU152` (appended to `EWeaponName`) |
| Folder | `Rus_Light/SU76` | `Rus_Heavy/ISU152` |
| Tracks / wheels | T-70's: `SK_Pazner1_Tracks` + `AnimBp_Panzer1` | IS-1's: `SK_KV_1_Tracks` + `AnimBp_KV-1_Tracks` |
| Yaw limits | ±15° | ±8° |
| Pitch limits | -5° .. +25° | -3° .. +20° |
| Launch / impact VFX | T-34/76 main gun | E-100 main gun |

## Assets per tank

- `Meshes/SM_<T>_Hull`: complex collision from `SM_<T>_Collision` (two convex hulls: lower hull and superstructure),
  `CTF_USE_COMPLEX_AS_SIMPLE`, Nanite off.
- `Meshes/SM_<T>_TurretBase`: the fixed gun mount, attached to `Hull` as `TurretBase` with `NoCollision`.
- `Meshes/Weapon/SK_<T>_WeaponSystem`: SU-122 bone names (`B_Turret` yaw, `B_Mantlet` pitch, `B_Gun` recoil);
  the mesh origin is the gun pivot, so the `Turret` child actor sits at the pivot and `TurretHeight` is 0.
  Skeleton sockets `BulletLaunch0` (muzzle) and `BulletOut0` (casing).
- `Meshes/WeaponAnims`: TPose, additive Base/Left/Right/Up/Down (mesh-space rotation offset on TPose), Fire,
  `AO_<T>`, `Montage_Fire<T>`. `Meshes/Weapon/AnimBP_<T>` is the SU-122 anim BP retargeted to the new skeleton.
- `Materials/M_<T>_Base`: a copy of `M_IS_1` with the tank's diffuse and normal; `MI_<T>` (and `MI_ISU152_Camo`).
- `SetupArmor` in the vehicle BP holds plain armor boxes in hull space (historical thicknesses).

## Wrecks

`Env_Dest_<T>` (a copy of `Env_Dest_SU85`, `MySubtype` set) is what `BP_OnUnitDies` spawns. Its `Hull` is
`SM_<T>_Hull_Destroyed`: hull, turret base and the donor's running gear (frozen rest pose) in one mesh, sunk a few
centimetres; slot 0 `M_<T>_Destroyed` (a copy of `M_IS_1_Destroyed` with `T_<T>_Destroyed_D` and the tank's normal),
slot 1 `MI_RTS_Destroyed_Default` on the running gear. `Weapon` is `SM_<T>_Gun_Destroyed` at the gun pivot, posed
drooped and turned. Both have box collision. `burn_texture.py` makes the burnt diffuse from the tank diffuse;
`build_wreck.py` (configs `wreck_*.json`) builds the meshes from the `<Tank>_Split.blend` files.

## Rebuilding the meshes

`Source/Tools/CasemateTDs/build_td.py` (Blender 5.2, headless) reads the KADO source, bakes the fit onto the donor
tracks, splits hull / turret base / gun, builds the collision, rigs the gun, bakes the aim and fire animations and
writes FBXs plus `rig_info.json` (pivot and muzzle in Unreal centimetres):

```
Blender5.0.exe -b --factory-startup --python build_td.py -- su76.json
```

The fit matches the donor hull as placed in its blueprint: the SU-76 is scaled (0.9, 0.76, 0.9) to sit on the
Panzer I tracks the T-70 uses, the ISU-152 takes the IS-1 hull's 1.1 scale and offset. The parts are built in
centimetres and exported with `FBX_SCALE_NONE` and global scale 0.01; other unit settings import 100x too large or
small through Interchange. Re-importing a mesh means re-applying Nanite off, the collision mesh and the materials.
