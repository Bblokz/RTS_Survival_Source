# Farm tractors (natural cover, Ger_Trains)

Two 1930s Soviet tractors that infantry use as cover. When one dies it turns into a burnt wreck that is still cover.

| Blueprint | Tractor | Size | Health | Cover sockets |
|---|---|---|---|---|
| `BP_Cover_Tractor_STZ1` | STZ-1 (SKhTZ 15/30) wheeled tractor: steel spoked wheels with lugs, exposed engine, red wheels | 3.8 x 1.9 x 2.1 m | 1300 | 7: crouch behind each wheel (4) and behind each rear wheel from the back (2), standing_right at the radiator |
| `BP_Cover_Tractor_S65` | ChTZ S-65 "Stalinets" crawler with canopy | 3.7 x 2.5 x 2.6 m | 2000 | 10: three crouch per side along the tracks, standing + standing_right at the radiator and at the rear fuel tank |

Content lives in `/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/Tractors/`:
- `Meshes/SM_Tractor_<N>` and `SM_Tractor_<N>_Wreck`. Each carries its UCX collision (12 hulls for the STZ-1, 9 for
  the S-65) and the same cover sockets.
- `Textures/T_Tractor_<N>_{D,N,ORM}` and `_Burnt_{D,ORM}`. These are 2048 maps on unique UVs; ORM is R = AO,
  G = roughness, B = metal.
- `Materials/M_FarmTractor`, plus `MI_Tractor_<N>` and `MI_Tractor_<N>_Burnt`.

Two of them are placed in Ger_Trains as `FarmTractor_STZ1` and `FarmTractor_S65` (folder `FarmTractors`), in the farm
yard next to the farm buildings.

## How it works
- **Class chain.** `BP_Cover_TractorMaster` is a child of `BP_NaturalCover`. The cover subsystem finds the tractors
  through the `BP_NaturalCover` entry in `M_SocketCoverActorClasses` and turns every `MainMesh` socket into a cover
  point (see `Source/DesignTexts/InfantryCoverSystemDesign.md`, "Cover from sockets").
- **Sockets.** Each socket is the soldier's feet at Z = 0, 40-55 cm outside the hull, with +X facing the tractor.
  - The peek side of a standing socket is trusted, not checked, so each one sits 20-25 cm inside the edge it peeks
    past.
  - Facing the tractor, a soldier's right is `Up x Forward`, so `standing_right` goes on the side where the edge is
    to his right.
  - Standing points are only used where the obstruction is at least 145 cm tall: the radiators and the S-65 fuel
    tank. Everything else is crouch, because wheels and tracks are 90-140 cm tall.
- **Master graph.**
  - BeginPlay runs in this order: parent BeginPlay (`SetupComponentCollisions`), then
    `HealthComp.SetMaxHealth(MaxHealth)`, then `SetupCrushDestructionOverlap(MainMesh, HeavyTank, RegularDeath)`.
  - `BP_OnUnitDies` calls `CollapseMeshWithSwapping`. It swaps `MainMesh` to `WreckMesh`, plays `WreckExplosionFX`
    (`NS_TankExplosion_1_Large`) and `WreckSound`, and attaches `WreckFireFX` (`NS_Fire_Tank_1`) at `FireOffset`.
  - The actor is not destroyed, so its cover points stay. The wreck has the same footprint and sockets.
- **Navigation.**
  - `NavModifier` uses `RTS_NavArea_BlockCharLightMedium`, with `failsafe_extent` set to the mesh half extents. The
    failsafe box rotates with the actor.
  - `Overlap Tanks` is on, so tanks never collide with the tractor. Infantry, light tanks and medium tanks path
    around it through their RTS nav filters. A heavy tank drives through it and crushes it, which is the death above.
    This is the same setup as the dead-tree logs.
- **Verification (2026-10-27, Simulate on Ger_Trains).**
  - With `LogRTSCoverFinder Verbose`, all 17 sockets logged `RTS_COVER_SOCKET ... on_navigation=1` with 0 cm offset,
    and none were skipped.
  - `TriggerDestroyActor` swapped the S-65 to `SM_Tractor_S65_Wreck` with the fire attached.
  - Path tests across each tractor used the per-agent filter (`RTS_NavFilter_*`):
    - The character path ran 1119 cm against 1000 cm straight; light and medium tanks also went around.
    - The heavy-tank path ran straight through.
  - Without a filter, the paths run straight through, because the area's cost is 1.

## Rebuilding
The pipeline is in `D:/Blender/BlenderProjects/BlenderTanks/FarmTractors/` (see its README):
1. `blender -b -P build_tractors.py -- STZ1|S65` (Blender 4.0, Cycles on CPU).
   - It builds the parts (`tractors.py`, `tr_lib.py`), unwraps them, and builds the wreck from the same UVs.
   - It exports FBX with `UCX_` hulls and writes `tractor_<N>.json` with the sockets in UE axes.
   - It also bakes position, normal, material class, convex edge and AO maps.
2. `python paint_tractors.py STZ1 S65` paints D/N/ORM and the burnt D/ORM from those maps.
3. Copy `ue_import_tractors.py` to `Saved/FarmTractors/` and run `import_all()` in the editor.

## Gotchas
- **UCX hulls and the importer.** UE 5.5 Interchange drops `UCX_` hulls and auto-generates one convex hull.
  `ue_import_tractors.import_mesh_legacy` imports the meshes through the legacy FBX importer instead: it sets
  `Interchange.FeatureFlags.Import.FBX 0` and restores it afterwards.
- **Blender export.** Hidden objects cannot be selected, so they silently miss a `use_selection` FBX export. The UCX
  objects are unhidden before every export.
- **Path tests need the RTS filters.** `RTS_NavArea_Block*` areas only block through the RTS nav filters, so a path
  test without a filter shows nothing.
- **Dust and the farm ground.** The paint's dust is the farm ground colour. Too much of it made the tractors vanish
  under the map's sun, so the painter keeps it light. The painted base colours are already darker for that sun.
