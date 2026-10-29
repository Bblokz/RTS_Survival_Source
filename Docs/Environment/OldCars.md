# 1940s civilian cars (one shader, per-instance colour)

Eight 1940s civilian cars share one material, one atlas and one material slot each. Every placed car or instance
gets its own paint colour and wear from custom data, so a whole car park renders with one shader and no dynamic
material instances.

| Mesh | Car | Size (cm) | LOD0 / 1 / 2 verts |
|---|---|---|---|
| `SM_OldCar_Sedan` | 4-door sedan: shield grille, lamps on stalks, running boards, spare on the boot | 482 x 178 x 176 | 10.1k / 6.4k / 3.1k |
| `SM_OldCar_Fastback` | 2-door streamliner: fender lamps, horizontal grille, rear skirts | 463 x 182 x 163 | 9.4k / 5.9k / 2.9k |
| `SM_OldCar_Coupe` | business coupe: short cabin, long deck | 471 x 178 x 167 | 9.1k / 5.8k / 2.8k |
| `SM_OldCar_Cabriolet` | open two-seater: leather seats, framed screen, folded top | 457 x 174 x 144 | 9.2k / 5.8k / 2.8k |
| `SM_OldCar_Limousine` | long 6-window staff car, sidemount spares | 539 x 212 x 186 | 11.1k / 7.2k / 3.4k |
| `SM_OldCar_Pickup` | light pickup, wooden bed floor | 470 x 184 x 189 | 8.8k / 5.6k / 2.7k |
| `SM_OldCar_PanelVan` | delivery panel van | 480 x 188 x 207 | 9.2k / 6.1k / 2.9k |
| `SM_OldCar_Compact` | small 2-door city car | 370 x 144 x 148 | 8.3k / 5.3k / 2.6k |

Content lives in `/Game/RTS_Survival/Blueprints/Environment/Props/OldCars/`:
- `Meshes/`: one material slot and one section per LOD, two UCX convex hulls (body plus cabin), and vertex colours.
- `Materials/M_OldCar`: the only material, with Instanced Static Mesh usage on.
- `Textures/T_OldCar_Atlas_{D,N,ORM}`: 2048 maps. D alpha holds grunge; ORM is R = 1, G = roughness, B = metal.
- `BP_OldCar` and `BP_OldCarRow`: see below.

Nothing is placed in a map yet.

## Placing them
- **`BP_OldCar`: one car.**
  - Set `CarMesh`. `RandomPaint` (on by default) picks one of the 12 `Palette` colours from the actor's position,
    so a car keeps its colour when other things change. With `RandomPaint` off it uses `Paint`.
  - `Wear` runs from 0 (clean) to 1 (chipped and muddy).
  - The construction script writes custom primitive data with `SetDefaultCustomPrimitiveData*`, so the values are
    saved with the level: colour to 8-10, wear to 11.
- **`BP_OldCarRow`: a row or grid of one model in a single ISM, one draw call per pass for the whole row.**
  - Settings: `Count` cars per row, spaced `Spacing` apart (side by side along local Y); `Rows` rows, `RowSpacing`
    apart (along X).
  - Each car gets up to `YawJitter` degrees of yaw. About one in five is parked facing the other way.
  - Colour and wear come per instance from a hash of the row, the column and `Seed`, so they are stable when you
    move the actor.
  - The values are written to per-instance custom data 0-3. Wear is picked between `WearMin` and `WearMax`.
  - Use one row per model. Several rows still share the same shader.
- **Palette.** 12 period paints, filled in by `ue_oldcar_bp_defaults.apply()` (array defaults can't be set from the
  graph tools): black, two greens, maroon, navy, cream, dove grey, brown, olive, faded blue, dark red and beige.

## How the shader works
- **Atlas quarters.** The atlas has four quarters, one per Blender part shader: paint (top-left in UE), metal (chrome
  and dark steel), glass (windows and lenses) and trim (tyres, leather, wood, black fabric). The paint mask is just
  "UV in the top-left quarter": `(1 - floor(2u)) * (1 - floor(2v))`.
- **Colour source.** Colour is `PerInstanceCustomData3Vector(0)`. Its `DefaultValue` is the vector parameter
  `PaintColor`, which is bound to custom primitive data 8. A single static mesh uses primitive data and an ISM uses
  instance data, with the same material instance. Wear works the same way: `PerInstanceCustomData(3)`, defaulting to
  `Wear` (primitive data 11).
- **Why primitive data starts at 8.** Primitive data 0-7 is taken: the faction flag uses 0 (`FRTS_FactionFlags`) and
  the team colour uses 4-7 (`Docs/Rendering/TeamColorShaders.md`).
- **Per-car detail comes from vertex colour** (R = AO, G = convex edges, B = dirt). The formulas:
  - `tinted = D * lerp(1, colour, PaintMask)`
  - `chip = PaintMask * saturate((VC.G*0.9 + grunge*0.7 - 1.15 + wear*0.6) * 6)`
  - `dirt = saturate(VC.B * (grunge + 0.5) * wear * 1.6)`
  - `base = lerp(lerp(tinted, chip colour, chip), dirt colour, 0.85 dirt) * (0.1 + 0.9 VC.R)`
  - `roughness = lerp(ORM.G, 0.85, max(dirt, 0.6 chip))`
  - `metallic = ORM.B * (1 - dirt)`
- **Cost.** About 330 pixel shader instructions and 6 samplers. The material is opaque; the glass is dark and glossy
  rather than translucent, which is what keeps a single material possible.
- The Blender preview (`OC_Wear` in the pipeline) uses the same formulas, so the Blender renders match the game.

## Rebuilding
The pipeline is in `D:/Blender/BlenderProjects/BlenderTanks/OldCars/` (see its README). It runs in Blender 5.2
through the Blender Lab MCP add-on bridge, using its own headless instance on port 9877.

To get the meshes into UE:
1. Copy `ue_import_oldcars.py` and `ue_oldcar_bp_defaults.py` to `Saved/OldCars/`.
2. Run `import_all()`, then `apply()`.

## Verification (2026-10-28, Ger_Trains)
- **Test set.** One 4-car `BP_OldCarRow` per model (32 cars from 8 ISMs) and two `BP_OldCar`s were spawned near
  (-15500, -12500). They were removed afterwards and the map was not saved.
- **Data.** Every instance had 4 custom floats, each with a palette colour and wear between 0.15 and 0.6. The single
  cars had primitive data 8-11 set.
- **Looks.** The screenshots showed every car in its own colour and wear, under the one material.
- **Vertex colours.** They reach UE unchanged: a 0..1 ramp test bar read back 0.0 to 1.0, and 0.5 came back as 0.498.
- **Bug found and fixed in the test.** Blueprint `Frac` keeps the sign, so the first version of the row hash gave
  negative palette indices. The hash now uses `frac(abs(sin(...)) * k)`.

## Crushable cover cars (`Cover/BP_Cover_OldCar_<Name>`)
`BP_Cover_OldCarMaster` is a duplicate of the farm tractor master (`FarmTractors.md`), so it is a child of
`BP_NaturalCover` and works the same way:
- **BeginPlay** sets the health and `SetupCrushDestructionOverlap(MainMesh, MediumOrHeavyTank, RegularDeath)`.
- **Death** (`BP_OnUnitDies`): `CollapseMeshWithSwapping` swaps to `SM_OldCar_<Name>_Wreck` and attaches fire, then
  `VerticalDestruction` plays the explosion, sinks the actor by `CollapseDeltaZ` (car height + 30 cm) over 5 s and
  destroys it. The cover points go with it.
- **Burnt.** The master also sets custom primitive data 12 (`Burnt`) to 1 on death, so `M_OldCar` draws the wreck as
  soot with rust breaking through. Wrecks use the same single material.
- **Paint.** The construction script picks the colour like `BP_OldCar` (`RandomPaint`, `Paint`, `Wear`, `Palette`)
  and writes it to custom primitive data 8-11. It also sets 12 to 0.
- **Navigation.** `NavModifier` uses `RTS_NavArea_BlockCharLight`, with `failsafe_extent` set to the car's half
  extents. Infantry and light tanks path around the car; medium and heavy tanks drive through it and crush it.
- **Health.** 1100 for most cars; 900 for the Compact; 1300 for the Limousine, Pickup and PanelVan.

The intact and wreck meshes carry the same cover sockets (`oc_cover.py` in the pipeline). They are placed from each
car's measured height profile, 42 cm outside the car:
- **standing / standing_right:** at both ends of every cabin that is at least 145 cm tall, 22 cm inside the edge.
- **crouch:** beside the front and rear fenders where the car is at least 90 cm tall, plus a middle one when there is
  no tall cabin (Cabriolet). Also in front of and behind the car where the nose or tail is at least 90 cm tall.

The peek side follows `Up x Forward` in UE axes. On the car's UE -Y side (soldier facing +Y) the rear cabin edge is
on his right; on the +Y side it is the front edge. Counts: Sedan 8, Fastback 10, Coupe 10, Cabriolet 8, Limousine 8,
Pickup 10, PanelVan 8, Compact 7.

`ue_oldcar_bp_defaults.apply_cover()` re-applies mesh, wreck, health, fire offset, sink depth, nav area and extents,
and the palette.

**Test (2026-10-28, Simulate on Ger_Trains, cars placed temporarily and removed afterwards):**
- With `LogRTSCoverFinder Verbose`, all 18 sockets of a Sedan and a Coupe logged `on_navigation=1` with `snap=0`.
- A T-34/76 (medium) teleported onto each car crushed it, and the car was removed within about 4 s.
- A Panzer II F (light) sitting on the Coupe did not crush it.
- A Sedan killed with `TriggerDestroyActor` swapped to the wreck, showed the burnt material with fire, sank (z 144 to
  87 about 1 s in) and was destroyed.

## Not done
- `BP_OldCarRow` (instanced) cars are scenery only: an ISM instance cannot be crushed or used as cover. Use the
  `Cover/` blueprints where infantry or tanks interact.
- None of the cars is placed in a map.
