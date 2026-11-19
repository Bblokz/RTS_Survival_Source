# Ger_IntroMission base ridge rework (2026-11-18)

User request: make the base ridge's rear bigger so the player has room for trenches and defences, put rock faces all
around the ridge like on the rear ridges (but not at the ramps), and stop Soviet buildings leaving objects that block
player buildings.

## Terrain and cliffs

Tools: `Source/Tools/IntroMission/ridge/`.
- `base_leftovers_probe.py` exports the current terrain (`Saved/IntroRear/terrain/now_full_*`) and surveys the actors.
- `ridge_gen.py preview` classifies the rim; `ridge_gen.py build` writes the result.
- `ridge_apply.py` applies and saves; `ridge_verify.py` / `ridge_verify2.py` check it.

- **Rear extension.** The polygon (4350,600) (7400,800) (7800,2300) (7600,4300) (6800,5900) (4350,5950) is raised to
  plateau height (531). It adds about 30 x 53 m east of the old east edge, north of the east climb from the hub. Its
  south edge stays clear of road D. The paint already matched the plateau (Sand with DesertRockDark), so only heights
  changed.
- **Rim.** The plateau outline (contour at 471 around the HQ spot) is sampled every metre.
  - Gentle stretches (< 25 degrees: the west ramp, the long south slope) stay as they are.
  - Stretches next to walls, gates, buildings, pits, slabs, resources, road D, the fuel truck lane
    (x 4640, y -5400..-1500) and the counterattack route points stay as well (this keeps the east climb from the hub
    open).
  - Every other steep (25-44 degrees) or cliff (> 44 degrees) stretch was steepened like the rear ridges (height-fraction
    remap, top never cut).
  - The SW corner where the plateau runs into the hill chain was left out.
- **Cliff kit.** 114 Ore kit C pieces (mostly MassLow / Lip / Scree, because the faces are only 3-6 m high) sit in
  `RIDGE_Cliff_<piece>` HISM actors, folder `IntroRidge/Cliffs`, material `MI_IntroCliff`, no collision or nav.
  - 41 dressing props were re-seated to the new height and 9 decals/stones under the faces removed.
  - Map backup: `Saved/RidgeRework/Ger_IntroMission_before_ridge.umap`.
- **Nav check.** Every agent (Character, Light, Medium, Heavy tank) reaches the HQ spot from:
  - the new rear area (54 m);
  - the west ramp, the south slope and the hub;
  - the centre, station and landing pad spawns of the counterattack.

  The fuel truck lane still connects.

### Second rock pass (2026-11-19)

The user sent a screenshot of the low terrace round the camo MG bunker (`Rus_BXP_MGBunkerOneCamoed`, -6778,3127),
NW of the base below the plateau's arm, which had rocks on only part of its edge, and asked for rocks "all around it".

`ridge_gaps.py` (no terrain change) adds the kit wherever the current terrain is steeper than 30 degrees, has a face of
1.5 m or more, and has no first-pass rock within 6.5 m. It finds faces on contours every metre, at the face's
mid-height, and bridges gaps of up to 3 m.
- Low faces (1.5-2.5 m) get MassLow pieces scaled down to the face.
- No rock reaches into:
  - the core of a bunker or building (60% of its bounds);
  - a wall, slab or resource;
  - a road or the fuel truck lane;
  - the east climb from the hub.

This added 67 pieces: the bunker terrace, the hollow's west rim, the south edge, the edges of the rear extension and
the terraces south of the base. `RIDGE_CLIFFS_ONLY=1 ridge_apply.py` re-placed all 181 pieces (map backup
`Saved/RidgeRework/Ger_IntroMission_before_ridge_rocks2.umap`). The terrace's gentle SW curve (under 30 degrees) and the
ground right at the bunker stay bare. First-pass instances are kept in `Ridge_cliffs_pass1.json`; re-run
`ridge_gaps.py` from that copy.

## Soviet building leftovers

- **New C++.** `ABuildingExpansion::DeathSpawnClassesToSkip` (`TArray<TSoftClassPtr<AActor>>`, class default). It
  removes those classes from whatever the death graph passes to `DestroyAndSpawnActors`.
- **Rus_Building_Barracks.** Skips `ScavObj_VehicleParts2` (the vehicle parts scavenge pile). The barracks rubble
  `BP_Obstacle_RusBarracks_Destroyed` still spawns; it has no placement collision.
- **Rus_Building_CoolingTowers.** Skips `Resource_Metal_Barrels`.
- These apply on every map, and the `_Concrete` children inherit them.
- **BP_Dest_RusFuelLarge** (the wreck of `Rus_Building_FuelStorageLarge`): `InitialLifeSpan` 10 s on every map. No map
  places it by hand.
- **Verified in PIE.** Every Soviet building in the base was killed. No loot piles came, nothing new blocks building
  placement (channel BuildingPlacement), and the wreck counts down from 10 s and is gone.
- **Not changed.** Other Rus buildings reference the same loot classes: AdminBunker, Factory, LargeTent,
  PlatformFactory, ResearchCenter and Bunker02_ZIS.
