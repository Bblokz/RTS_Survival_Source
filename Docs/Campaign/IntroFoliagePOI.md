# Ger_IntroMission: dry foliage, crushable scrub, west/east POIs, cliff stone decals

Pass of 2026-11-19/20 (five rounds, map saved 2026-11-20; each checked by a fresh adversarial review agent). Tools in
`Source/Tools/IntroMission/foliage/`; working data, plans, shots and reviews in `Saved/IntroFoliage/` (not in git).
Round log: `Saved/IntroFoliage/ROUNDS.md`.

## What is on the map (labels / outliner folders)

| What | Labels | Folder |
|---|---|---|
| Flat ground cards, concept A ash-tan tussocks + C rust-umber mats, in clumps | `FOL_Detail_*` (HISM per 100 m tile, no collision, no nav, no decals) | IntroFoliage/Detail |
| Crushable dead brush patches (any tank) | `FOL_Scrub_DeadBrush_*` | IntroFoliage/Scrub |
| Crushable dead thistle patches, concept D (any tank) | `FOL_Scrub_Thistle_*`, farm `POIW_ThistleAccent*`, `POIW_FieldFallow` | IntroFoliage/Scrub |
| Cliff stone decals (8 types), beside / below / above the cliff kit | `FOL_StoneDecal_*` | IntroFoliage/CliffStones |
| West POI: drought farm (barn, house, hay shed, stacks, cart, van, burnt field + scorch fringe, fallow thistle field, drought orchard with dead trees, fences, yard litter) | `POIW_*` | (POI) |
| East POI: dried quarry lake (rim bath hut, cars, tables, bench, signs, plank walkway down the bank, stranded rowboats, dead willows) | `POIE_*` | (POI) |

## Assets (all under `/Game/RTS_Survival/Blueprints/Environment/`)
- `CrushableInstancedActor/DryScrub/`: `M_DryScrub_Simple` (masked, ISM usage saved: the crop field builds its ISMs at
  runtime, the Megascans foliage master did not render there), `SM_DryScrub_DeadShrub_A..E`,
  `BP_CrushableScrub_DeadBrush`; `M_DeadThistle` (opaque, VertexColor x Tint), `Meshes/SM_DeadThistle_A..C` +
  `_Crushed_A..C` (built by `thistle_blender.py`, source FBX in
  `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/DeadThistle/`), `BP_CrushableScrub_Thistle`;
  card MIs `MI_DryScrub_AshGrassTop`, `_AshGrassPit`, `_RustMat`, `_RustMatTop`; `MI_DryScrub_OrchardDrought`
  (orchard leaf texture is ~1.8x greener than red: the tint needs R about 2.2x G to read sere).
  `BP_CrushableScrub_DryGrass` + `MI_DryScrub_FeralGrass` exist but are unused (user: golden grass does not fit).
- `OreMountains/CliffKit/Decals/CliffStones/`: `MI_CliffStoneDecal_<FallenBlock|BlockPair|ChipScatter|ScreeFan|
  RubbleStrip|HalfBuriedSlab|FineGravel|LipFragments>` on the existing `M_OreCliffDecal` master, textures baked from
  the cliff kit's own rock generator and atlas (`T_OreCliff_BC/_N` with the `MI_IntroCliff` desert values baked in).
  Bake: `.../OreCliffKit/Decals/CliffStones/stones_blender.py` + `pack_stones.py`.

## Placement rules (fol_plan.py / decal_plan.py / poi_plan.py)
- Keep-outs (m): cards road 1.5, mission routes 2.5, track stripes 1.2, water 2.5, resources 7; crushable patches road
  6, routes 8.5, stripes 4.5, resources 9, plus the west ramp approach and fuel truck lanes, mission spots, base box.
  Nothing under the collision-less mountain / cliff meshes (`survey/cover.png`).
- Crushables never add nav: all 19 mission routes x 4 nav agents keep their baseline length (checked every round).
- Patches get per-patch spacing / scale for 5-15 plants; thistle patches favour verges just past the road keep-out,
  fences, the farm and rocky ground.
- Stone decals: centres within 9 m of `REAR_Cliff_*` / `RIDGE_Cliff_*`; the whole rotated rectangle must miss the cliff
  coverage dilated 40 cm (DBuffer decals paint any mesh in their box) and is then slid towards the rock until it would
  touch (stones sit at the foot / lip). Foot vs lip by the ground on the far side of the rock (+-1.5 m), else
  'beside'; fans only below, lip plates only above; ScreeFan / RubbleStrip image top faces the cliff. The ground under
  the whole rectangle stays within +-45 cm of its centre and clear of routes, roads, stripes, lanes, base.
  DecalActor pitch -90, `YAW_FIX = 180` (PIE calibrated), sort order 1, box +-60 cm, MI angle fade 0.45-0.75,
  DepthFade 0.4 (less paint on units parked on a decal).
  The rotator read back at pitch -90 is gimbal-mixed: use `get_actor_up_vector()` (texture width axis) to check.

## Re-running
Throwaway DebugGame editor with `fol_editor_server.py`, requests via `fol_cmd.py`. Order: `fol_assets.build_*`,
`fol_plan.py --round N`, `poi_plan.py --round N`, `decal_plan.py --round N`, then in the editor
`fol_apply.apply_plan(N)`, `poi_apply.clear_pois(); apply_pois(N)`, `decal_apply.apply(N)`, `fol_nav.nav_routes(tag)`,
`fol_shots.py` (PIE pictures). Re-survey (`fol_survey.py`) if the map changed.
