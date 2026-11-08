# Team colour shaders for vehicles

**In the game since 2026-11-03: CamoSwap on every German vehicle.** The player picks a team colour in the faction
selection menu; the German hull, turret and body materials repaint their dark camo bands in it. The default ("Camo")
changes nothing. The sections from "Assets" down describe the 2026-10-19 test that compared three shader types on copies
(`MasterMaterials/TeamColorTest/`, still there and unused by the game).

## In the game

### Pieces

| Piece | Where |
|---|---|
| `MF_TeamColor_CamoSwap` (production) | `/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/TeamColor/` |
| Splice into the German vehicle materials | `Source/Tools/TeamColor/rollout_team_color.py` (run in the editor through the bridge) |
| Which materials | `Source/Tools/TeamColor/team_color_targets.py` (6 hull masters, 32 vehicle materials) |
| Per-camo keys | `pick_team_color_keys.py` from the camo textures (`survey_` -> `inspect_` -> `export_` -> `pick_` scripts) |
| Colour storage | `URTSGameInstance::SetPlayerTeamColor` / `GetPlayerTeamColor`; saved in `USavePlayerProfile::M_PlayerTeamColor` |
| Writing it to vehicles | `FRTS_TeamColor` (`FactionSystem/TeamColor/`), called from `ATankMaster::BeginPlay_ApplyPlayerTeamColor` |
| Picker UI | `UW_FactionTeamColorPicker` / `UW_FactionTeamColorSwatch` (`FactionSelection/TeamColorPicker/`), built by `UW_FactionSelectionMenu::SetupTeamColorPicker` |

### Default look stays exact

The function's Custom node starts with `[branch] if (TeamWeight <= 0) return Base.rgb;`. Team colour weight is the alpha
of custom primitive data 7, uniform per draw, so the branch is coherent: without a team colour (every enemy and neutral
vehicle, and the player's vehicles while "Camo" is picked) the camo passes through bit for bit and the swap maths is
skipped. `FRTS_TeamColor` does not even write the primitive data while the colour's alpha is 0.

### How the colour reaches a vehicle

1. The faction menu loads the saved colour into the game instance before its first hangar preview spawns
   (`FRTS_Profile::LoadSavedPlayerTeamColor`, silent when there is no profile yet).
2. A swatch click stores it on the game instance, saves it in the profile (`FRTS_Profile::SavePlayerTeamColor`; only
   when a profile file exists, so no test profile is ever saved as the player's) and repaints every player vehicle in
   the hangar (`FRTS_TeamColor::ApplyPlayerTeamColorToWorldVehicles`).
3. In a mission every `ATankMaster` owned by player 1 writes the colour to custom primitive data 4-7 of all its
   primitives and of its child actors (turrets) in `BeginPlay`, right after the faction flag (index 0) is set.

Same reach as the faction flags: nomadic trucks and harvesters are tank masters, so they are covered; aircraft, team
weapons, buildings, wrecks and parts attached after `BeginPlay` are not.

### Which band is repainted

The German schemes share two dark bands over a dunkelgelb base coat: red-brown (about sRGB 0.13-0.19, 0.06-0.09, 0)
and dark green (about 0.05-0.07, 0.09-0.10, 0). The production function keys up to two bands (`TC_KeyColor`,
`TC_KeyColor2`, whose alpha switches the second band on) and every material has its own `TC_KeyColor`, `TC_KeyColor2`,
`TC_Strength`, `TC_KeyMin` and `TC_KeyMax` parameters (group TeamColor; instances override them). `TC_KeyMin` /
`TC_KeyMax` is the key radius in chromaticity: fully repainted within the first, untouched beyond the second (default
0.08 / 0.15). The keys were measured from each camo texture:
saturated pixels in the red-brown hue range (0-32 degrees) and the green range (58-160 degrees), median chromaticity,
kept when the band covers at least 2% of the paint and repaints at most 15% of the base coat.

- **Three-tone camo** (Panzer II/III/IV, 38(t), Hetzer, Panther D, Tiger H1 rail, Maus, Panther II, E-50, King Tiger,
  SdKfz 251, StuG): both dark bands become the team colour (6-47% of the paint), dunkelgelb stays. Strength 0.95.
- **Pale camo** (Brummbar, SdKfz 234/4 turret, SdKfz 9): the brown and green blotches are pale and only about
  0.06 from the dunkelgelb in chromaticity, inside the default key radius. When no band is found the picker retries
  with `TC_KeyMin` / `TC_KeyMax` 0.015 / 0.03 and less saturated band pixels, and keeps the keys when they cover 5-35% of
  the paint (more means the key caught a reddish base coat, or repaints more than a three-tone vehicle: the Jagdpanzer
  IV/70's 38% read clearly stronger). 8-30% of the paint is repainted and the base coat stays.
- **One-tone camo** (SdKfz 250 turret `MI_Sdkfz250`, Sturmtiger, Jagdpanzer IV/70, Panzer I, Panzer II turret, rocket truck):
  no band or blotch to repaint, so the whole paint gets a light tint, strength 0.2. 0.3 already read stronger than the
  three-tone vehicles on colourful camo (Jagdpanzer IV/70, Tiger E). Until 2026-11-03 these (and the pale camos) were
  repainted at 0.7 and changed far more than the rest.
- **One band by hand** (`TARGET_KEY_OVERRIDES` in the picker, 2026-11-04): Tiger E and Panther G repaint a single camo
  band, not a tint over all of it.
  - Tiger E: `MM_Ger_PostApoTank` shows `lerp(TankDiffuse * DiffuseTint, Camo, CamoAlphaStrenght)` and the team colour
    reads that blend. The raw overlay colours sit about 0.3 away from the blend in chromaticity, which is why keys
    measured on the overlay alone (`overlay_png`) missed. The keys are measured on a rebuilt blend instead: the desert
    overlay's red-brown on the hull and turret base (0.03 / 0.06), the forest overlay's brown on the turret
    (0.015 / 0.03). Its two turret materials share one diffuse but not the overlay, so the override goes by material.
  - Panther G green scheme: its olive-green blotches (hue 52-90, too pale for the generic green band) at
    0.015 / 0.03, about 12-16% of the paint, on hull, turret and engine deck alike.
  - SdKfz 231 (`M_sdkfz_231`, no instance of its own): its grey-brown stripes over the sand base at 0.015 / 0.03,
    about 17% of the paint. The stripes are barely saturated, so the shader's paint mask (`PaintMin` / `PaintMax`)
    passes only a fraction of them; `TC_Strength` 8 makes up for that (the swap clamps the weight to 1). An
    override's optional fourth value is its strength.
  - Puma (`M_Puma`, its hull and turret instances, `MI_Puma_RadixGlow_Red` / `_Yellow`) and SdKfz 234/3
    (`M_Sdkfz_234_3`, `MI_Hull_Sdkfz_234_3`): repainted like the SdKfz 250 on 2026-11-04. Their three-tone camo
    (olive, sand, red-brown) gave no band that read as a marking: the pale grey-sand key was speckled over the whole
    vehicle at strength 6 and invisible at 2.5, and the olive key (0.012 / 0.025, strength 1.2) took too much of it.
    `Tools/TeamColor/paint_three_tone_grey_yellow.py` turns the olive blotches (about 22% of the paint) into the dark
    yellow and the sand and red-brown into one Panzergrau base: a neutral grey at the pixel's brightness relative to
    its class paint, since a per-channel gain from the warm brown turned the darker panels blue. Light pixels need a
    greener hue to count as olive, because the 234/3 sand is greenish (hue 48-54). New textures
    `Puma/Materials/SdKfz_234_Puma_GreyYellow_D` and `Sdkfz_234_3/Materials/SdKfz234-3_GreyYellow_D`; the old ones
    are untouched. `M_Puma`'s `DiffuseColorMap` default, the two RadixGlow instances' override, `M_Sdkfz_234_3`'s
    plain texture sample and `MI_Hull_Sdkfz_234_3`'s `TankDiffuse` point at them. Keyed on the yellow at 0.04 / 0.08,
    strength 0.95: the picker's maths keys 82% (Puma; the rest is dark olive mottling kept as it was) and 94% (234/3)
    of the yellow and 0% of the grey and of neutral metal and markings. Checked in captures
    (`Saved/TeamColorTest/pumagreyyellow/`: `puma_234_3_before_after.png`, `puma_list.txt`) of the Puma, DualBarrel,
    RadixiteSniper and 234/3: Red and Blue repaint only the yellow blotches. The 234/3 renders much darker than the
    Puma, so its grey reads close to black; it already did before the repaint (its instance takes the master's
    shading defaults).
  - SdKfz 250 hull (`MI_Sdkfz_250_Hull_DesertAPO`: SdKfz 250, ammo carrier, 250/37 and variants;
    `MI_Sdkfz_250_Hull_Ambulance`): repainted 2026-11-04 so a team colour reads as a marking. Its old camo (sand
    blotches over blue-grey, both low chroma) could only take the one-tone tint, so red read as a brown wash and blue
    as grey. `Tools/TeamColor/paint_sdkfz250_grey_yellow.py` repaints the exported diffuse: the blue-grey stays as the
    Panzergrau base (max - min about 0.02, below the paint mask, so it is never keyed), the dark-brown interior and
    underside panels turn dark grey, and the artist's sand blotches become dark yellow (sRGB about 0.42, 0.36, 0.20,
    fleet dunkelgelb brightness but more saturated), shrunk from about half to a third of the camo panels. Wear,
    rust, shading, the crosses and the alpha carry over; sand that turns grey takes the grey next to it, so no ghost
    of the old pattern stays. The ambulance keeps every pixel that differs from the plain hull (red crosses, canvas
    tarp). New textures `HullTextures/T_Sdkfz_250_GreyYellow_D` and `T_Sdkfz250_Ambulance_GreyYellow_D`; the old ones
    are untouched, so pointing `TankDiffuse` back reverts it. Keyed on the yellow at 0.03 / 0.06, strength 0.95: the
    picker's maths keys 97% of the yellow and 0% of the grey, dark panels, tarp and crosses (the tarp sits about 0.08
    from the key, so the default 0.08 / 0.15 would repaint it). Checked in captures
    (`Saved/TeamColorTest/sdkfz250_greyyellow/`: `sdkfz250_before_after.png`, `sdkfz250_list.txt`): Red minus Default
    changes only the yellow blotches; the grey, the markings and the tarp stay at the lighting noise floor. Out of
    scope: the turret material `MI_Sdkfz250`
    (SdKfz 140, Mechanized Depot), tracks and wheels.
- **Off** (strength 0): Wespe (grey paint); E50 and E75 (custom camo the user keeps, via `TARGET_KEY_OVERRIDES`).
- Exceptions by hand in `pick_team_color_keys.py`: `TARGET_KEY_OVERRIDES` (above), `ONE_TONE_TEXTURES` (SdKfz 250
  turret; the Panther G entries are overridden) and `PALE_FIRST_TEXTURES` (Brummbar side plates) so one vehicle never mixes two looks; `NO_SWAP_TEXTURES` for
  textures that are not the camo.
- `keys_sheet.png` (written by the picker into `Saved/TeamColorTest/`) shows every camo, its swap mask and a red
  preview made with the shader's maths.

To tune one vehicle, change `TC_KeyColor` / `TC_KeyColor2` / `TC_Strength` / `TC_KeyMin` / `TC_KeyMax` on its
material instance (or the material's parameter defaults for vehicles without an instance).

### Splice points

- Hull masters with the `DiffuseTankHull` named reroute (`MM_Ger_PostApoTank_NoBlend` and its RailRed / RailOrange /
  FullUV copies): between the `TankDiffuse` sample and the reroute, before `MF_Apply4xSquareStamp`, so unit markings,
  flags and the shared atlas parts stay untinted.
- Every other material (`MM_Ger_PostApoTank`, `MM_TankHull`, the vehicle materials): right before Base Color.
  Markings there go through the function too; they are low-chroma (black, white) or far from the keys, so they keep
  their colour.

### Checks (2026-11-03)

- **Default unchanged:** 19 German vehicles pictured before and after the rollout (`fleet_baseline.txt`,
  `compare_fleet_default.py`). Before/after differences sit at the same-run lighting noise floor; the five largest
  (JagdPanther, Panther G, SdKfz 251, Brummbar, Tiger E) were checked by eye: only the map's light differs, the camo is
  identical.
- **Colours:** `fleet_team_colors.txt` + `make_fleet_sheet.py` (sheets in `Saved/TeamColorTest/fleet_sheets/`).
  Three-tone vehicles keep the dunkelgelb pattern with the dark bands in team colour (Panzer III, 38(t), Hetzer,
  Tiger H1, King Tiger, Panther D/II, JagdPanther, Maus, E-50, SdKfz 251, trucks). Weak: Panzer IV H (few band
  pixels show from above), Keubel (thin lines). In this first run the one-tone vehicles (Puma, Brummbar, Panther G,
  Tiger E, SdKfz 250) were repainted almost entirely; the pale-camo keys and the 0.3 tint above replaced that
  (`brummbar_check.txt`, `one_tone_check.txt`).
- **Menu:** `run_faction_menu_team_colors.sh` (`-FactionMenuCaptureTeamColors`) clicks red, blue, yellow and Camo; the
  hangar preview's primitive data follows (`0.80,0.02,0.02,1` ... `0,0,0,0`). It backs up and restores
  `Saved/SaveGames`, because every click saves the profile.
- **Cost:** pixel shader instructions (static count, `get_statistics`): `MM_Ger_PostApoTank_NoBlend` 423 -> 500
  (+77; the production function keys two bands), no extra samplers, vertex shader unchanged. At runtime the branch
  skips the maths on every vehicle without a team colour (all enemies and neutrals).

### Not covered

Wheels, tracks, glass, lenses, antennas, nets, MGs, tools, props atlases, crew and the shared environment masters
(`MM_RTS_Env`) are left alone on purpose. The grey Panzer III M Schreck turret (`M_PanzerIIIM_Schreck_DarkGrey`) has no
camo and was left out.

## Assets

| Asset | What it is |
|---|---|
| `MF_TeamColor_CamoSwap` | repaints one camo band (picked with `TC_KeyColor`, default red-brown) in the team colour |
| `MF_TeamColor_PaintTint` | glazes all painted plates toward the team colour; bare metal untouched |
| `MF_TeamColor_CamoTint` | both in one: a light tint over all paint (`TC_TintStrength`, 0.2) plus the keyed band repainted on top (`TC_SwapStrength`, 0.95); either strength at 0 gives the pure version |
| `MF_TeamColor_TopologyAccent` | paints the steep/side plates (turret walls, superstructure sides); flat decks and the turret roof keep their camo |
| `MM_Ger_PostApoTank_TC_<Type>` | duplicate of the tank master with the function spliced in |
| `MI_PZIII_Hull_DesertAPO_TC_<Type>` | copy of the Panzer III hull instance on that test master |
| `M_TigerII_Hull_TC_<Type>` | copy of the King Tiger hull/turret material (a standalone material, not on the master) with the function spliced in |
| `MM_Ger_PostApoTank_TC_CamoTint`, `MI_PZIII_Hull_DesertAPO_TC_CamoTint`, `M_TigerII_Hull_TC_CamoTint`, `MI_TigerII_Hull_TC_CamoTint_GreenKey` | the combined function on both test tanks |
| `MI_TigerII_Hull_TC_CamoSwap_GreenKey` | King Tiger CamoSwap with `TC_KeyColor` set to the camo's dark green |

## Where the function sits in the master

`TankDiffuse` (hull camo, UV x2) -> **MF_TeamColor_*** -> named reroute `DiffuseTankHull` -> `MF_Apply4xSquareStamp`
(icons / crosses) -> lerp with the shared tank atlas (`TankUVs`) -> BaseColor.

Inserting before the stamp keeps unit markings crisp, and the atlas parts (fuel cans, antenna, tools) never get the
team colour. Integrating into the real master later is: drop the function in this same spot, nothing else.
`M_TigerII_Hull` has the same structure (`Hull_low_BaseColor` -> `MF_Apply4xSquareStamp.InDiffuse`); vehicles with their
own materials like it need the function added per material.

## How the colour is applied (all three types)

- Blending happens in gamma space (`pow(x, 1/2.2)`): the camo is dark (median ~0.1-0.2 sRGB), linear maths only darkens.
- The camo's value is **overlaid** with the team colour: wear, grime, panel shading and the stamp all survive, only the
  hue/chroma changes. `TC_Lift` pulls the value toward mid grey so dark camo still reads as the colour.
- `PaintMask` = chroma of the camo (`max - min` in sRGB, smoothstep `TC_PaintMin..TC_PaintMax`): painted plates are
  coloured, bare steel, grilles, tools and soot are grey and never get the team colour. Checked on the Panzer III
  texture: grilles and tool clips fall out cleanly.
- CamoSwap keys one band by **chromaticity** (colour with brightness divided out, `rgb / (r + g + b)` in sRGB): pixels
  whose chromaticity lies within `TC_KeyMin..TC_KeyMax` (0.08..0.15) of `TC_KeyColor` are repainted. Wear and shading
  inside the band stay keyed because brightness is ignored. `TC_KeyColor` is set per camo in a material instance by
  eyedropping the band to swap: default red-brown (sRGB 0.19, 0.085, 0.002); dark green for the King Tiger's ambush camo
  (sRGB 0.07, 0.10, 0.005). The first version keyed only red-brown with `(R - G) / (R + G + B)`; the chromaticity key
  gives the same Panzer III mask (8.4% vs 9.0% of the texture) and works for any band colour.
- TopologyAccent uses the per-pixel world normal: `1 - smoothstep(TC_UpMin, TC_UpMax, PixelNormalWS.z)` colours the
  steep plates, and the red-brown band is left out so the pattern reads through. Normal-map rivets, welds and bevels
  break the colour like chipped paint. It reads geometry, not UVs, so it needs no per-vehicle setup.
  First tried the other way round (colour the up-facing plates): from the RTS camera (about -60 degrees) nearly every
  visible pixel faces up, so it recoloured the whole tank and looked like a stronger PaintTint. Painting the sides
  instead outlines the hull in team colour while the large camo areas on top stay whole.

## How each type behaves on other hull shapes

- **Boxy hulls with vertical sides (Panzer III/IV, StuG):** TopologyAccent gives a clean coloured band around the
  silhouette; CamoSwap and PaintTint behave the same everywhere.
- **Sloped armour (T-34, Panther glacis at roughly 55-60 degrees, N.z about 0.5):** inside the `TC_UpMin..TC_UpMax`
  falloff, so these plates get a partial coat; the more slope, the more camo survives. Tune the window per faction
  master if a sloped tank looks under-coloured.
- **Cast/rounded turrets (T-34/85, IS):** the falloff gives a soft gradient from coloured walls to a camo roof rather
  than a hard edge.
- **Other camos:** CamoSwap needs `TC_KeyColor` set to a band that the camo has and that is visible from above. On the
  King Tiger the default red-brown key only finds a few patches (the visible plates are dunkelgelb and dark green), the
  dark-green key repaints the green stripes and reads clearly. Single-colour camo (Panzergrau, Soviet green) has no
  second band to key; there PaintTint (or CamoSwap keyed on the base colour) is the fallback. PaintTint and
  TopologyAccent need no per-camo setup because they key on paint chroma and geometry.
- **Team colours close to the camo palette (yellow/sand on desert camo):** CamoSwap reads weakly; that is a limit of
  swapping only one band.

## Feeding the team colour per player

`TeamColor` is a vector parameter bound to **custom primitive data 4-7** (RGBA, linear; A = weight, 0 = off). Index 0
is already used by the faction flag (`FRTS_FactionFlags`). Per-primitive data keeps one shared material instance for
every tank (no dynamic instances, batching intact) and different teams on screen at once; a Material Parameter
Collection would only allow one colour per scene. Game code would set it like `FRTS_FactionFlags::SetFlagOnActorAndAllChildren`
does, on every primitive of the vehicle and its turret child actors:
`Primitive->SetCustomPrimitiveDataVector4(4, FVector4(TeamColor.R, TeamColor.G, TeamColor.B, 1.f))`.
Without primitive data the material instance's `TeamColor` value is used (default alpha 0, so tanks look unchanged).

## Testing

`Source/Tools/TeamColor/run_team_color_capture.sh` spawns the Panzer III M offscreen (unit portrait capture,
`Docs/UI/UnitPortraits.md`) once per list entry in `pz3m_team_colors.txt`, swapping the shared hull instance for a
test instance and setting the team colour. New capture-tool switches/fields:

- list entry fields `|Swap=<material path>><material path>|TeamColor=r,g,b[,a]`
- `-UnitPortraitShots=0` takes only the idle picture and spawns no AI controller (the tank otherwise engaged map
  enemies mid-picture: smoke, slewed turret)
- `-UnitPortraitRtsView` adds a steep `RtsCamera` view like the in-game camera
- `-UnitPortraitFixedExposure` uses manual exposure (bias from `EXPOSURE_BIAS`, 2.0 for these runs) so cells are
  exposed alike. The map's light still changes over the ~2 minutes of game time a run takes (the road gets darker
  in some cells), so compare colours within a row more than across rows.

`python make_team_color_sheet.py <captures> <out> [--unit KingTiger] [--variants A,B]` lays the pictures out as shader x
colour sheets per view. `kt_team_colors.txt` does the same for the King Tiger; it starts with `Warmup_` entries because a
test material shown for the first time still compiles its shaders and pictures as the grey default material.
Material edits made while no editor bridge is free (editor closed or in PIE) can run headless:
`UnrealEditor-Cmd <uproject> -run=pythonscript -script=<file> -unattended -nullrhi -DisablePlugins=Fab`.

## Results on the Panzer III M (desert camo, RTS camera)

- **CamoSwap** keeps the most of the artwork while reading clearly as a team: the sand and dark outlines stay, the red-brown
  band becomes the team colour. Yellow barely reads against sand (a limit of swapping one band).
- **PaintTint** gives the strongest team identity; the pattern survives as light/dark, but the whole tank takes the hue.
- **TopologyAccent** keeps almost all camo but gives the weakest identity on this hull: the Pz III's steep plates are
  small and lie in shadow at this sun angle, so from the RTS camera it reads close to the original. It would do more
  on tall-sided hulls, or combined with a low-strength PaintTint glaze.

## Results on the King Tiger (ambush camo)

- **CamoSwap, dark-green key** is the best fit here: the green stripes (also on the gun barrel) become the team
  colour, dunkelgelb and red-brown stay, and the tank still reads as ambush camo. Yellow is weak against dunkelgelb.
- **CamoSwap, default red-brown key** shows the team colour only on a few brown patches: it must be keyed per camo.
- **PaintTint** works without setup but changes the most; on the bright dunkelgelb it turns pastel (blue reads as
  silver-white), so dark team colours lose saturation on bright camo.
- **TopologyAccent** colours the steep turret and hull sides; better than on the Panzer III because the King Tiger's
  turret walls are larger, but still mostly camo from the RTS camera.

## Combined CamoTint and performance

CamoSwap and PaintTint share the same core, so `MF_TeamColor_CamoTint` runs both in one Custom node: tint all paint
lightly, then repaint the keyed band on top (each stage with its own strength and lift). On the Panzer III the tint
makes the sand plates lean toward the team hue, which helps the weakest case (yellow team on sand); on the King Tiger's
bright dunkelgelb a 0.2 tint is barely visible, so raise `TC_TintStrength` per instance there.

Pixel shader instructions from `MaterialEditingLibrary.get_statistics` (SM6, 2026-10-19):

| | Panzer III master | King Tiger material |
|---|---|---|
| Original | 423 | 395 |
| + PaintTint | 461 (+38) | 433 (+38) |
| + TopologyAccent | 473 (+50) | 445 (+50) |
| + CamoSwap | 481 (+58) | 453 (+58) |
| + CamoTint | 496 (+73, +17%) | 468 (+73, +18%) |

No extra samplers or texture samples (the function reuses the camo sample), vertex shader unchanged (554), no new
shader permutations. The cost is base-pass pixel ALU on vehicle pixels only; not profiled in a running mission yet.
Cheaper later if needed: replace the gamma `pow`s with `sqrt`/square (gamma 2.0). Adding the function to the master
means a one-time recompile of every instance on it; vehicles with their own materials need it added per material.

