# Farm tractors (natural cover, Ger_Trains)

Two 1930s Soviet tractors that infantry use as cover. Medium and heavy tanks crush them. When one dies (crushed or shot)
it turns into a burnt wreck, explodes, sinks into the ground and is removed.

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
    `HealthComp.SetMaxHealth(MaxHealth)`, then
    `SetupCrushDestructionOverlap(MainMesh, MediumOrHeavyTank, RegularDeath)`. A crush therefore goes through the
    normal death (`OnUnitDies` -> `BP_OnUnitDies`), the same as being shot.
  - `BP_OnUnitDies` runs two steps:
    1. `CollapseMeshWithSwapping` swaps `MainMesh` to `WreckMesh` (burnt material) and attaches `WreckFireFX`
       (`NS_Fire_Tank_1`) at `FireOffset`.
    2. `VerticalDestruction` plays `WreckExplosionFX` (`NS_TankExplosion_1_Large`) and `Cue_Explosion_GeneralMedium`
       at `FireOffset`. It then sinks the actor `CollapseDeltaZ` (STZ-1 240, S-65 290, master default 260) over
       `CollapseTime` (5 s), with up to 6 deg of yaw and 3 deg of pitch/roll, and destroys it.
  - Once the actor is destroyed, the cover subsystem drops its socket points (`RemoveSocketCoverOfDestroyedActors`).
- **Navigation.**
  - `NavModifier` uses `RTS_NavArea_BlockCharLight`, with `failsafe_extent` set to the mesh half extents. The
    failsafe box rotates with the actor.
  - `Overlap Tanks` is on, so tanks never collide with the tractor. Infantry and light tanks path around it through
    their RTS nav filters. Medium and heavy tanks drive through it and crush it, which triggers the death above.
    This matches the area and crush pairing of `BP_Cover_Log1`.
- **Verification (2026-10-27, Simulate on Ger_Trains).**
  - With `LogRTSCoverFinder Verbose`, all 17 sockets logged `RTS_COVER_SOCKET ... on_navigation=1` with 0 cm offset,
    and none were skipped.
  - Crush test on 2026-10-28, with tanks spawned and teleported onto the tractors:
    - A T-34/76 (medium) killed the STZ-1, and a Tiger II (heavy) killed the S-65.
    - A Panzer II F (light) left the S-65 alive.
    - Both crushed tractors exploded, sank (the S-65 went from z 144 to 125 within 0.4 s) and were destroyed within
      about 10 s.
  - Path tests across each tractor used the per-agent filter (`RTS_NavFilter_*`), against 1000 cm straight:
    - Character 1119 cm and light tank 1278 cm: both go around.
    - Medium tank 1001 cm and heavy tank 1010 cm: both go straight through.
  - Without a filter, the paths run straight through, because the area's cost is 1.
  - Do not run that filtered path query from editor Python during Simulate. On 2026-10-28 it hit an engine ensure
    (`NavFilter_AIControllerDefault`: no AI controller) and the editor then crashed when Simulate stopped.

## Rebuilding
Blueprint defaults (health, collapse depth and time, nav area and extents, meshes) can be re-applied with
`ue_tractor_bp_defaults.apply()` from the pipeline folder. The master graph is described under How it works.

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
