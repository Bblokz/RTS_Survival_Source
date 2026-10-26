# Farm animals: AWanderer + VAT static meshes

Ambient farm animals (sheep, goat, pig, cow, chicken) that wander around their start location, can be shot by
both sides and play a death animation. They are static meshes animated by vertex animation textures (VATs),
not skeletal meshes, so a herd costs a handful of draw calls and no animation ticking.

## Where things are

| What | Path |
| --- | --- |
| C++ class | `Source/RTS_Survival/Environment/Wanderer/Wanderer.h/.cpp` (`AWanderer : ADestructableEnvActor`) |
| Master material | `/Game/RTS_Survival/Blueprints/Environment/Animated/FarmAnimals/M_FarmAnimal_VAT` |
| Per animal | `.../FarmAnimals/<Animal>/` — `SM_<Animal>_VAT`, `T_<Animal>_VAT_Offset/Normal`, `MI_*_VAT`, `BP_Wanderer_<Animal>` |
| Test map | `/Game/RTS_Survival/Maps/UnitTests/UT_FarmAnimals` (herds of every animal + one German and one Russian test squad) |
| Blender source + bake | `D:\Blender\BlenderProjects\MappingDetails\FarmAnimals` (see its `README.md`) |

## Behaviour

- **Wander:** idle (graze clip, looped 1–3 times) → pick a random point within `WanderRadius` of the spawn
  location (projected on the navmesh) → follow a navmesh path, turning at `TurnSpeedDegrees` and snapping to the
  ground with a `COLLISION_TRACE_LANDSCAPE` trace → idle again. Without a navmesh under the animal it walks in a
  straight line. Tick is only enabled while walking.
- **Damage:** inherits `Health` / `TakeDamage` from `ADestructableEnvActor`. Collision comes from
  `FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision`: blocks `Projectile`, the `Enemy` and `Player`
  weapon traces and `Visibility`, so weapons of both sides hit it, splash damage (`FRTS_AOE` queries
  `ECC_Destructible`) reaches it and the player can right-click attack it. A damaged idle animal starts walking.
- **Crush:** `bDieWhenCrushedByTanks` kills it when any tank overlaps it (regular death clip).
- **Death:** plays the death clip once (holds the last frame), disables collision so corpses do not soak up
  bullets, and destroys the actor after `CorpseLifeTime` seconds (≤ 0 keeps it).
- **Variants:** `MaterialVariants` (cow, chicken, pig) — one is picked at random on BeginPlay.

## VAT material contract

`AWanderer` drives playback through custom primitive data; `M_FarmAnimal_VAT` reads it with CPD scalar
parameters (indices in `WandererVatPrimitiveData`):

| Index | Parameter | Meaning |
| --- | --- | --- |
| 0 | ClipStartRow | first texture row of the clip |
| 1 | ClipNumFrames | rows in the clip |
| 2 | ClipStartTime | `World->GetTimeSeconds()` when the clip started (minus phase) |
| 3 | ClipLoop | 1 loop, 0 hold last frame |
| 4 | ClipFramesPerSecond | playback rate (30) |

The Custom node reads texels with `Texture.Load` (exact, independent of filtering/streaming):
`row = floor(VATUV.y * Height) + StartRow + frame`, interpolating between two frames. Offsets and normals are
**local space** and go through *Transform Local → World* before WPO / Normal (the material uses world-space
normals, `Tangent Space Normal = false`).

Gotchas found while building it:

- GameTools bakes frame 0 into the **top** texel row and UE's FBX import flips V, so frames go *down* the
  texture (`+ row`).
- `MF_AzureDesertKite_VAT` (kite flock) had the same bug, `VATUV.y - Frame/33`, but it never showed: the
  `M_ADK_*` materials' *Local → World* transform had no input, so WPO was 0 and the kite did not animate at
  all (its Custom code also held literal `\n` characters and would likely not have compiled). Fixed 2026-10-25: the
  Custom node now uses the same `Texture.Load` lookup (`floor(VATUV.y * H) + frame`, 32-frame loop, row 32 is
  the duplicate closing pose), the transform input is wired, both VAT textures are `TC_HDR` with nearest
  filtering instead of BC6H (`TC_HDR_Compressed`), and the mesh uses full-precision UVs. Checked in PIE on
  static-mesh instances of `SM_AzureDesertKite_VAT` (same `M_ADK_*` materials) by pausing at known game times:
  row 11 shows wings up and row 27 wings down, as the forward lookup predicts (the old lookup would show the
  opposite). A kite rolled 90° stays intact. The Niagara flock itself was not framed: its birds leave the spawn
  area within seconds.
- WPO is world space: feeding local VAT offsets directly looks fine for small walk offsets but tears the mesh
  apart for the death roll once the actor is rotated.
- VAT UVs need *Use Full Precision UVs* on the mesh; VAT textures are `TC_HDR`, no mips, never stream.
- Mesh bounds are extended (positive/negative bounds extension) so the rolled-over corpse is not culled.

## Budgets (vertices as reported by UE)

| Animal | Verts | Tris | VAT size | Walk speed | Health |
| --- | --- | --- | --- | --- | --- |
| Chicken | 473 | 363 | 465 × 148 | 74.9 cm/s | 20 |
| Sheep | 1014 | 1540 | 1000 × 164 | 39.5 cm/s | 60 |
| Goat | 1077 | 1518 | 1067 × 164 | 46.4 cm/s | 70 |
| Pig | 1205 | 1624 | 1163 × 160 | 51.5 cm/s | 90 |
| Cow | 1473 | 2173 | 1454 × 172 | 52.7 cm/s | 160 |

Walk speed is measured from the stride in Blender so feet do not slide; keep `WalkSpeed` in sync if the walk
clip changes.

## Adding an animal / re-baking

1. Add an entry to `ANIMALS` in `FarmAnimals/Scripts/farm_animal_vat.py` and run it headless (see README).
2. Check the preview renders, then run `ue_import_farm_animals.py` in the editor (inject the report JSON when
   running through UECP — its Python sandbox cannot read files outside the project).
3. Clip rows, walk speed and health end up on `BP_Wanderer_<Animal>`; place the BP in a level.

## Verified in PIE (UT_FarmAnimals)

All animals wander inside their radius and switch between walk and graze; `ApplyDamage` lowers health, kills
at 0 and plays the death clip (screenshots in `FarmAnimals/<Animal>/Previews/InGame_*.png`); a German Mauser
squad killed a sheep and a Russian Fedrov squad damaged one with live fire; an enemy artillery hit killed a pig
and a cow during the same session.
