# Scavenger portrait: breath vapour + glowing vials

The GermanScavengers speaker portrait (`ERTSPortraitTypes::GermanScavengers`, speaker name "Scavenger") is the existing
scavenger render (hooded camo suit, gas mask with cheek filters, ribbed hose, green vials, "F-0352" plate). While a line
plays, two effects run on the same talking rhythm:

- **Atemdampf (breath vapour):** warm vapour puffs out of the two cheek filters. Each puff carries the loudness of the
  moment it left, drifts out to its own side and up, breaks into wisps and fades within about 1.5 s, so loud phrases
  make dense clouds and pauses let the air clear. A small puff on every breath (about one every 4 s) keeps him alive
  when silent.
- **Radix-Ampullen (glowing vials):** the two green vials on his strap brighten with each word (a little behind the
  voice), bubbles rise in the liquid, and green light falls on the strap, the plate and the hose. A dim glow stays on
  between lines.

Both were options in the 2026-11-12 preview (C and B). The vapour was picked first; the vials were added later the same
day at the user's request. The third option, a pressure wave down the hose, was not used.

The rhythm is time-driven, not synced to the voice audio. It uses the same talking envelope as the other speaker
portraits (`CommanderPortraits/Tools/announcer_trace.py`; see `HazmatPortrait.md`, `VulturePortrait.md`).

## Assets

All in `/Game/RTS_Survival/Textures/GameUI/ScavengerPortrait/` (asset names predate the vials):

| Asset | What it is |
| --- | --- |
| `T_Portrait_ScavengerVapour` | The old thumbnail's 2048 source cropped to the portrait box aspect 176 x 232 (source x 247..1801), 1554 x 2048. The old brush squeezed the square image into the box. UI group, no mips, never stream. |
| `M_UI_ScavengerBreathVapour` | UI material: portrait + one Custom node (`ScavengerBreathVapour`) drawing vials and vapour. Time ignores pause (period 600 s). Opacity is `VertexColor.a` only, for the widget fade-in. |
| `SB_Portrait_ScavengerVapour` | Slate brush drawing the material. `W_BP_Portrait.PortraitBrushes[GermanScavengers]` points here. |

To revert, point `PortraitBrushes[GermanScavengers]` back at
`/Game/Thumbnails/Thumb_BP_Thumbnail_RusHazmatThumbnail_Scavenger_Brush` (left untouched). To drop only the vials, set
`VialStrength` to 0; to drop only the vapour, set `Strength` to 0.

## Material parameters

| Parameter | Default | Effect |
| --- | --- | --- |
| `Strength` | 1.0 | Vapour opacity scale (0 = no vapour). |
| `Rise` | 1.0 | How fast the puffs drift up. |
| `VialStrength` | 1.0 | Vial light scale, glow, bubbles and spill together (0 = vials as in the render). |
| `Voice` | 1.0 | Weight of the talking rhythm for both effects (0 = breath puffs and the dim vial glow only). |
| `FilL` / `FilR` | fitted | Filter centre in height units `p = (u * Aspect, v)`; `z` = side x radius (left negative). |
| `V1A` / `V1B`, `V2A` / `V2B` | fitted | Vial capsule end centres in height units; A is the lower end, bubbles rise toward B. |
| `V1R` / `V2R` | fitted | Vial capsule radii in height units. |
| `Aspect` | 0.7588 | Texture width / height. |
| `TimeOffset` | 0 | Only for captures. |

## Rebuilding

- Source image: exported from `Thumb_BP_Thumbnail_RusHazmatThumbnail_Scavenger` with `AssetExportTask` (the texture
  reports 32 x 32 while streamed out) to `Saved/ScavengerPortrait/scavenger_source.png`. Feature positions (filters,
  hose, diaphragm, vials, crop) are in `Saved/ScavengerPortrait/features.json`; `build_scavenger_page.py` there writes
  the crop and the three-option preview page.
- HLSL: `Source/Tools/ScavengerPortrait/scavenger_breath_vapour.hlsl`. Everything is mixed in display space (texel
  encoded with pow 1/2.2, result decoded with pow 2.2): the vial light is added first (with a soft knee), then the vapour
  is screened over. Pixels away from both come out exactly as the texture. Bubbles rise at a fixed speed and only their
  brightness follows the voice: the preview tied their speed to the voice, which makes the phase jump once the material
  clock is large.
- Build: `Source/Tools/ScavengerPortrait/build_scavenger_vapour.py` through UECP `execute_python`
  (`exec(open(...).read(), ns)`, then `run('texture')`, `run('material')`, `run('point')`). `capture()` writes PNG
  frames to `Saved/ScavengerPortrait`; the editor clock is added to `TimeOffset`, so sweep offsets to compare frames.
  After an editor restart the portrait texture shows a grey checkerboard until it has rebuilt: check that
  `blueprint_get_size_x()` is 1554 before capturing.
- Live check: `Source/Tools/ScavengerPortrait/scavenger_portrait_test.py` in a throwaway DebugGame editor.

## Verified

2026-11-12, vapour:
- Material validates. Canvas capture at half size without vapour: mean difference to the texture 0.35 / 255, mean
  shift -0.01 (brightness unchanged). Captures at a sixth of the size differ more (1.4 / 255) only because the no-mip
  texture aliases when shrunk that far. An 8 s sweep shows puffs building on phrases and clearing in pauses.
- Live widget on `RTS_ToCopySmall` through `UPlayerPortraitManager::PlayPortrait(GermanScavengers, ...)` in a throwaway
  DebugGame editor: fade-in works, the cropped 176:232 framing shows, vapour puffs from both filters change between every
  shot. The first run still showed the old brush because `EditorAssetLibrary.save_asset` had not written
  `W_BP_Portrait`; the build scripts now use `save_loaded_asset` and raise if it fails.

2026-11-12, vials added:
- Material validates (26 nodes). Half-size capture, silent: away from vials and filters the mean difference to the
  texture is 0.20 / 255 (shift -0.02). An 8 s sweep: the vials' green rises from about +20 (rest glow, in pauses) to
  about +75 while talking, spill shows on the plate; vapour unchanged. At 176 x 232 the vials read as a pulsing green
  patch.
- Live widget in a throwaway DebugGame editor: the cropped portrait fades in with both the glowing vials and the vapour
  (`Saved/ScavengerPortrait/live_sequence_vials.png`). The shared `Saved/Screenshots/WindowsEditor` folder can pick up
  another editor's `shot` at the same time (one frame of this run was a different session's map): match shots by size.

Not run: a mission line that uses the GermanScavengers portrait (the live tests borrow a hazmat line).
