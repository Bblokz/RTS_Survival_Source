# EWeaponName snapshot check
- rows: 34
- packages in snapshot: 7
- packages in grep inventory: 7

## Rows by kind
- Pin / In: 11
- Pin / Out: 9
- Property / -: 8
- Variable / -: 6

## 1. Inventory assets with no snapshot row (0)

## 1b. Snapshot packages not in the grep inventory (0)

## 2a. Snapshot packages with no recorded weapon value at all (0)

## 2b. Weapon-looking assets with no restorable pin literal (5 of 7)
These hold the weapon elsewhere (CDO variable, struct pin, function library); Apply does not restore them as written.

- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/AA/Ger_Turret_Sdkfz140_AA: struct-pin values 0; rows {'Pin/In': 3, 'Pin/Out': 3, 'Variable/-': 1, 'Property/-': 1}
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/Bunker/Ger_EmbTurret_Sdkfz140_Bunker: struct-pin values 0; rows {'Pin/In': 2, 'Pin/Out': 2, 'Variable/-': 2, 'Property/-': 2}
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PZII_Turret/Radixite/Ger_Turret_PZ_II_F_Radixite: struct-pin values 0; rows {'Pin/In': 1, 'Pin/Out': 1, 'Variable/-': 1, 'Property/-': 1}
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/DualBarrel/Ger_Turret_Puma_DualBarrel: struct-pin values 0; rows {'Pin/In': 2, 'Pin/Out': 2, 'Variable/-': 1, 'Property/-': 1}
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/RadixiteSniper/Ger_Turret_Puma_RadixiteSniper: struct-pin values 0; rows {'Pin/In': 1, 'Pin/Out': 1, 'Variable/-': 1, 'Property/-': 1}

## 3. Unresolved values (0)

## 4. Stale or redirected names (0)

## 5. Wired weapon pins (manual fix list, 9)
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PZII_Turret/Radixite/Ger_Turret_PZ_II_F_Radixite | EventGraph | Make Init Weapon State Projectile.WeaponName <- Get RadixiteWeaponName.RadixiteWeaponName
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/RadixiteSniper/Ger_Turret_Puma_RadixiteSniper | EventGraph | Make Init Weapon State Laser.WeaponName <- Get SniperWeaponName.SniperWeaponName
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/DualBarrel/Ger_Turret_Puma_DualBarrel | EventGraph | Make Init Weapon State Projectile.WeaponName <- Get DualWeaponName.DualWeaponName
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/DualBarrel/Ger_Turret_Puma_DualBarrel | EventGraph | Make Init Weapon State Projectile.WeaponName <- Get DualWeaponName.DualWeaponName
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/Bunker/Ger_EmbTurret_Sdkfz140_Bunker | EventGraph | Make Init Weapon State Projectile.WeaponName <- Get GunWeaponName.GunWeaponName
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/Bunker/Ger_EmbTurret_Sdkfz140_Bunker | EventGraph | Make Init Weapon State Arch Projectile.WeaponName <- Get MortarWeaponName.MortarWeaponName
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/AA/Ger_Turret_Sdkfz140_AA | EventGraph | Make Init Weapon State Projectile.WeaponName <- Get DrillingWeaponName.DrillingWeaponName
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/AA/Ger_Turret_Sdkfz140_AA | EventGraph | Make Init Weapon State Projectile.WeaponName <- Get DrillingWeaponName.DrillingWeaponName
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/AA/Ger_Turret_Sdkfz140_AA | EventGraph | Make Init Weapon State Projectile.WeaponName <- Get DrillingWeaponName.DrillingWeaponName

## 6. Other manual-fix rows (Switch, variables, BP function signatures, literals; 6)
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PZII_Turret/Radixite/Ger_Turret_PZ_II_F_Radixite | Variable |  | Blueprint |  | RadixiteWeaponName = 
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/RadixiteSniper/Ger_Turret_Puma_RadixiteSniper | Variable |  | Blueprint |  | SniperWeaponName = 
- /Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/DualBarrel/Ger_Turret_Puma_DualBarrel | Variable |  | Blueprint |  | DualWeaponName = 
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/Bunker/Ger_EmbTurret_Sdkfz140_Bunker | Variable |  | Blueprint |  | GunWeaponName = 
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/Bunker/Ger_EmbTurret_Sdkfz140_Bunker | Variable |  | Blueprint |  | MortarWeaponName = 
- /Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/AA/Ger_Turret_Sdkfz140_AA | Variable |  | Blueprint |  | DrillingWeaponName = 
