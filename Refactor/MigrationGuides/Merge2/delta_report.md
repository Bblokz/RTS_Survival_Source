# EWeaponName snapshot check
- rows: 11
- packages in snapshot: 3
- packages in grep inventory: 3

## Rows by kind
- Pin / In: 4
- Pin / Out: 3
- Property / -: 3
- Variable / -: 1

## 1. Inventory assets with no snapshot row (0)

## 1b. Snapshot packages not in the grep inventory (0)

## 2a. Snapshot packages with no recorded weapon value at all (0)

## 2b. Weapon-looking assets with no restorable pin literal (2 of 3)
These hold the weapon elsewhere (CDO variable, struct pin, function library); Apply does not restore them as written.

- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz_234_3/IncendiarySupport/EmbTurret_Sdkfz_234_3_IncendiarySupport: struct-pin values 0; rows {'Pin/In': 2, 'Pin/Out': 2, 'Variable/-': 1, 'Property/-': 2}
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz_234_3/StummelAssault/EmbTurret_Sdkfz_234_3_StummelAssault: struct-pin values 0; rows {'Pin/In': 1, 'Pin/Out': 1, 'Property/-': 1}

## 3. Unresolved values (0)

## 4. Stale or redirected names (0)

## 5. Wired weapon pins (manual fix list, 3)
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz_234_3/StummelAssault/EmbTurret_Sdkfz_234_3_StummelAssault | EventGraph | Make Init Weapon State Projectile.WeaponName <- Get Weapon Name.Weapon Name
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz_234_3/IncendiarySupport/EmbTurret_Sdkfz_234_3_IncendiarySupport | EventGraph | Make Init Weapon State Arch Projectile.WeaponName <- Get FlameWeaponName.FlameWeaponName
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz_234_3/IncendiarySupport/EmbTurret_Sdkfz_234_3_IncendiarySupport | EventGraph | Make Init Weapon State Arch Projectile.WeaponName <- Get FlameWeaponName.FlameWeaponName

## 6. Other manual-fix rows (Switch, variables, BP function signatures, literals; 1)
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz_234_3/IncendiarySupport/EmbTurret_Sdkfz_234_3_IncendiarySupport | Variable |  | Blueprint |  | FlameWeaponName = 
