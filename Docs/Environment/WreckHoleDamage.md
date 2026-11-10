# Wreck shell holes (2026-11-09)

Vehicle wrecks (`Env_Dest_*`) and scavengeable vehicle parts (`ScavObj_*`) show procedural holes where shells of
sufficient calibre hit them, and metal shards fly off as pooled Niagara debris. It is option B of
`WallHitMasking.md` (the procedural wall holes of `WallChunkDamage.md`) on any number of meshes per actor. The wreck's
own death (vertical sink, geometry collection, scavenging) is not changed.

## Pieces

| Piece | Where |
|---|---|
| `UWreckHoleDamageComponent` | `Environment/DestructableEnvActor/WreckHoleDamage/`. One per wreck hierarchy, named `WreckHoleDamage`, on the family roots (see Rollout). `ADestructableEnvActor::TakeDamage` forwards every hit while the wreck lives. |
| `UWreckHoleMaterialMap` | Same folder. `DA_WreckHoleMaterials` in `/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckHoles/`: opaque slot material -> masked `_WreckHoles` material, shared by every wreck. No Blueprint lists materials. |
| Materials | `Tools/WreckHoles/make_wreck_holes_material.py`. `<Master>_WreckHoles` (masked, two-sided copy of an opaque master with the hole Custom node) and `<MI>_WreckHoles` (copy of a slot instance reparented onto it), each next to its source. |
| Char overlay | `M_WreckChar_Overlay` (translucent overlay of charred wrecks) multiplies its opacity by the same hole cut, so the char does not cover a hole. |
| Debris | `NS_AddOnArmorPanelDebris` (unchanged) with `SM_WreckHoleShard_0..3` (engine-deck plates and chunks in `MI_WreckHoleShard`, burnt `M_WreckDebrisChunk`). |
| Rollout | `Tools/WreckHoles/build_wreck_holes.py`: materials for every wreck slot, overlay patch, shards, map, component on the roots. |

## Behaviour

- **Which meshes.** Every static mesh of the owner with at least one slot material in the map (`MeshComponentNames`
  empty), or only the named ones. Each mesh has its own 6 holes in custom primitive data 11..34 (mesh X, Y, Z, radius).
- **Where.** The shell's reported impact is on the collision; the component traces the struck mesh's complex
  collision along the shot (150 cm before and behind) for the visible surface and its normal. Splash (no point event)
  traces from the explosion to the nearest mesh. Meshes without query collision fall back to their local bounds.
- **Size.** Calibre x 0.4 cm (37 mm 15 cm, 75 mm 30 cm, 152 mm 61 cm), at most 90 cm. Below 30 mm nothing happens.
  Hits without a projectile: sqrt(damage) x 2.5 cm. Splash further from the surface makes a smaller hole.
- **Growth.** A hit inside a hole widens it. A seventh hole merges the oldest with its nearest neighbour.
- **Health cap.** While alive, at most lost health x 0.5 x the meshes' bounds surface is open. The killing hit adds
  nothing: the wreck's death takes over and the holes stay on the meshes while they sink.
- **Materials.** An intact wreck keeps its opaque materials. The first hole on a mesh async-loads its `_WreckHoles`
  materials and swaps them in (a frame or two later; the mask is already written). Slots without an entry (camo nets,
  foliage, material-attribute masters) keep their material and show no hole. The masked copies are two-sided: through
  a hole you see the hull's inside, drawn charred black (masters that were already two-sided keep their back faces).
- **Shards.** One per 700 cm² removed, at most 8 per hit, thrown out of the struck face towards the shooter. None
  while fog of war hides the wreck.
- **Not visual:** collision, nav and line of sight do not change.

## Bridges and logs (Wood style, 2026-11-09)

- `BP_Industrial_RailRoadBridge` and `BP_Industrial_RailRoadBride90Deg` (timber trestles, `ADestructableEnvActor`)
  and `BP_Cover_DeadTreeLog_01..10` (`ADestructableActorHealth`, whose `TakeDamage` now calls
  `ForwardDamageToHoleComponents`) each carry the component with the `SM_WoodHoleSplinter_0..3` debris.
- Materials use the `Wood` style of the library (`make_damaged_material(source, 'Wood')`): `<source>_WoodHoles`,
  lightly scorched rim, dark-wood inside. Registered: `M_WeaponRack` (bridge pillars and deck) and
  `MI_Env_DeadTree_1` (logs). The bridge rail slot `M_RustedRefinery_Atlas` is not registered, so rails never hole.
- Bridges: `MeshSpaceHoleZones` holds one box up to z 1150 (mesh space), so holes stay on the pillars below the deck
  beams and rails (a hit at rail height was refused in PIE). Holes are smaller there (0.25 cm per mm, at most 45 cm),
  because the legs are about 40 cm thick.
- Set up by `build_extra_targets()` in `build_wreck_holes.py` (`EXTRA_TARGETS`).
- PIE: debug hits holed the logs and bridge legs; 8 tanks firing at a bridge cut 6 holes through `AProjectile`.
  The bridges' `AimOffsetPoints` (class defaults) were three points at the actor origin, so every shell hit the end
  leg's base. They now hold one point per trestle leg at the leg's centre, 350/450 cm up (straight bridge: x -12, 988,
  1988 at y 156 / -234; 90-degree bridge: its six legs). In PIE ten tanks then hit the legs, filled all 6 hole slots
  and kept cutting new holes.

## Adding a wreck

A new `Env_Dest_*` or `ScavObj_*` that derives from a family root inherits the component. Build its materials and
register them:

```python
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WreckHoles/build_wreck_holes.py', encoding='utf-8').read())
build_materials(['/Game/.../Env_Dest_New'])   # writes Saved/WreckHoles/build_report.json for these wrecks
register_all()                                 # adds the report's pairs to DA_WreckHoleMaterials (keeps the others)
```

`build_materials()` without arguments covers every wreck again (existing copies are reused, so it only builds what is
missing) and is the safe default. A wreck outside the roots needs `setup_blueprint(path, build_debris())`.

## Checks (PIE on `UT_VehicleObstacle`, 2026-11-09)

- `Tools/WreckHoles/pie_validate_all_wrecks.py`: all 162 `Env_Dest_*` / `ScavObj_*` classes spawned, two 88 mm debug
  hits per visible mesh. Every class tracked 1-7 meshes, cut 2-9 holes and swapped the hit meshes to `_WreckHoles`
  materials; 0 failures (`Saved/WreckHoles/pie_validation.json`).
- Pictures (`Saved/WreckHoles/`): `shot_2.png` (T-34/76, deck and side holes), `shot_tiger2.png`, `shot_pziv.png`.
- Real fire: Panzerjäger I, T-26, E-100, Pz III AA and Tiger II tanks shooting an `Env_Dest_PZIV` (health raised to
  6000 in PIE): 7 holes through `AProjectile`, both meshes swapped (`realfire_1.png`). With its health lowered it died
  and was removed through its own Blueprint death; no errors.
- Build: 87 materials (51 masked masters, every one mask-live: PS +60 or more over its source); 6 slots skipped:
  4 masked camo nets / foliage (`MI_PzJager_AmbushNet`, `MI_JagdPanther_Pfeil_Foliage_Burnt`,
  `MI_GerCamoAtlas_WespeNestBurnt`, `M_GerCamoConcretePalet_Atlas`) and 2 Megascans metal sheets that use material
  attributes (`MI_Rusty_Painted_Metal_Sheet_*`). Those slots never show holes.
- **Not tested:** splash without a point event, fog of war, a packaged build (the map, materials and shards are hard or
  soft references from the family-root Blueprints, so they should cook), save/load (holes are not saved).

## Known limits

- Holes do not carry onto a wreck's geometry collection when it dies; a sinking mesh keeps them.
- Low-poly wreck meshes show the hole's outline along their facets; the noise warp roughens it but big holes on flat
  plates can read as round.
- A holed mesh renders all its back faces (charred black); hollow wreck meshes therefore look darker inside openings
  they already had.

## Cost

- No cost while intact: the opaque materials stay until the first hole on that mesh.
- A holed mesh is masked and two-sided: no early-Z in the depth and shadow passes for it, back faces drawn.
- One masked master copy per wreck master (about 55): extra shader compiles and cooked shaders.
- One complex line trace per tracked mesh per qualifying hit.
