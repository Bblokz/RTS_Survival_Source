# Berlin brewery enclave (Ger_Berlin POI, 2026-11-15)

The Burrower enclave in the Friedrichshain brewery cellars: two destructible pieces that behave like the Berlin ruin
kit (shell holes, then a short sink into the ground as a dead remnant). The owner picked concepts A1 "Kupferhaube"
and B3 "Gewoelbefeld" (with more green tile). Not placed on `Ger_Berlin` yet (site assumed south of the player start,
not confirmed).

- **Blender source:** `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinBreweryEnclave/Model/`
  (generators, validator, `Brewery_Report.txt`). Concept rounds in `../Concepts`, sheets in
  `Docs/Concepts/BerlinBreweryEnclave/` of the game project.
- **Unreal build:** `Tools/BerlinMap/poi/brewery_import.py` (headless `-run=pythonscript`, idempotent; report
  `Saved/Berlin/brewery/import_report.json`).
- **Test:** `Tools/BerlinMap/poi/brewery_test_run.sh <log>` (throwaway editor on `RTS_ToCopySmall`, never Ger_Berlin;
  pictures and `result.json` in `Saved/Berlin/brewery/test/`).

## Assets (`/Game/RTS_Survival/Blueprints/Environment/Berlin/BreweryEnclave/`)

| Asset | What |
|---|---|
| `Blueprints/BP_Berlin_BreweryHall` | the brewhouse: bare brick hall, half copper roof + ridge turret, rafters, two copper kettles, cellar head-house at the north door |
| `Blueprints/BP_Berlin_BreweryVaults` | the lager-cellar vault field: four brick vaults broken open, green glazed tile linings, floors, tiled north front, tiled wainscot wall, floor slabs |
| `Meshes/SM_Berlin_BreweryHall` / `_Vaults` | 5,512 / 7,086 tris, 31 / 19 UCX hulls, vertex colours (R soot, G plaster loss, B dust), lightmap UV1 |
| `Materials/M_BerlinBrewery_Glaze` | opaque master for the two signature looks (BaseTex RGB + roughness in A, NormalTex, Metallic, RoughnessMul, vertex R soot / B dust like `M_BerlinRuin`) |
| `Materials/MI_BerlinBrewery_Copper` / `_Tile` | verdigris copper (metallic 0.35) / green glazed tile |
| `Materials/*_WallHoles`, `M_BerlinBrewery_Glaze_WallHoles` | masked, two-sided hole variants (WallChunkDamage.md "A + B", no grid) |
| `Textures/T_BerlinBrewery_{Copper,Tile}_{D,N}` | 2048 tileable, 1 tile = 4 m |

Slots: brick `MI_BerlinRuin_Wall` and rubble `MI_BerlinRuin_Rubble` (the ruin kit's), copper / tile, and on the hall the
reused `M_BF_Crane_WornSteel` (rafters, rails, bands; UV at 1.6 m).

## Damage

Both are children of `BP_BerlinRuin_Base` and were built with the ruin kit builder's own functions
(`ruin_blueprints_build.py`), so they get the same components and collision: `StaticMesh` (Movable, Destructible),
`WallHoleDamage` (NoGrid, kit debris pieces), `NavBlock_NN` boxes from the ground hulls.

| Piece | Health / reduction | Hole class | Damaged materials per slot | Nav boxes |
|---|---|---|---|---|
| Hall | 6000 / 15 | hero (1.1 cm per mm, max 220 cm) | brick holes, -, copper holes, - (steel none) | 12 |
| Vaults | 7000 / 20 | core (1.0 cm per mm, max 200 cm) | brick holes, -, tile holes | 8 |

- A hole is a sphere in mesh space and cuts every slot inside it: a hit on the roof opens the copper sheet, a hit on the
  north front opens the tile and the brick behind it. Rubble and the iron never show holes (thin parts).
- Aim points: the hall's come from its wall hulls (kit rule); the vault field's are set by hand on the east haunches and
  the fermentation wall (`VAULT_AIMS`), because the kit rule reads wide vault hulls as heaps.
- Test (2026-11-15): 12 of 12 labelled 105 mm hits made holes (brick walls, gable, head-house, kettle, copper roof,
  tile front, tile wainscot, tile lining, vault crown, floor slab); the brick, copper and tile slots switched to their
  `_WallHoles` instances.

## Death (remnant, no heap)

The base graph runs it (BerlinRuinKit.md, "Death"); the values are class defaults from `PIECES[...]['death']`:

| Piece | Sinks | In | Turn | Dust | Thrown debris |
|---|---|---|---|---|---|
| Hall | 12 % of 20 m = 2.41 m (walls keep ~7.6 m, head-house ~1.2 m) | 2.5 s | tilt 1-2.5 deg, yaw 1-3 deg | `NS_Artillery_StoneImpact` x 1.5, 3 bursts, speed 0.6, grey-brown | up to 20 |
| Vaults | 18 % of 5.5 m = 0.99 m (crowns ~1.7 m above ground) | 1.8 s | tilt 0.5-1.5 deg, yaw 1-3 deg | same effect x 1.3, 3 bursts, speed 0.55 | up to 14 |

- The actor is never destroyed: it stays lowered and slightly turned, solid (blocks shells and sight), dead (more damage
  does nothing), with the nav boxes whose walls still stand 50 cm or more (hall 9 of 12, vaults 5 of 8). Test: drops of
  241 / 99 cm, roll 0.2 / 0.8 deg, yaw 1.3 / -1.6 deg, actor valid, second damage ignored.
- The first test used 20 % / 25 %: the hall's 3.6 m head-house (the cellar door) went fully under, so the sinks were
  reduced. On the vault field the tiled floors of the opened vaults (5 cm above ground) go under with any sink; the
  remnant shows the ground there.

## Placing on Ger_Berlin (not done)

Place both at scale 1, yaw free (+Y of the actor = the kettles / the entrance vault, toward the game camera if the
camera looks west), then **rebuild navigation and save** (the nav boxes are cut in at build time, BerlinRuinKit.md).
Keep the 6 m in front of the head-house door (actor +X) and of the vault mouth clear for the contact point.
