# Field kit: MG 42 hatch gunner

Built 2026-11-26. This is the first **field modification kit**. A kit is a technology the player researches in a
building expansion during a mission. It is never on the campaign tech tree and never saved. Once researched, every
eligible tank already on the field gets a scavenger with an MG 42 in a hatch, and so does every one built afterwards.

## Where to research it

Light Steel Forge → **Radixite Reactor** (`Ger_Bxp_LFactoryRadixiteReactor`), component `HatchMG42KitResearch`.

- Available to both German factions.
- Slot 10.
- Costs 900 radixite and 250 vehicle parts.
- Research button: `Buttons_DataTableb` → `IdResearchTechnology`, key 25.
- The icon is a placeholder (`T_TechIcon_InfantryKit`).

## Tanks

The owner picked these eight. The Pz III J and Pz IV F1 are left out on purpose. Variants inherit the base tank's
`KitGunnerMount`.

| Tank (variants) | Gunner stands on (mesh, socket) |
|---|---|
| Pz III M (Overcharged) / Pz III Flamm | cupola: `SM_Collision_PanzerIII_Turret`, `KitGunner_PzIII` |
| Pz III M SpacedArmor (Panzerschreck) | cupola: `SM_PanzerIIIM_SchreckShield` (the Schreck crew stand front right) |
| Pz IV G / Pz IV H | cupola: `SM_Collision_PanzerIV_Turret`, `KitGunner_PzIV` |
| Pz IV G DoubleTurret | front turret: `SM_Collision_PzIVG_DT_FrontTurret` |
| Pz IV H StandardPlus | cupola: `SM_Collision_PzIVF1_Spotter_Turret`. He **replaces** the variant's MG 34 gunner (`CupolaMount.UnmountOnTechnology`) |
| Pz IV H Ostwind | engine deck: `SM_PanzerIV_H`. The open flak turret is in `TurretClassesWithoutMount` |
| Pz 38(t) (ATAmmoStock, Rocketeer) / Pz 38(t) Railgun (Overcharge, Sniper) | cupola: `SM_Collision_Turret_Pz38T`, `_ATAmmoStock`, `_Rocketeer` collision, `KitGunner_38t` |
| Pz 38(t) R (PrecisionMissile) / IncendiarySalvo | engine deck: `SM_Hull_PZ38_T` / `SM_Hull_Pz38tRockets_IncendiarySalvo`. The rocket rack has no hatch |
| Jaguar / Jaguar Flanker | `SM_Ger_JaguarTurretBase` / `SM_Jaguar_Flanker_TurretBase`, `KitGunner_Jaguar` |

Sockets mark the gunner's feet, and the kit mounts add no offset (`MountLocalOffset` 0). On cupolas the socket sits
80 below the hatch top, so only his upper body shows. On decks it sits on the deck. The positions were computed from
the collision meshes' vertices; for the Pz IV it matches the existing `Infantry` socket minus 80.

**Variants that set their own meshes at runtime** (the four variants in the second table row group above, plus the
IncendiarySalvo, ATAmmoStock and Rocketeer) need the family socket on their own mesh. The mount looks on the turret
meshes first, then on the hull, so a variant without the socket on either gets no gunner and logs
"found no ... with socket". The test's `RTS_FIELDKIT_TEST MESH` lines list a tank's runtime meshes.

## Pieces

- **C++ `UEmbeddedTurretMountComponent`.** New settings and functions:
  - `RequiredTechnology`: the mount waits for this kit.
  - `UnmountOnTechnology`: this kit removes the mounted gunner.
  - `RequestMount()`: safe to call more than once and before BeginPlay.
  - `Unmount()`: removes the gunner for good.
  - `TurretClassesWithoutMount`: turret classes the mount never searches.
  - Mount mesh `TankTurretThenHullMesh`: any turret mesh with the socket, else any hull mesh.
- **`UTE_EmbeddedGunnerKit`** (`TechTree/Technologies/FieldKits`). It unmounts the replaced gunners, then mounts
  the kit gunners. Its Blueprint is `TEBP_Ger_FieldKitHatchMG42` (`TechTree/GerTechs/FieldKits`), and it is mapped
  in `RTS_BP_PlayerController` → `GetPlayerTechs`.
- **`ETechnology::Tech_FieldKit_HatchGunnerMG42`** and `Global_GetIsFieldModificationKit`. Kits are refused by
  `UWorldCampaignProgressManager::AddResearchedTechnology` and skipped in `UPlayerProfileLoader::ApplyTechnologies`
  (campaign and card technologies), so a kit can never become free for good.
- **Gunner `Ger_EIT_MG42_Scavenger`.** A copy of the MG 34 scavenger that uses the twin MG 42 pit's weapon:
  `Ger_PitMG42_7_92MM`, 6–10 round bursts 0.055 s apart, sound `Cue_MG42_GerTank_11Burst`. **He still carries the
  MG 34 model**, because there is no MG 42 static mesh yet.

## Tools (`Source/Tools/FieldKits`)

- `setup_field_kit_mg42.py`: idempotent. Steps `gunner sockets mounts effect buttons research techmap`.
  - Run it in a headless DebugGame editor with `-DisablePlugins=Fab,BpGeneratorUltimate`. With the UECP plugin
    loaded, a save of `Buttons_DataTableb` crashed (the bridge's asset-registry task ran during serialization).
  - Saves fail while another editor holds the files; close it first.
- `run_field_kit_test.sh [capture]`: runs `UFieldKitGunnerTestSubsystem` (`-FieldKitGunnerValidate`) on
  RTS_ToCopySmall. `capture` renders offscreen and saves 3 pictures per tank to `Saved/FieldKits/Capture`; check
  `nvidia-smi` first.
- `dump_kit_hosts.py`, `dump_variant_turrets.py`, `dump_mesh_vertices.py`: read-only dumps used for the socket
  positions.
  - Python's subobject gather returns the **parent's** template for inherited components, so variant overrides don't
    show up there.
  - EditorActorSubsystem spawning crashes under `-nullrhi`.

## Test

2026-11-26: **PASS, 241 checks.**

- Each tank has one kit mount and no kit gunner before the research. The StandardPlus has its MG 34 gunner.
- The research goes through the tech manager.
- Afterwards each of the 20 tank Blueprints (bases and variants) has exactly one MG 42 gunner on the expected mesh.
  He is registered in the tank's `Turrets`, has one weapon, and has no collision or navigation.
- The StandardPlus's MG 34 gunner is gone.
- The Pz III J and Pz IV F1 have none.
- A Pz 38(t) built after the research gets one.
- A repeated mount request adds nothing.
- Every gunner fires on an attack-ground order.

Not yet checked: how the placements look. The picture run (`capture`) is still to do, so the positions are computed
from mesh data only.
