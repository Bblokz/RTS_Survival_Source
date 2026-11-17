# Frozen Lake ice shaders

The ice look for the Frozen Lake mission ([FrozenLakeMission.md](FrozenLakeMission.md)). It covers only the shaders:
one master ice material, its instances, a water material for under the ice and a test setup. The ice mechanics (load,
holes, flood, sinking) are not built yet. This page says how they should drive the material.

Status (2026-11-17): round 4, built from the owner's feedback. Round 4 added vehicle tracks per ice level. Shot headless
on the test map and in `Ger_FrozenLake`'s winter light (map not saved). Not committed.

Owner decisions (2026-11-17):

- one lake mesh with the state mask, with narrow 2–3 m borders and a visible edge line;
- the vein stays Radixite amber, and the warn changes to "cryo failing" (dark wet cracks, a pale cyan pulsing rim);
- holes are see-through (Masked plus a water plane);
- black ice holds **infantry only**.

## Assets

Everything is in `/Game/RTS_Survival/Blueprints/Environment/FrozenLake/Ice`.

| Asset | What it is |
|---|---|
| `M_FrozenLakeIce` | Opaque, default-lit master. One Custom node (`FrozenLakeIce`) holds all the logic. The map references it, so it is only changed in place. |
| `MI_FrozenLakeIce_Lake` | **The map's parent.** `UseStateMask` on, blend mode overridden to **Masked** (real holes). `StateMask` = the neutral texture. The map's `MI_FL_IceLake` is a child of it and sets only `StateMask`, `LakeOrigin` and `LakeSize`. |
| `MI_FrozenLakeIce_Heavy` / `_Medium` / `_Infantry` | One type per plane (`BaseThickness` 1 / 0.5 / 0), opaque. For separate planes and props; the lake uses `_Lake`. |
| `MI_FrozenLakeIce_Infantry_Vein` | Black ice with `VeinAmount` 1: the glowing Radixite roots show from below. |
| `M_FrozenLakeWater` / `MI_FrozenLakeWater` | Translucent open water for a plane about 20 cm under the ice. You see it only through the holes. It has a depth fade (shallow water shows the lakebed and sunk hulls; deep water goes dark), a soft sky sheen so deep water is not flat black (`SkySheen`, `SkySheenColor`), and drifting ice crumbs. The map's lead water (`SM_FL_IceLeadWater`) uses it too. |
| `Textures/T_FrozenLakeIce_Noise` | 1024², tiles, BC7, linear. R = cloud noise, G = distance to the nearest crack, B = root network, A = bubbles. |
| `Textures/T_FrozenLakeIce_StateNeutral` | 4x4 default for `StateMask` (medium, nothing else). |
| `Test/MI_FrozenLakeIce_TestLake`, `Test/T_FrozenLakeIce_TestState` | Child of `_Lake` with the test layout. Test only. |
| `Test/MI_FrozenLakeIce_Black_Darker` / `_Lighter` | The black-ice readability bracket (test only, see below). |

Tools are in `Source/Tools/FrozenLake/Ice`:

- `gen_ice_textures.py` (plain Python with numpy and Pillow) writes the PNGs into `textures/`.
- `build_ice_material.py` (Unreal Python) creates the assets, or **updates them in place** when they exist.
  - It never deletes a node. It retunes the parameter defaults, adds new parameter nodes and replaces the Custom
    node's code.
  - It checks the graph first. If any Custom input is unconnected or the graph is not the one it built, it raises and
    saves nothing.
  - The noise and neutral textures are imported only when they are missing. Only the test mask is re-imported.
  - Even so, run it only in a throwaway editor and never while another editor has the master loaded. That editor would
    keep a stale copy, and saving it would overwrite the update.
- `ice_headless_test.py` and `run_ice_test.sh` run a throwaway DebugGame editor on `RTS_ToCopySmall`.
  - The run builds or updates the assets and lays out a test lake (never saved).
  - It starts PIE and takes pictures from a 40 m, -55° camera into `Saved/FrozenLake/ice/`.
  - Post-process materials are off for the pictures (`ShowFlag.PostProcessMaterial 0`), because the test map's fog of
    war blacks out the unexplored lake. In seen areas the fog does not darken.

## How the look works

One number, the **thickness** `T`, drives everything: 1 = heavy (white), 0.5 = medium (grey), 0 = infantry only
(dark blue-grey).

- **Borders, measured in world centimetres.** The shader finds the world-space gradient of the mask's `T` (screen
  derivatives of `T` and of the world position), and from it the distance to the `T` = 0.75 and `T` = 0.25 crossings.
  - The blend is `BorderWidth` wide (250 cm), whatever the mask's blur, and centred on the crossing. The crossing is the
    gameplay truth.
  - `BorderWobble` (70 cm) makes the border ragged without moving it far.
  - **Edge lines** (`RimWidth` 70 cm, `RimStrength`):
    - heavy edge: a white pressure ridge of broken blocks just inside the heavy ice, and a dark wet seam just outside;
    - black edge: a pale shear crack on the medium side, and a dark seam on the black side.
  - Both lines read from 40 m and from 90 m (`A_borders_close.png`, `W_mid_90m.png`).
  - With `UseStateMask` off (constant `T`) there is no border and no line.
- **World space only.** The mesh UVs are not used.
- **Tiling.** One noise texture is sampled at about 173 m, 37 m and 6.4 m, plus a wind-stretched sample.
  - No tile repeat shows over the 586 m test lake or the real lake.
  - Stretched noise read as diagonal flecks from above, so it now only shapes things:
    - medium snow dust comes from unstretched noise (`DustAmount`);
    - the heavy snow drifts are led by the 173 m macro noise (`DriftWidth` 30 m);
    - the black-ice sky sheen comes from unstretched noise.
- **Black ice** (infantry only) is a dark blue-grey, not black, so units and their shadows stay readable:
  - a soft sky sheen (`SkyReflect`, `SkyReflectColor`);
  - wind frost (`FrostAmount`) and fine frost scratches (`ScratchStrength`, `ScratchLength`/`Width`);
  - `BlackRoughness` 0.34, which tames the sun blob;
  - parallax depth: bubbles 35 cm down (`BubbleDepth`), roots 3 m down (`RootDepth`).
- **Roots** (amber, `VeinColor`): they show only under black ice, scaled by mask A or `VeinAmount`.
- **Warn, "cryo failing"** (mask G or `WarnGlow`). The colour is a pale cyan, far from the amber roots.
  - The cracks widen and go dark and wet (`WetCrackColor`).
  - The surface gets wetter and darker, most on snow (`WarnWetness`).
  - Cyan light leaks along the crack edges.
  - A cyan rim pulses at the edge of the area, where G = 0.3 (`WarnRimWidth`, `WarnEmissive`, `WarnPulseSpeed`).
  - The centre of the area stays calm, so steam VFX can sit there.
  - The rim needs a mask gradient. A whole plane with `WarnGlow` set gets only the cracks and the wetness.
- **Open water** (mask B or `OpenWater`): a jagged edge (`HoleEdgeNoise`) with a slush rim and small floes near the
  edge.
  - On `_Lake` (Masked) the water pixels are cut away; the slush rim and the floes stay as ice.
  - On the opaque type instances the water is drawn as dark water.

### Vehicle tracks (round 4)

The ice reads the same ruts as the landscape. `UTankTrailSubsystem` draws every tracked tank's twin ruts by world XY
into the landscape-wide R8 `RT_TankTrails` (16 cm per texel; 4725² on `Ger_FrozenLake`), so tanks on the ice already
leave strokes there.

- **Reading the ruts:**
  - The Custom node takes the RT (`TrailRT`) and `MPC_TankTrails` (`TrailBoundsMin`, `TrailBoundsInvSize`,
    `TrailEnabled`).
  - It samples 5 taps through `View.SharedBilinearClampedSampler`, inside `[branch] if (TrailEnabled > 0.5)`, so it
    costs no sampler slot. Off in the editor and outside the RT, it fetches nothing.
  - The coverage is the centre tap blended with its 4 neighbours, so the binary 16 cm texels step less. The slope comes
    from the ±`TrackReachTexels` taps, and the berm from the neighbour maximum.
  - The fade is the subsystem's global fade. Nothing in the ice fades on its own.
- **One look per level,** driven by the thickness (so it follows the state mask):
  - **Heavy (snow over ice):** like the landscape snow ruts (`ls_frozenlake_snowruts.py`), with the same defaults so
    the shore and the lake match:
    - a compacted, shadowed trough (`TrackSnowTint` (0.30, 0.34, 0.44));
    - pale scoured ice where the rut is deepest (`TrackExposeHeavy` 0.6), instead of peat;
    - a bright berm (`TrackBermHeavy` 0.3);
    - a steep normal (`TrackNormalHeavy` 5.5).
  - **Medium (grey ice):** little depth (`TrackNormalMedium` 1.0). Whitish abrasion scuffs along the track
    (`TrackScuffMedium`, `TrackScuffColor`), and fine pale crack lines along both rut edges.
  - **Black (clear ice):** almost no depth (`TrackNormalBlack` 0.3). Two faint white scratch lines per rut edge
    (`TrackScratchBlack`) and a little frost powder. The amber roots stay visible, because they are emissive.
  - **Open water:** no tracks.
- **Normal:** the master now writes a **world-space** normal (`Tangent Space Normal` off). It is (0, 0, 1) plus the rut
  slope, so the mesh tangents (and the mirrored OBJ import) don't matter.
- `TrackStrength` (default 1) scales everything. Set it to 0 to switch the tracks off on one instance.

### Why opaque, and Masked for the lake

- Translucent ice would sort badly against units sinking through it, and it gets no DBuffer decals (tracks, craters).
  So the ice is opaque, and the depth of clear ice is faked with parallax.
- The lake instance overrides the blend mode to **Masked**. The `OpacityMask` output already cuts exactly where the
  material draws water. Masked keeps depth writes, decals and correct sorting. Through the holes you see the
  translucent water plane, sunk hulls and the lakebed.
- The ice still casts shadow onto everything under it. Only the sun coming through a hole lights the bed and the
  hulls there, which looks right.

## Parameters

Groups as they appear in the instance editor:

| Group | Parameters |
|---|---|
| 1 State | `BaseThickness`, `WarnGlow`, `OpenWater`, `VeinAmount`, `UseStateMask` (static switch), `StateMask`, `LakeOrigin` (x, y in cm), `LakeSize` (cm) |
| 2 Look | `BorderWidth`, `BorderWobble`, `RimWidth` (cm), `RimStrength`, `RidgeColor`, `SeamColor`, `MacroVariation`, `DustAmount`, colours (`SnowColor`, `ScouredIceColor`, `GreyIceColor`, `GreySnowColor`, `CrackColor`), `SnowRoughness`, `GreyRoughness`, `CrackWidth`, `CrackStrengthGrey` |
| 2b Look black ice | `BlackIceColor`, `BlackRoughness`, `SkyReflect`, `SkyReflectColor`, `FrostAmount`, `FrostColor`, `ScratchStrength`, `CrackStrengthBlack`, `BubbleDepth`, `BubbleAmount` |
| 3 Tiling | `MacroScale`, `MidScale`, `FineScale`, `DriftLength`, `DriftWidth`, `WindAngle`, `ScratchLength`, `ScratchWidth` |
| 4 Vein (black ice) | `VeinColor`, `VeinEmissive`, `VeinHalo`, `VeinPulseSpeed`, `RootDepth`, `RootScale`, `RootSharpness` |
| 5 Warn (cryo failing) | `WarnColor`, `WarnEmissive`, `WarnPulseSpeed`, `WarnRimWidth`, `WarnWetness`, `WetCrackColor` |
| 6 Open water | `WaterColor`, `SlushColor`, `WaterRoughness`, `HoleEdgeNoise` |
| 7 Vehicle tracks | `TrackStrength`, `TrackRutLow`/`High`, `TrackReachTexels`, `TrackNormalHeavy`/`Medium`/`Black`, `TrackSnowTint`, `TrackExposeHeavy`, `TrackBermHeavy`, `TrackScuffMedium`, `TrackScuffColor`, `TrackScratchBlack` |

Water (`M_FrozenLakeWater`): `FadeDepth` (cm of water until it is dark), `ClearOpacity`, `DeepOpacity`,
`ShallowColor`, `DeepColor`, `SkySheen`, `SkySheenColor`, `WaterRoughness`, `CrumbScale`, `CrumbAmount`,
`CrumbColor`, `DriftSpeed`.

Look tuning lives in the master's defaults (`build_ice_material.py`), so `MI_FL_IceLake` picks it up.

## How the map should use it

- **Ice mesh:** one lake-shaped mesh at z 0 (from the map session, see [FrozenLakeMap.md](FrozenLakeMap.md)). The mesh
  follows the outline, because a flat plane under the land grows a navmesh tunnel. Material: a child of
  `MI_FrozenLakeIce_Lake`.
  - If the child is parented straight to the master, it is opaque: override the blend mode to Masked, or reparent it.
  - The mesh is Nanite, so the master has `bUsedWithNanite`.
- **Water plane (required with Masked):** the same outline mesh at about z -20 cm with `MI_FrozenLakeWater`, no
  collision.
  - Set its component's **Translucency Sort Priority to -10**, so steam VFX over the holes draws on top of it.
  - Without the water plane, the holes show the bare lakebed.
- **Open leads are geometry, not paint.** The outflow lead and the two steam leads must be holes in the ice mesh, or
  blocked on the navmesh. Mask B is visual only. Paint B along a lead so the slush rim lines up with the cut.
- **Scale:** at 1024² over about 400 m, one texel is about 40 cm. That is fine for zone borders and holes of 2 m
  radius or more.

## State mask format (the hook for the mechanics)

`uv = (WorldPosition.xy - LakeOrigin.xy) / LakeSize`: u follows world X (north) and v follows world Y (east). In the
image, north is to the right and east is down.

**Texture settings:** sRGB **off** (with sRGB on, 0.5 thickness reads as black ice), `VectorDisplacementmap` or
uncompressed, clamp addressing.

**Bake:** blur the thickness channel R across each border, with a radius of about 1.5 to 3 m. The material measures
the border width itself. It only needs a smooth slope through the 0.25 and 0.75 crossings: a hard step gives broken
lines, and a very wide blur gives a mushy border. Blur G (warn) over several metres, so the rim has a slope to sit on.

| Channel | Meaning | 0 | 1 |
|---|---|---|---|
| R | thickness `T` | infantry only (black) | heavy (white); 0.5 = medium. Borders at 0.75 and 0.25. |
| G | warn, "cryo failing" | none | full. The pulsing rim sits where G = 0.3. |
| B | open water (holes, melted ice) | ice | water. The cut is at about 0.5, with a noisy edge and a slush rim before it. |
| A | Radixite vein under the ice | no roots | roots visible (only where the ice is black) |

When `UseStateMask` is off, `T` = `BaseThickness`, warn = `WarnGlow`, open = `OpenWater` and vein = `VeinAmount`.
When it is on, `T` = mask R, warn and open are `max(mask, parameter)`, and the vein is mask A.

### How the mechanic code should drive it (not built)

Proposed shape, modelled on the tank-track trails:

1. **A `UFrozenLakeIceSubsystem`** owns a `UTextureRenderTarget2D` (RGBA8, 1024², linear, clear colour
   (0.5, 0, 0, 0)).
   - At BeginPlay it copies the authored `T_FL_IceState` into it.
   - It sets `StateMask` on the lake's dynamic material instance. `LakeOrigin` and `LakeSize` stay as authored.
2. **Per-area type switch** (lane preparation, refreeze, thaw): draw soft-edged shapes into R. The gameplay grid
   should read the same values, so the look and the rules agree. Heavy is `T` > 0.75 and black is `T` < 0.25.
3. **Warn:** write G for the pump zone at the start of the 10 s warning. Steam VFX can spawn inside the zone.
4. **Melt front** (5 m/s from the pump): each frame, draw a growing disc into B. Ahead of it, draw a ring about 15 m
   wide into R, going down to 0. Keep G on ahead of the front. `C_hazards_rts.png` shows this state, painted by hand.
5. **Holes:** draw a disc into B. Fade B back to 0 to refreeze. B is visual only, so the hole must also block
   navigation.
6. **Vein:** paint A once, with the map.

## Cost

From `MaterialEditingLibrary.get_statistics` after PIE (SM5 base pass, `Saved/FrozenLake/ice/test_result.json`):

| Permutation | Pixel shader | Vertex shader | Samplers |
|---|---|---|---|
| Type instances (opaque, `UseStateMask` off) | 767 (669 before tracks, +98) | 284 | 4 |
| `MI_FrozenLakeIce_Lake` (Masked, state mask on) | 779 (681 before tracks, +98) | 284 | 5 |
| `MI_FrozenLakeWater` (translucent, forward-shaded) | 1625 | 341 | 8 |

- **Ice:** the Custom node reads the noise texture 9 times; with the mask on, there is one more fetch. Round 2
  added about 125 instructions over round 1 (542), for the world-distance borders, the edge lines, the scratches and
  the warn rim.
- **Tracks:** +98 instructions, no extra sampler. 5 RT fetches, only inside the `[branch]` (trails on, pixel inside
  the RT). Static instruction counts include the branch whether it runs or not.
- **Water:** the high count is the forward-shading translucent lighting path, not the node, which makes 2 noise
  fetches. Opaque ice in front of it rejects its pixels by depth, so it only costs pixels inside the holes.
- **Masked:** the ice keeps early depth rejection, but the depth pass runs the opacity part of the node.
- No quality switch yet. A low path could drop the parallax root sample, the scratches and the edge lines.

## Radixite colour

`VeinColor` is (1.0, 0.54, 0.0), the normalised glow of the world's Radixite crystals (`MM_Crystal_Emissive`,
`Cristal_3A_Yellow_Cristal_03_Emissive`, average sRGB (0.87, 0.66, 0.0)). The minimap draws Radixite green, but the
crystals are amber. The warn moved to cyan, so the two no longer clash.

## Test results (`Saved/FrozenLake/ice/`)

**Round 4, vehicle tracks** (`TR_*.png`; `bash run_ice_test.sh tracks`):

- **How the test works:**
  - It starts on `Ger_FrozenLake` in PIE (never saved; the map, `RT_TankTrails` and `MPC_TankTrails` files had the
    same md5 before and after).
  - The trail subsystem is switched off (`rts.TankTrails 0`).
  - Six tanks' twin ruts (60 cm wide, 2.6 m apart, one half faded) are drawn into the subsystem's own 4725² target, in
    its own mapping, across a 164 m square around (21 m, -4 m) where heavy, medium and black ice meet.
  - `MPC_TankTrails` is re-applied before every shot.
- **Shots:** `TR_tracks_rts` (40 m), `TR_heavy_close`, `TR_medium_close`, `TR_black_close`, `TR_tracks_low`, and
  `TR_tracks_rts_off` (the same view with trails off).
- **Pitfalls found:**
  - Resizing `RT_TankTrails` mid-PIE (as the editor-world pattern in `c_rut_test.py` does) left the material sampling
    the old resource, and nothing showed. Draw into the subsystem's own target instead.
  - `rts.TankTrails 0` takes effect on the subsystem's next tick and writes `TrailEnabled` 0. Set the MPC after that.
- **Known:** the rut edges staircase at 16 cm per texel in close views (the RT is binary per texel, and the
  subsystem's triangles are not antialiased). The neighbour blend softens it a little; from the 40 m RTS camera it
  reads as a soft edge. The landscape snow ruts share the same limit.

**Real-tank bench** (2026-11-17):
`MAP=/Game/RTS_Survival/Maps/GerMissions/Ger_FrozenLake RES=2560x1440 bash Source/Tools/TankTrails/run_bench.sh fl_ice_tracks "-TankTrailBenchAt=10974,-10455"`.
Output is in `Saved/TankTrailBench/fl_ice_tracks/`. The map was not saved (its md5 was the same before and after).

- **Spot:** 40 Pz IV H in 8 lanes at (110 m, -105 m), entirely on the lake mesh. The south half of the lanes is heavy
  ice; the north half is grey ice with a little black.
- **Strokes:** 32,891 trail texels, all on the lake. 91% are on heavy, 9% in the heavy/grey blend band, none on black
  (`rt_strokes_over_ice.png`: the exported RT over the ice types).
  - The capture crop (`TankTrails_View_crop_heavy.png`) shows the subsystem's real ruts on heavy ice with the snow look:
    blue-grey troughs and pale berms.
  - The strokes are mostly small loops, because the tanks never reached their lane ends (below).
- **GPU off/on pair delta:** median -0.67 ms (min -2.3, max +7.3) over 10 pairs, which is noise. Two other sessions'
  editors were running, and some phases spiked to 30+ ms GPU on both sides. Writer cost: 44–60 µs per update
  (one 127 µs outlier).
- **Nav (for the map session):** no tank touched black ice. But none entered **grey** ice either: the whole column
  stopped in a line at the heavy/grey border. That contradicts the editor probe, where the MediumTank agent reaches
  grey ice. The bench logged "navigation ready after 60 s (still building: yes)" and 43 "Looking for NavData using
  invalid FNavAgentProperties" warnings at spawn. The likely causes are that Pz IV H resolves to the HeavyTank agent, or
  that the runtime navmesh was still rebuilding. Not investigated (nav is the map session's area).
- **Not covered by real strokes:** the medium and black looks. They are checked only with the hand-painted ruts.

**Round 3 in `Ger_FrozenLake`** (`FL_*.png`; `bash run_ice_test.sh map`): the map is opened in a throwaway editor and
never saved (its md5 was the same before and after). The test lake floats 300 m up and 700 m north, with no shadows,
so it does not touch the real lake.

- **`FL_R_overview`:** the real lake. The three types and the amber vein read clearly, and there are no diagonal flecks.
- **`FL_R_vein_black_rts`, `FL_R_border_heavy_rts`, `FL_R_border_black_rts`, `FL_R_heavy_rts`:** borders and edge
  lines in winter light. Soldiers read on the black ice.
  - The Pz IV in `FL_R_border_heavy_rts` stands on black ice only because the test placed it badly; it says nothing
    about the nav areas.
- **`FL_R_lead_rts`:** the outflow lead's water is grey-green with a sky sheen, no longer flat black.
- **`FL_C_hazards_rts`, `FL_C_warn_heavy_rts`:** the warn rim is a thin cyan line now, with no white bloom. The cracks
  go dark and wet.
- The test lake's holes show grey here, because there is 300 m of air under them. The see-through check is the round-2
  `C_hole_low` on the test map.

**Round 2 on `RTS_ToCopySmall`** (warm desert light):

- **`A_types_mask_rts`, `A_borders_close`:** narrow borders with the ridge/seam lines, and units readable on black ice.
- **`D_types_separate_planes_rts`, `E_black_bracket_rts`:** the black-ice bracket, on strips left to right: default,
  darker, lighter. Each has a squad and a Pz 38(t).
  - Darker: `BlackIceColor` (0.026, 0.034, 0.042), sky 0.22.
  - Default: (0.045, 0.058, 0.07), sky 0.35.
  - Lighter: (0.07, 0.088, 0.105), sky 0.45, more frost.
  - Units read on all three; the owner picks. The values go into the master defaults.
- **`C_hazards_rts`, `C_hole_low`:** see-through melt hole. Visible through the water: a Pz 38(t) fully under it, tank
  shadows on the lakebed, and the lakebed's ripples.
- **`C_warn_heavy_rts`:** warn on heavy ice. The dark wet cracks and the wet darkening read well. The rim was too
  bright (it bloomed white); fixed in round 3.
- **`W_mid_90m`, `W_wide_tiling`:** no streak grain on medium ice, and no tile repeat.

## Not verified / open

- **Build log noise.** An in-place update logs one "Custom material ... missing input 1 (WP)" compile failure per
  material. It comes from the moment between replacing the input list and reconnecting it. The final compile is clean:
  the statistics after PIE are non-zero and the shots render the material.
- **Lead edges.** The edges of the map's mesh-cut leads show small blocky slush bits along the cut. Smoothing the mask's
  B channel along the leads would soften them.
- **Exposure.** The real map's exposure is the game's; the map session's scene captures use a fixed exposure.
- **Test units.** The tanks are training-preview meshes with the Tiger forest camo. The soldiers are `SK_RailTrooper`
  in bind pose. The camera is a `CameraActor` with FOV 90, not the real camera pawn.
- **Masked depth-pass cost** on a Nanite mesh was not profiled.
- **Tracks:** real subsystem strokes have been seen on heavy ice only (the bench tanks stayed on heavy ice). The
  medium and black looks are checked with hand-painted ruts. The fade was not watched.
- **Not run:** a render target on `StateMask`.
- **Not checked:** the parallax depth and the pulses in motion, low-quality scalability, fog-of-war colouring over the
  ice, night lighting, and DBuffer decals on the ice.
