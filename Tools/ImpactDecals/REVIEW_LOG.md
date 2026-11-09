# Landscape impact decals - adversarial review log

Tools: `gen_atlas.py` (procedural atlas), `preview.py` (lit DBuffer-emulating previews on exported project ground
textures), `ue_build_material.py` (import + material build in the editor).
Each round: fresh critic subagent got the preview sheets + raw atlas and the design goals only.

Preview sheets per round folder: `sheet_close.png` (16 cells x 4 grounds, texture-res), `sheet_rts.png` (true RTS
scale 4.5 cm/px, 2x), `sheet_battlefield.png` (40x24 m scatter on grass and sand).

## Round 1

Before: `Saved/ImpactDecals/round1_before/` (first full set, = r0). After: `Saved/ImpactDecals/r1/`.

Critic pointers (ranked):
1. B2-B7 bowls read as grey boulders: hard polygon outline, lit wall brighter than the grass, no AO.
2. No visible raised rim or ejecta rays from B3 up; normal ends abruptly at the bowl edge.
3. Normal faceting / radial pie-slice pinching at the centre; posterisation on the walls.
4. B2-B7 one design scaled up; crater only 30-35 % of the cell; B7 crater ~2.2 m, should be 4-6 m (65-75 % of box);
   B3/B4 should be saucers, B6 a deep steep bowl.
5. B (oblique) variants are just stretched A craters; need shallow uprange entry, steep downrange wall, ~70 %
   ejecta in a downrange fan, clean uprange.
6. Spray halo is a soft translucent cloud disc (cold grey ring on sand, dirty glass on grass); want on/off speck
   coverage thinning with distance; warmer dust.
7. Soot invisible.
8. Clods read as confetti: same-size triangles, black on sand, blue chips in B6/B7; want size falling with
   distance, lumpy, damp brown, mostly along rays; B7 ~120-150.
9. B0B/B1B furrow = black cigar; want taper thin entry -> wide deep exit, side ridges, end throw, not black;
   B1B wider/shorter, torn end.
10. B0A and B1A the same design (B0A should be a scuff + tiny pit, B1A a real 12-15 cm pit with broken lip).
11. Repetition: memorable crater silhouettes in the battlefield sheet.
12. Content fills ~40 % of the cell; some spray near the cell edge (mip bleed risk).
13. On rock everything looks like a soil crater punched into stone.

Changes made (generator rewritten around a new per-band table):
- Crater radii re-designed from real crater sizes: Rc 2.4 / 7 / 16 / 29 / 41 / 60 / 98 / 163 cm (B7 crater 3.3 m,
  rim to ~4.3 m of the 6 m box). B3/B4 are shallow saucers (thin rim), B6/B7 deep floor+wall bowls with terraces
  and slumps. (1, 4)
- Bowl profile = flat-ish floor + wall over rn 0.2..1 (no knife-edge wall); removed a hidden height step at the
  blanket start (thin black outline line) found while checking. (3)
- Non-directional AO baked into albedo (floor ~55 % of soil), lighter dry rim crest, warmer brown palette. (1, 2)
- Rim height/width per band (0.26-0.38 Rc), blanket r^-3; rays 8-16 per band, wider, clods 50 % along rays. (2)
- Polar (streak) noise blends into isotropic noise near the centre: no pie-slice pinch. Atlas written dithered. (3)
- Oblique variant: asymmetric bowl (long shallow uprange ramp, steep downrange wall, deepest point downrange),
  downrange-weighted rim, rays/specks/clods in a downrange fan, uprange clean. (5)
- Speck field is now thresholded on/off coverage thinning with distance (no translucent wash), warmer dust. (6)
- ScorchStrength default 0.6 -> 0.75; soot concentrated inside ~1.3 Rc, on the rim and rays. (7)
- Clods: lumpy rounded domes (2nd/5th harmonic outline), size falls with distance, damp brown; B7 145 clods +
  26 warm-grey stones (no blue). (8)
- Furrow redesigned: tapering groove (entry 1.6 -> exit 3.6 cm on B0, 3.5 -> 8.5 cm on B1), side ridges, torn heap
  at the exit, spray thrown downrange; B1B wider/shorter than B0B. (9)
- B0A = scuffed 11x7 cm patch + 5 cm pit; B1A = 14 cm pit with a broken lip. (10)
- Outline: weaker low-order lobes, more mid-frequency breakup. (11)

Rejected / partly:
- (12) alpha is already hard-zero beyond r = 0.45 D (>= 26 px at the cell edge midpoints, more in the corners); the
  critic's "spray within 20 px of the edge" was the zero-alpha neutral fill. Content now fills more of the cell via
  the bigger craters instead.
- (13) one atlas serves every ground by design; soil-on-rock is the accepted compromise (no rock-specific cells).

In-engine check after round 1 (StartGame map, Meadow landscape, actors removed afterwards, level not saved):
`Saved/ImpactDecals/ingame/r1_meadow.png`, `r1_meadow_b7close.png`, `r1_rotation_slope.png` (same cell at yaw
0/90/180/270 all read as holes -> U and V normal signs correct), `r1_slope_probe.png` (cube standing in a B6 decal: no
smearing on the vertical faces -> slope fade works).

## Round 2

Before: `Saved/ImpactDecals/r1/` (+ in-engine r1 shots). After: `Saved/ImpactDecals/r2/`.

Critic pointers (ranked):
1. B0-B2 unreadable at RTS scale (B0 core 1-2 px); exaggerate core/scorch 2-3x, spray opacity 0.5-0.7.
2. B2-B5 one design scaled; give each band a signature (B2-3 pit + scorch fan, B4-5 shallow bowl thin rim short
   splash, B6 rays, B7 deep multi-lobed slumped bowl with long rays to the edge).
3. Crater core albedo near-black -> double darkening with normal shadow + soot (ink-hole crescent in engine);
   clamp floor albedo, let the normal map make the shadow, soot only on lip/rays.
4. Central rubble lump in B3-B7 bowls reads as a volcano/donut; floor should be a smooth dish.
5. Clods = stamped pebbles in a "string of pearls" around the rim, light tan in engine; want power-law sizes,
   clustered along rays/downrange, darker/damper, softer edges, less relief.
6. Apron = visible flat-alpha disc with cauliflower outline; lighter tan halo in engine.
7. Oblique B variants physically backwards (dark comet tail uprange); want stretched downrange, clean uprange,
   at most a narrow entry gouge.
8. Repetition: A cells too symmetric so random yaw adds nothing; build in off-centre core, lopsided splash,
   3-5 dominant rays.
9. Normal faceting/banding; terraces posterised; normal intensity per world-cm inconsistent.
10. Furrows still cigar-like and too thin for 4.5 cm/px.
11. Apron runs warm olive; desaturate.
12. Preview vs engine disagree (engine relief much stronger, apron albedo vanishes); retake engine shots in game
    view, RTS height, front + back light.
13. Texel budget wasted in B0/B1.

Changes made:
- B0/B1 exaggerated ~2x (B0 Rc 3.6, scuff 17x12 cm; B1 Rc 9), denser speck field (per-band `density`). (1, 13)
- Per-band signatures: B2 pit + soot fan (`fan`), B3 saucer + thin rim, B4/B5 shallow bowl/thin rim/short splash,
  B6 strong rays (`ray_boost`), B7 multi-lobed bowl (`lobed`) with rays to ~0.44 D. (2)
- Crater albedo floor clamp (FLOOR_MIN), AO reduced (0.42 -> 0.25), soot removed from the bowl centre. (3)
- Floor relief removed (no terraces, slumps only on the walls). (4, 9)
- Clods: power-law sizes (u^3), 55 % along rays weighted to dominant rays, darker damp colours, softer edges,
  height ratio 0.40 -> 0.28. (5)
- Apron opacity now a radial curve streaked along the rays, value just below mid grey, crest lightening reduced. (6, 11)
- Oblique: bowl stretched downrange, uprange narrowed into an entry gouge, ejecta fan downrange only. (7)
- A cells: bowl off-centre by 0.12 Rc, splash lopsided towards a random direction, 3-5 dominant rays. (8)
- Furrows widened (B0B exit 6.5 cm, B1B 12.5 cm), broken edges. (10)
- Engine shots retaken in game view, 30 m and 50 m, front- and back-lit. (12)
Rejected / partly:
- (9) "normal intensity per world-cm inconsistent": small bands are steeper on purpose (a 20 mm pit is a sharp
  hole); kept.
- (12) did not calibrate the preview renderer; tuned against engine captures instead.

In-engine after round 2: `ingame/r2_meadow_backlit.png`, `r2_meadow_frontlit.png`, `r2_meadow_30m.png`,
`r2_tint_probe.png` (B7A tinted red through a dynamic instance: confirms the colour path and the opacity footprint).
Own finding: on the olive meadow the soil value (sRGB ~0.33) was not darker than the ground, so the apron vanished.

## Round 3

Before: `Saved/ImpactDecals/r2/` (+ in-engine r2 shots). After: `Saved/ImpactDecals/r3/`.

Critic pointers (ranked):
1. In engine only the bowl wall reads; rays exist almost only in the normal map; pre-existing star-splat decals
   out-read every crater. Put 12-24 tapered rays into albedo + opacity (0.6-0.85 on, <= 0.15 between), reaching
   0.85 of the cell radius on B4-B7.
2. Bowls read as four-sided pyramids / diamonds with straight seams.
3. Normal posterisation; add fine soil noise on the walls, blur before the derivative.
4. Walls too steep (saturated normals -> black hard-edged mouth); cap tilt ~35 deg, raised lit rim.
5. B2-B5 still one decal at four sizes; B2B-B5B barely differ from A.
6. Same 4-6 grey pebbles in every big crater floor (repeated "face"); floor flat pale grey like a concrete pan;
   floor should be darker than the rim, mottled.
7. Clods read as black paint chips (hexagonal); lumpy outlines, domed normal, lit tops, roughness ~0.7,
   70 % along rays, density ~1/r^2.
8. Haze is a grey noise disc to ~0.85 R; drop to <= 0.12, lobed boundary tied to the rays.
9. B0/B1 still invisible; bold dark core ~30 % of the cell, spray streaks; furrow darker/wider (~25 % of cell).
10. Preview flatters; engine floor turns pale (grazing specular?); roughness 0.85-0.95 except clods.
11. Soot almost absent in engine; 6-10 radial soot wedges to 0.3-0.5 R.
12. Warm the outer haze on sand (R/B ~1.08).

Changes made:
- Rays: 8-22 per band, tapered (wide root -> thin tip), dark RAY albedo, opacity 0.88 x strength (0.6-1.0),
  clumpy along their length; B4-B7 reach 0.42-0.44 D; oblique fan limited to +-60 deg downrange. (1, 5)
- Outline lobes 2/3/5/7 (removed the square k=4 term), oblique entry gouge softened (less kite). (2)
- Height blurred 1.2 px before the derivative + fine soil noise (2.5 % of depth) on walls. (3)
- Depth/diameter ~1:4 (B6 50 cm, B7 85 cm, B5 24 cm) and a soft slope cap at 55 deg in the normal derivation;
  rim crest lightened (DUST, 55 %) so the lit near rim reads. (4)
- Floor pebbles removed (a few clods low on the walls only); floor darker than rim, mottled. (6)
- Clods: 2/3/7-harmonic lumpy outlines, height ratio 0.36, lighter tops, roughness ~0.72, 70 % along rays,
  density falling with r^2. (7)
- Haze specks opacity 0.85 -> 0.6 and multiplied by proximity to the rays (0.25 between rays); apron boundary
  +-25 % lobes (k = 4/5/6). (8)
- B0 Rc 4.2 / B1 Rc 10, centre soot 0.70; furrows B0B 9.5 cm, B1B 17 cm exit, soot along the furrow. (9)
- Roughness 0.88-0.95 everywhere, 0.68-0.75 on clods. (10)
- Soot: radial wedge mask (polar streak noise, narrow arcs) over the scorch radius, stronger mask scale. (11)
- Overall value darkened from my own engine finding: apron sRGB 0.30, floor 0.22, rays 0.19, clods 0.17. (own)
Rejected / partly:
- (4) cap at 55 deg rather than 35 deg: at 35 deg (tried, see r3 first pass) the bowls flattened into ink splats in
  the preview; the cap is a soft limit so typical walls land around 35-45 deg.
- (8) haze opacity kept at 0.6 in specks (coverage, not a wash) instead of a 0.12 wash.
- (12) warming the haze: kept neutral; any neutral looks cool against orange sand, and warmer haze goes olive on
  grass (round 2 pointer 11 asked for the opposite).

In-engine after round 3 (final textures, reimported 2026-11-09 00:47): `ingame/r3_meadow_30m.png` (backlit),
`ingame/r3_meadow_frontlit_30m.png` (front-lit, 30 m), `ingame/r3_close_b6b7.png`, `ingame/r3_probe_tint_normal.png`
(B7A with NormalStrength 0 shows the albedo + rays alone; B6B with Tint 0), `ingame/r3_alpha_pair.png` (B7A at
DecalColor.A 1.0 left vs 0.5 right; a pre-existing level splat overlaps the middle). Finding: backlit, the rough
soil floor picks up a grazing specular sheen and reads pale grey; front-lit the craters read as dark holes. Tunable
on the MI (Tint, RoughnessMul) without regenerating.

Final previews: `Saved/ImpactDecals/final/` (contact_sheet_all_bands.png, sheet_rts.png, sheet_battlefield.png).

## Summary of the three rounds
1. Round 1: re-designed every band from real crater sizes (B7 crater 0.3 -> 3.3 m), floor+wall bowls, rims,
   blankets, oblique/furrow variants, on/off speck fields, lumpy power-law clods; fixed a hidden blanket height step.
2. Round 2: exaggerated B0/B1 for readability, per-band signatures (scoop/fan, saucer, thin rim, rays, lobed bowl),
   off-centre lopsided A cells, smooth floors, downrange-only oblique ejecta, wider furrows.
3. Round 3: rays moved into albedo/opacity (tapered, clumpy, 8-22 per band), gentler 1:4 bowls with a soft slope
   cap, floor darker than rim with no centre pebbles, ray-tied sparse haze, radial soot wedges, rough soil.
4. Throughout: in-engine checks for cell selection, normal sign (four yaws), slope fade (cube probe), colour path
   (tint probe) and DecalColor.A.
5. Overall value darkened (apron sRGB 0.30) after the engine showed the r2 soil was not darker than an olive meadow.

---

# 2026-11-09: bomb craters + larger shells (B3-B7)

Jobs: (1) shells 75 mm and up get bigger decal boxes (B3 160->240, B4 210->330, B5 290->450, B6 400->600,
B7 600->850 cm) and are re-designed at the new size; B0-B2 stay byte-identical (checked against `r3/` after every
generator change: max abs diff 0 for cells 0-5 in both textures). (2) a new bomb crater atlas: `gen_bomb_atlas.py`,
4 weight bands (K0 250 kg / 9 m box, K1 500 kg / 12 m, K2 1000 kg / 16 m, K3 2500 kg / 22 m) x 4 variants
(round, lopsided, double, broken), `T_BombImpact_BCO/NRS`, `MI_BombImpactDecal` (same master, texture parameters).
Previews: `preview.py --set bomb` (contact sheet, bomb next to B6/B7 at RTS scale, battlefield with a bomb stick) and
`preview.py --old-vs-new` (old and new shells at one common 4.5 cm/px). Each round a fresh critic subagent got the
image pack (`Saved/ImpactDecals/review_roundN/`) and the design goals only.

## Round 1 (bomb_r0 + shell_r0 -> bomb_r1 + shell_r1)

Before: first bomb set (bowl crest at ~0.27 D, rim+blanket squeezed into ~0.08 D, neutral grey, clods/blocks scaled with
the band, 13-140 cm) and shells re-designed at the new D (Rc 42/60/84/130/205 cm, physical speck/mottle/erode sizes).

Critic pointers (ranked):
1. Bombs: bowl too big, blanket a thin ring -> "grey cookie". Crest ~0.27 D, blanket 0.27-0.45 D with opacity
   0.95 -> ~0.55 -> 0, noise-thresholded so ground shows through; (suggested growing the boxes to 12/16/21/28 m).
2. Bombs: rim does not read as raised: needs low-frequency profile (steep inner wall 30-35 deg, crest ~25 % of
   depth, ~10 deg outer slope), halve the clod noise, 8-14 wall gullies / terraces.
3. Bombs: flat neutral grey on every ground (ash on sand, tar on grass); warm loam blanket, lighter fresh walls, dark
   wet floor, 2-3 subsoil tones blotched at 1-3 m.
4. Bombs: round and lopsided nearly identical (want 1.35:1 ellipse, low uprange lip, blanket 1.6x farther downrange in
   3-4 lobes); broken variant reads as a dark gash (want a lighter smooth slump tongue + crest gap); K0-3 = K1-3.
5. Bombs: clod size scales with the box -> K3 boulders and a cobbled floor; absolute sizes 80 % 15-40 cm, 20 %
   50-100 cm, none > 120; count by area; smooth floor.
6. Bombs: circular stamp silhouette; 5-9 lobes reaching 0.45 D with gaps ending ~0.33 D.
7. Bombs: K0 not clearly heavier than B7.
8. Shells: same composition scaled up; B3 core barely grew; more rays instead of fatter rays.
9. Shells: spray hairs = spider legs on sand, soft halo on grass, near-black clods; halo <= 0.35, lit brown clods.
10. Shells: new B7B a straight-sided triangle with a sharp uprange apex.

Changes made:
- Bombs rewritten: crest Rc = 0.255-0.258 D for every band (K0 232 cm, K1 308, K2 410, K3 560; bowl 4.6 m vs the
  B7 shell's 4.1 m), blanket lobes reach 1.75 Rc (0.445 D), gaps stop at 1.28 Rc; blanket opacity ramp 0.95 -> 0
  thresholded by ~1 m fbm patches. (1, 6, 7)
- Rim profile: crest at rn 1, short inner face (0.13 Rc), long outer slope to 1.75 Rc; depth 0.39 Rc (soft cap keeps
  walls ~35-45 deg); crest height 32-76 cm; low-frequency crest undulation instead of high-frequency lumps; wall gullies
  (polar noise, ~0.22 rad arcs) and broken benches on K2/K3. (2)
- Warm subsoil palette: loam blanket sRGB (0.27,0.22,0.18) blotched with ochre clay and grey marl at 1.6-2.2 m,
  dried thin edge, lighter fresh-cut walls, wet dark floor (roughness ~0.78, wet patch 0.55: kept above the critic's
  0.3 because the engine showed low-roughness decal floors catching a pale grazing specular, round 3 above). (3)
- Lopsided: 1.35:1 ellipse, deepest point 0.25 Rc uprange, uprange lip at 50 %, centre shifted 0.05 D uprange, 3-4
  blanket lobes downrange reaching ~2.2 Rc, uprange blanket ~1.15 Rc. Broken: rim gaps (1 or 2 per band) with a lighter
  smooth tongue (TONGUE albedo, 15 % lighter than the wall) running to 0.3 Rc; K0/K2 one gap, K1/K3 two. Double: per-band
  secondary size/angle (0.55/0.62/0.47/0.58 Rc). (4)
- Clods absolute for all bands (80 % 15-40 cm, 20 % 50-100, cap 120), count by area (240/400/650/1000), 55 % just
  outside the crest, the rest mostly inside the lobes; crest blocks 50-110 cm (25 % lighter clay); floor smooth. (5)
- Shells B3-B7: Rc 46/64/88/130/205 (core of B3-B5 grown ~10 % more: measured visible diameter x1.55 for B3/B4,
  x1.45 B5, x1.37 B6, x1.33 B7 vs the old cells); 16/18/20/24/30 rays with absolute root widths 9-21 cm (more rays,
  not fatter); spray opacity 0.6 -> 0.42; clods 25 % lighter (lit brown); oblique bowl stretch 1.45 -> 1.25 and uprange
  narrowing 0.18 -> 0.05; outline k=3 lobe weakened (B6/B7 bowls had read triangular). All via band keys B0-B2 lack. (8-10)
Rejected / partly:
- (1) bigger bomb boxes: the box sizes are fixed by the C++ side for this job, so the layout was rebalanced inside
  9/12/16/22 m instead (noted in the report as an option).
- (3) floor roughness 0.30-0.35 (see above).

## Round 2 (bomb_r1 + shell_r1 -> bomb_r2 + shell_r2)

Critic pointers (ranked):
1. Bombs read shallower than shells: bowls one even mid grey-brown (~0.35-0.40) while the shell bowls next to them
   are near-black; floor sometimes lighter than the rim. Radial value profile wanted: floor x0.45, walls x0.65 with
   2-3 faint strata, crest x1.1, apron x0.8; standing water in 1-2 variants per band.
2. Floor too wide and flat (~55-60 % of the rim diameter); bomb craters are cones: flat floor 15-20 % of the radius,
   wall tilt 45-55 deg near the rim; bake a down-sun occlusion term.
3. The same clod pile dead centre in every cell (a "central peak"): delete; 0-3 loose clods 0.2-0.5 r off centre.
4. Still no ejecta blanket: clod necklace ends ~1.2 r, then clean ground; continuous dark apron to 0.45 D, colour
   opacity 0.8 -> 0 along 6-9 noise-lobed fingers; normal opacity above colour opacity at the fringe.
5. Clods grey pebbles; K3 reads as a boulder field: 15-70 cm power-law, flat and angular, subsoil hue x0.85, clustered
   along the fingers, density 1.0 at the crest -> 0.1 at 0.45 D.
6. Variants too alike: col 1 barely oval (1.3:1, 0.06 D uprange shift, downrange rim 1.6x thicker, apron 2x farther
   downrange, thin low uprange rim); col 3 reads as a heart, K3-3 a hard stair-step terrace line (convex outline, 60-90
   deg rim drop filled with a soft spill, no contour line); young crater in col 2 needs ragged clods on its rim.
7. K0 not clearly bigger/heavier than B7A (footprint ~6.7 m vs ~5.9 m): >= 1.4x footprint, rim normal >= 1.5x B7.
8. Shells: still the old layout scaled (near-black disc + streaks = ink blot); B6/B7 show no rim crest. Soot only in the
   inner bowl, a ring of earth with a lit slope at the crest, streaks starting outside it.
9. Shells: B6/B7 debris is black confetti; earth-coloured 10-40 cm clods + grey stones, spread to 0.9 of the box radius.
10. Shells: B7B spike / teardrop at the uprange end; B7A grey fog ring at 0.35-0.45 D; B3A-B5A rays a regular comb.

Changes made:
- Bombs, value profile: wall albedo 0.29 -> 0.215, wet floor 0.18 -> 0.15 (mixed 70 % over the floor), depth AO
  0.20 -> 0.30, apron x0.9 of the crest, crest stays the lightest; two faint lighter strata bands on the walls
  (iso-radius, noise-warped). Standing water/slurry (value ~0.10, roughness ~0.45, 13-20 % of the bowl radius) in the
  broken variant and the older crater of the double variant (2 of 4 per band). (1)
- Bombs, cone bowl: flat floor only inside 0.12 Rc, wall a 1.6-power profile steepest at the rim; depth 0.58 Rc
  (K0 135 cm ... K3 325 cm), the soft 55 deg cap in height_to_normal keeps the near-rim tilt around 45-55 deg. (2)
- Centre pile removed: at most 3 loose clods at 0.2-0.5 Rc. (3)
- Apron: gaps now stop at 1.45 Rc (~0.37 D) instead of 1.28, lobes reach 1.75 Rc; opacity 0.92 -> 0 with weaker
  patch noise (continuous near the rim, broken only towards the edge). (4)
- Clods 15 + 55 u^3.2 cm (many 15-25, a few 50-70), flat-topped angular outlines (|sin| facets), LOAM x0.80 base /
  x1.04 top, clustered on the lobes, radial density falling ~ u^1.8 to the edge; crest blocks 40-80 cm, less elongated,
  height ratio 0.22. (5)
- Lopsided: ellipse 1.42, downrange rim 1.4x higher and its outer slope 1.6x wider, uprange lip at 40 % and outer slope
  0.6x, apron uprange ~1.1 Rc vs downrange up to ~2.4 Rc. Broken: outline lobes 2.6 -> 1.1 (convex), one 78 deg gap
  (+ a 46 deg gap on K1/K3), tongue Gaussian-blurred into a soft spill (no contour), no terraces in this variant;
  terraces elsewhere wider and 30 % lower (no stair-step line). Double: crest blocks shared between both rims (40 % on
  the young crater). (6)
- K0 rim 32 -> 55 cm (B7 shell rim 38 cm), K1 70, K2 92, K3 124; the continuous apron makes the K0 footprint
  ~0.8-0.9 D. (7)
- Shells B3-B7: dark core now stops inside the crest (core_edge 1.0 -> 0.78 instead of 1.25 -> 0.85), so the lit rim
  crest / ring of earth reads between bowl and rays; rim soot 0.3 -> 0.1. (8)
- Shells: clods lifted to x1.5 of the base clod colour (lit brown), clod field to 0.43 D on B6/B7. (9)
- Shells: oblique apron keeps 30 % uprange (oblique_floor) and no uprange narrowing (stretch 1.2) -> no teardrop apex;
  B6/B7 spray opacity 0.42 -> 0.30 (fog ring); rays clustered in 3-5 groups with 0.6-1.7x width and 0.75-1.35x length
  (no comb). (10)
Rejected / partly:
- (2) baked down-sun occlusion: decals spin randomly at runtime, so any baked light direction would be wrong 3/4 of the
  time; non-directional AO raised instead.
- (4) normal opacity above colour opacity: the material has one opacity for colour, normal and roughness (DBuffer
  translucent); a second opacity would need another channel and more instructions.
- (7) bigger K0 box: kept the C++ sizes; footprint raised through the apron instead.
- (8) soot only in the inner bowl: the 2026-11-09 round-3 lesson (double darkening = ink hole) keeps the bowl centre
  light on soot; the rim soot was cut instead.

Own findings between rounds 2 and 3 (fixed before the round-3 pack): the B3-B7 ring of earth was semi-transparent
haze once the dark core stopped inside the crest (made opaque, `ring` opacity 0.94 to 1.38 Rc); the low-roughness bomb
puddle rendered as a white glint in the preview (roughness raised to ~0.68, the engine's grazing-specular lesson).

## Round 3 (bomb_r2 + shell_r2 -> bomb_r3 + shell_r3 = final)

Critic pointers (ranked):
1. K0 still not clearly bigger than the B7A shell (bowl ~4.3 m vs dark core ~4.5 m); bomb bowls small for their
   weight. Raise the boxes to 12/15/20/26 m and spend the radius on a real blanket.
2. Almost no ejecta blanket (rim ~0.30 D, alpha ends 0.45 D -> ejecta only ~1.5 R); continuous darker subsoil sheet to
   1.6 R at 0.7-0.85, 5-9 lobes to 2.2 R, isolated clod clusters; kill the uniform low-opacity veil (soft disc/"smoke
   ring" on sand).
3. Ejecta lighter than the ground; should be dark moist umber, no grey.
4. Bowl floor grey concrete with a smooth gradient (K3 a stretched funnel/cloth); warm brown floor, mid-frequency
   fallback debris on floor and inner wall, 2-3 slump terraces at ~0.6 / 0.85 R.
5. A small bright mound / white speck at the centre of several cells.
6. Rim a crimped bottle cap (radial rectangular facets), no broad berm; berm peaking ~1.05 R, steep inside, long outer
   slope, clods on top with random orientation.
7. Clods all one size and stone-grey; 3-6 big clods at 4-6 % D within 1.0-1.3 R, then many small chips; ejecta brown.
8. Col 0 and col 3 look the same (slump only a faint wedge); 70-100 deg slump sector, tongue 0.4 R into the bowl,
   ejecta to 2.4 R on that side; col 0 non-circular (+-8 % at 3-5 lobes); suspected baked directional shading.
9. Col 2 younger crater reads as a blister: let it erase the old rim, spill dark fresh ejecta into the old bowl, deeper.
10. Shells: still a 1.5x stretch; core 0.25-0.30 D, physically sized clods, low raised rim.
11. Shells: near-black core (~0.05) and hairline streaks ending on a circle (urchins); lift/warm the core, soot only
    0-0.6 R at 0.6, streaks >= 10 cm wide with varied lengths.
12. Shells: B variants triangular smears; 1.3:1 bowl, entry gouge, 100-130 deg downrange fan.

Changes made (final):
- Bomb palette warmer and darker: blanket umber sRGB (0.24,0.19,0.14), clay/marl patches kept warm (no grey), drier
  outer blanket (0.26,0.21,0.17), wall (0.21,0.16,0.12), floor (0.15,0.115,0.086); clods LOAM x0.80/x1.04. (3, 7)
- Apron opacity is now solid-or-nothing: continuous ~0.9 to ~55 % of the apron, then noise decides solid soil or bare
  ground (smoothstep width 0.08), specks below 0.3 coverage dropped -> no veil / smoke ring. (2)
- Floor and lower wall: fallback debris lumps (noise, 20 cm at K0 -> 31 cm at K3 feature size); slump benches at
  0.6 / 0.85 Rc on every band except the broken variant; wall gullies halved; crater wall roughness 0.95 (the grey
  "concrete" was a specular sheen on the sunlit wall), apron 0.84. (4)
- Centre: the 0-3 loose floor clods removed completely (the bright specks were their lit flat tops). (5)
- Rim: radial streak relief in the blanket cut from 0.25 to 0.05, crest blocks randomly oriented (were tangential ->
  bottle-cap facets); berm unchanged in shape (crest at rn 1, inner face 0.13 Rc, outer slope 0.75 Rc). (6)
- Clods 10 + 60 u^3.6 cm plus 3-6 big clods at 4-6 % D (cap 130 cm) at 1.03-1.3 Rc. (7)
- Broken: 89 deg gap, crater centre shifted 0.04 D away from the gap, tongue reaching 0.22 Rc from the centre and
  62 % fill, apron +0.55 Rc on the gap side. Round: outline lobes x1.9 (~+-6-8 %). (8)
- Double: the young crater is 0.9x the main depth (was 0.55-0.75x), its fresh ejecta spill into the old bowl (darker
  albedo x0.75, slight relief). (9)
- Shells B3-B7: core soil lifted and warmed (SOIL_DAMP x(1.22,1.17,1.10)), soot x0.7, the ring of earth tinted with the
  same fresh soil (65 %) instead of grey haze, speck streak arcs 0.012 -> 0.03 rad (no hairlines), oblique stretch
  1.35 (longer entry gouge). (10-12)
Rejected / partly:
- (1) bigger bomb boxes: third critic in a row asking for it; kept the specified 9/12/16/22 m (C++ side) and reported it
  as the main open recommendation. Measured instead: K0 a>0.9 footprint 6.1 m vs B7A 4.0 m (1.5x diameter, 2.3x area).
- (2) lobes to 2.2 R: impossible inside alpha 0.45 D with the crest at 0.25 D (1.75 R max) without shrinking the bowl
  below the B7 shell's.
- (8) baked directional shading: none is baked; the identical crescent is the preview's fixed sun at yaw 0.
- (10) core 0.25-0.30 D: B3-B5 cores grew in round 1; B6/B7 kept (their box grew 1.42-1.5x); clods were already sized
  physically (B7 9-56 cm, B6 7-38 cm).
- (11) separate soot layer: the material has one soot mask channel; soot reduced instead.

Final check: B0-B2 cells 0-5 byte-identical to `r3/` (both textures). Visible crater (alpha > 0.5, equivalent
diameter) old -> new: B3 79 -> 131 cm, B4 115 -> 182, B5 164 -> 248, B6 256 -> 364, B7 399 -> 587 cm.
In-editor: `Saved/ImpactDecals/bomb_final/ingame_bombs_vs_shells.png` (Ger_IntroMission desert landscape, RTS-like
camera, B6A / B7A / B7B on the right, K0 round / K1 lopsided / K2 double on the left; test actors removed, level not
saved).

## Summary of the three rounds (2026-11-09)
1. Round 1: bomb layout rebalanced (crest 0.25 D, lobed blanket to 0.445 D), real rim profile, warm subsoil palette,
   absolute clod sizes, variants pushed apart; shells re-sized (Rc +10 % on B3-B5), more and thinner rays, lighter clods.
2. Round 2: bomb depth reading (dark wet cone bowl, lit crest, strata, standing water in 2 variants), centre pile
   removed, continuous apron, angular power-law clods, stronger lopsided/broken/double variants, taller rims;
   shells got a ring of earth between bowl and rays, clustered rays, rounder oblique cells.
3. Round 3: bombs darker/warmer with no veil, debris floor without the specular "concrete", random block orientation,
   big clods, stronger broken/double variants; shells lighter warm core, less soot, brown ring, broader speck streaks.
