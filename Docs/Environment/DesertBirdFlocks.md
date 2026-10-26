# Desert bird flocks: ABirdFlock + PBR VAT birds

Ambient flocks for the desert maps. Each flock is one actor drawing all its birds with a single instanced static
mesh; the wing beats are vertex animation textures (VAT) played on the GPU, so a flock costs one draw call and a
cheap per-tick transform update. Birds have no collision and are not targetable.

## Where things are

| What | Path |
| --- | --- |
| C++ class | `Source/RTS_Survival/Environment/Birds/BirdFlock.h/.cpp` (`ABirdFlock : AActor`) |
| Master material | `/Game/RTS_Survival/Blueprints/Environment/Animated/Birds/DesertFlocks/M_Bird_VAT` |
| Flock presets | `.../DesertFlocks/BP_BirdFlock_CobaltRoller`, `BP_BirdFlock_InkRaven`, `BP_BirdFlock_WhiteStork` |
| Per species | `.../DesertFlocks/<Species>/` — `SM_<Species>_VAT`, `T_<Species>_VAT_Offset/Normal`, `MI_<Species>_VAT` |
| Shared feather detail | `.../DesertFlocks/Textures/T_BirdFeather_Detail`, `T_BirdFeather_N` |
| Blender source | `D:\Blender\BlenderProjects\MappingDetails\Birds` (see its `README.md`) |
| Test | `/Game/RTS_Survival/Maps/UnitTests/UT_FarmAnimals` (actors `BirdFlock_*`) |

## Species (chosen to contrast with orange/yellow sand; colour sits on the back, which the RTS camera sees)

| Preset | Look | Birds | Height | Speed | Ellipse (semi axes) | Flight clip |
| --- | --- | --- | --- | --- | --- | --- |
| CobaltRoller | turquoise back, cobalt/navy wings, lilac breast, forked tail | 14 (×1.5–1.8) | 10 m | 11.5 m/s | 140 × 65 m | 3 quick flaps + glide |
| InkRaven | feather-card raven: 7 slotted primaries, 11 secondaries, covert rows, wedge tail, heavy bill; textured atlas with alpha cut-outs (`M_BirdTextured_VAT`) | 9 (×1.0–1.15) | 13 m | 9.5 m/s | 220 × 100 m | 2 flaps + glide |
| WhiteStork | white, black flight feathers, red bill and legs | 7 (×1.0–1.15) | 18 m | 8 m/s | 300 × 140 m | 1 slow flap + long glide |

### InkRaven PBR/topology pass (2026-10-26, after an adversarial review)

- Albedo: the atlas was double gamma corrected (feathers ~0.0015 linear). Now authored in linear and encoded once:
  vanes 0.026–0.045, plumage gaps ~0.02, shafts ~0.08 linear; soft 2.5-texel alpha edge; 3 % UV gutters.
- `T_InkRaven_ORM` (R = AO: feather roots 0.35 → 1, plumage gaps; G = roughness: vanes 0.6, shafts 0.5,
  plumage 0.55–0.65, bill 0.35). `M_BirdTextured_VAT` gained an `ORMTexture` + static switch `UseORMTexture`
  (off for Crow/Dove, so they are unchanged). Glossier vanes (0.45) mirrored the sun white from the RTS camera.
- Albedo texture: Clamp addressing, *Scale Mips for Alpha Coverage* (A threshold 0.333). Dither Opacity Mask was
  tried and turned off again: it showed as a screen-door pattern on the wings in close-ups.
- Rig: each card moves rigidly with its root's weights; body/bill only use root, head and tail bones (max card
  stretch during the flap 23 % → 6.7 %). Every overlapping card pair is now ≥ 1.8 mm apart (was 102 pairs < 1 mm).
- VAT offset texture stored as `TC_HDR_F32` (half floats stepped ~0.3 mm at wing-tip range).
- Before/after: `MappingDetails/Birds/InkRaven/Previews/BeforeAfter_*.png`.
- Round 2 (same day):
  - Bill extruded from the head ring (12 sides, raised culmen): body + bill are one closed surface (0 boundary
    edges), cap UVs projected flat (no zero-area UV faces).
  - 5 scapular cards per side over the wing root, weighted 50 % body root / 50 % shoulder.
  - Two material slots: `M_InkRaven_Body` → `MI_InkRaven_Body_VAT` (base property override: opaque, one-sided)
    and `M_InkRaven_Feathers` → `MI_InkRaven_VAT` (masked, two-sided).
  - Normal map `T_InkRaven_N` (shaft ridge, vane dome, barbs; scalloped contour feathers). VAT meshes have no
    tangents, so `M_BirdTextured_VAT` builds a per-pixel cotangent frame from ddx/ddy of the camera-relative
    world position (includes the VAT offset) and UV0, behind static switch `UseNormalTexture` (off for Crow/Dove).
    Amplitude matters: 3x stronger mip-averaged into blotches on the body.
  - LOD1 (`raven_lod1.py`): LOD0 minus the lesser/marginal covert rows, same vertex VAT UVs → same VAT textures.
    798 vs 1086 UE verts, screen size 0.15 (close-ups LOD0, RTS camera LOD1). **LOD1 build settings must match
    LOD0** (full-precision UVs, no recomputed normals, keep degenerates) or its vertices animate from the wrong
    VAT columns.
  - Unreal reimport: `MappingDetails/Birds/Scripts/ue_reimport_inkraven.py`. Before/after: `BeforeAfter2_*.png`.
- Still open: flat card normals (cards light as one sheet per row), crow/dove not on the ORM/normal paths.

### Textured species converted from the project's skeletal birds

| Preset | Source | Birds | Height | Speed | Ellipse | Flight clip |
| --- | --- | --- | --- | --- | --- | --- |
| Crow | `/Game/RTS_Survival/Meshes/animal/Crow` (420 verts, `Crow_D` 128 px with feather cut-outs) | 10 (×0.6–0.72) | 12 m | 9.5 m/s | 240 × 110 m | procedural: 2 flaps + glide (the asset only ships perched "look" clips) |
| Dove | `/Game/RTS_Survival/Meshes/animal/Dove` (244 verts, `Dove_D` 64 px) | 16 (×1.3–1.6) | 9 m | 12 m/s | 160 × 70 m | procedural: 4 quick beats + short glide |

They use `M_BirdTextured_VAT` (same VAT playback, base colour/opacity from the original texture, two-sided, masked,
VAT normal flipped on back faces with `TwoSidedSign`, roughness per material instance). The originals were unlit
low-res assets; converted they are lit PBR, but their textures stay small. The shipped `fly` clip of the dove
was not used: after re-orienting the rig both wings rotated the same way, so it got a procedural loop instead.
Source: `MappingDetails/Birds/Scripts/ue_birds_to_vat.py` (FBX exported to `RTS_Survival/Saved/BirdExport`)
and `ue_import_textured_birds.py`.

The default RTS camera is ~30 m up (3000 cm arm at −70°); flight heights stay below it. Zooming in further than
~20 m puts the storks above the camera, which is fine for ambient birds.

## ABirdFlock

- The actor location is the ellipse centre, the actor yaw its orientation; birds start at the +X end of the
  ellipse (rotate the actor to choose where the flock begins) and circle counter-clockwise unless `bClockwise`.
- Each bird has its own position in the formation (`FormationLength` along the ellipse, `FormationWidth` across),
  height offset, slow height drift, scale, speed (±`SpeedVariation`), wing-beat phase and rate.
- Constant arc speed along the ellipse (the angle advances by `Speed / |dP/dθ|`), yaw from the tangent, bank from
  the turn rate (`BankPerTurnRate`, clamped to `MaxBankDegrees`).
- `bFollowTerrain`: one `COLLISION_TRACE_LANDSCAPE` trace per tick (round robin) keeps `FlightHeight` above dunes.
- `Seed` makes a placed flock deterministic. Instances are rebuilt in `OnConstruction`, so the flock previews in
  the editor.
- Per-instance custom data (`BirdFlockInstanceData`): 0 = VAT phase seconds, 1 = wing-beat rate multiplier.

## M_Bird_VAT (lit, PBR)

- WPO and normal from the VAT (`Texture.Load`, frame row = `floor(VATUV.y * H) + frame`, Local→World).
- Base colour = vertex colour (stored sRGB, linearised with `pow 2.2`) × feather albedo detail.
- Roughness = vertex alpha (feathers 0.62 roller / 0.42 raven satin / 0.70 stork; beaks 0.35; legs 0.5) ×
  `RoughnessScale`; metallic 0; AO from the feather cavity mask.
- Feather normal map applied on the world-space VAT normal with the bird's forward axis as tangent
  (`FeatherTilingPerMetre`, `FeatherNormalStrength`).
- Usage flag *Used with Instanced Static Meshes* is set explicitly (needed for cooked builds).

## Fog of war

Birds are ordinary scene geometry, so the fog-of-war post process hides them over unexplored ground like
everything else. The test screenshots (`Birds/<Species>/Previews/InGame_RTSView.png`) were taken with the fog
post process disabled in PIE only.

## Verified in PIE (UT_FarmAnimals)

All three flocks circle their ellipses: every bird stays within ±7 % of the ellipse (its formation lane), faces
its direction of travel, banks in the turns and casts a ground shadow. Screenshots from a camera with the game
camera's framing (3000 cm arm, −70°, FOV 90) show all three species clearly against the sand.

## Not done

The flocks are not yet placed in any shipped desert map; drop a `BP_BirdFlock_*` in the map, scale the ellipse to
the playable area and rotate it so flocks pass over interesting spots.
