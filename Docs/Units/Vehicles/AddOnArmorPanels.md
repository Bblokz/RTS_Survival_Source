# Add-on armour panels (side skirts)

Side skirts and other bolted-on plates are add-on armour zones of the vehicle module system
(`VehicleModuleSystemDesign.md`). A zone adds armour on top of the structural plates it covers. Damage turns it
yellow (half the add-on armour, `UVehicleModuleAddOnArmorBehaviour`) and then red (none). The panels you see follow
the zone: damage hides panels, and repairs bring them back.

The technique is the "baked + mask" winner of `AddOnArmorBreakTest.md`. The panels stay part of the hull mesh, in
their own material slot. A masked material clips the hidden ones. One custom-primitive-data float holds a bit per
panel. A hit costs one float write, and the panels add no draw calls or components.

## Pieces

| Piece | Where |
|---|---|
| `UAddOnArmorPanelsComponent` | `RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/`. Zone settings: hull side, covered plate types, add-on armour value. Defaults: left and right skirts over `Plate_Side*` and `Plate_SideLower*`, 15 each. The panel mesh is the owner's static mesh with a panel layout. Its zones cover the plates of that mesh, or of the nearest registered mesh it is attached to, for a bolted-on panel mesh. |
| `UAddOnArmorPanelLayout` | Asset user data on the hull mesh: panel centre and side per panel. Panel index = mask bit = value in UV channel 3. |
| `RTS.AddOnArmor.BuildPanelHull` | Editor command (`AddOnArmorPanelAuthoring.cpp`). Copies a hull mesh to `<hull>_AddOnPanels`, moves the panels into hideable slots (one per source slot: `AddOnArmorPanels`, `AddOnArmorPanels_1`, ...), writes the panel UVs and the layout, and makes the panel materials. LODs with their own description are tagged too. Every prepared vehicle's parameters are in `Tools/AddOnArmorBreak/build_panel_hulls.py`, which rebuilds them all. |
| Panel materials | `<slot material>_AddOnPanels` next to the new hull. An instance is copied and reparented to `<master>_AddOnArmorPanels`, the master duplicated as Masked (made once per master). A plain material becomes a masked copy itself. The opacity mask reads the panel index (UV 3) and the mask (custom primitive data 8, `AddOnArmorHiddenPanels`). |
| Armour-off sound | `UAddOnArmorPanelAudioSubsystem` (world subsystem, same folder as the component). One clang per hit that hides panels, at the hit, from a pool of 16 `UAudioComponent`s (`DeveloperSettings::GamePlay::Audio::AddOnArmorOffAudioPoolSize`) on one `ARTSPooledAudio` actor; when all 16 play, the one that started first is reused. Cues: `Cue_AddOnArmorOff_Light/Medium/Heavy` in `/Game/RTS_Survival/SpecialEffects/AddOnArmor/Sounds` (always cooked, loaded by soft path). Weight from the tank subtype: heavy tank list -> Heavy, light tank or armoured car -> Light, anything else -> Medium. The cues carry their own attenuation and sound class. `-LogCmds="LogAddOnArmorAudio Verbose"` logs each sound and its pool entry. |
| Debris | `NS_AddOnArmorPanelDebris` in `/Game/RTS_Survival/SpecialEffects/AddOnArmor`, built by `RTS.AddOnArmor.BuildDebrisSystem` (`AddOnArmorPanelDebrisAuthoring.cpp`, HLSL in `DebrisHlsl/`). A pooled CPU mesh burst: one particle per hidden panel, mesh from `User.DebrisMesh`. The mesh is the hull's debris panel, a `UAddOnArmorPanelLayout` field set by `RTS.AddOnArmor.SetPanelDebrisMesh`, `BuildPanelHull DebrisMesh=` or `RTS.AddOnArmor.ExtractPanelDebris`. It is cut out of the built hull, not modelled; the rules are in `AddOnArmorPanels/AGENTS.md`. |
| Diagnostics | `AddOnArmorPanelDiagnostics.cpp`: `RTS.AddOnArmor.SurveyPanels` (thin outer side plates of every hull, the candidate list), `DumpMeshTriangles` (for side-view plots), `DumpMaterialHlsl`. |
| `UArmorCalculation::SetAddOnArmorOfPlateType`, `GetOnAddOnArmorZoneHit` | Sets the add-on armour per plate type. The hit event carries each damaged zone's impact position, published before the transitions of that impact. |

Custom primitive data: 0 is the faction flag, 4 to 7 the team colour, 8 the hidden panels. UV channel 2 is taken by
generated lightmap UVs on vehicle hulls, so panels use channel 3. At most 20 panels per mesh (exact integers in a float).

## What the player sees

- Hidden panels follow the zone's lost health: `ceil(lost * panels)`. A yellow zone hides at least one panel and
  keeps at least one. A red zone hides all of its panels.
- The panel hidden next is the intact one closest to the last hit on that zone. Splash damage uses the point of
  the hull closest to the explosion.
- Repairs bring back the panels hidden most recently first. A full service brings back all of them. A loaded save
  is synchronised from the module state, because loading emits no transitions.
- Hits under 20 mm calibre do not damage add-on armour, so they hide nothing.
- Every panel that damage hides flies off as debris. It leaves the hull face with the tank's own velocity, tumbles,
  bounces on the ground under the tank, lies flat and shrinks away 3.6 s after the hit. A loaded save throws nothing,
  and neither does a tank that fog of war hides.
- When the tank dies, whatever kills it (any death type, regular, cook-off or engine-fire death), every panel still on
  the hull blasts off: one burst per side, 1.6x the hit speed, pushed along the killing shot, with one clang
  (`UAddOnArmorPanelsComponent::BlowOffIntactPanelsOnDeath`, called from `ATankMaster::UnitDies` before
  `BP_OnUnitDies`). The dying tank keeps them hidden; late module transitions no longer bring panels back. A tank that
  fog of war hides only hides them. The wrecks are separate meshes, and some still show their plates.
- Each hit that hides panels plays one armour-off clang for the tank's weight class (light, medium, heavy) at the
  hit, however many panels come off. It also plays on a hull without a debris panel. Loads, repairs and tanks hidden by
  fog of war are silent.

## Converting a vehicle

1. Find the panels. Panel geometry = mesh islands lying entirely at |Y| >= `MinAbsY`, within `MinZ`..`MaxZ`, and no
   longer than `MaxIslandLength` along X. Longer islands are rails and stay part of the hull.
   - Plates (`PanelsPerSide=0`): islands of at least `MinPanelArea` cm² are panels. Two panels that overlap along X
     by more than half are the two faces of one plate and merge. Smaller islands (bolts, clamps, stiffeners) join
     the panel of their side whose X range holds them.
   - Wire screens and cages (`PanelsPerSide=N`): each side's panel geometry splits into N equal sections along X.
   - Skirts modelled as one piece (`SplitX=<x>/<x>/...`): islands in the panel volume are cut apart at those
     mesh-space X positions first (each polygon goes to the side of its centre), then classified as plates. Put the
     cuts on the seams, where the mesh already has vertex columns. Vertex instances shared across a cut are copied, so
     each plate carries its own panel index. The cut ends stay open (no cap); on the T-34E that is a 2 cm sliver.
2. Add the hull to `Tools/AddOnArmorBreak/build_panel_hulls.py` and run it (`ONLY=<hull name>`, `DRYRUN=1` to
   classify only). Check the `panel=` lines in the log: one per panel, with the side and bounds you expect. Blueprints
   whose hull component overrides a material need `MaterialOverrides=<slot>:<material>`.
   Then cut the debris panel out of the built hull with `RTS.AddOnArmor.ExtractPanelDebris Hull=<new hull>`
   (`AddOnArmorPanels/AGENTS.md`). Don't model one: the cut-out is a separate mesh asset that only Niagara draws, and
   it looks like the baked panel.
3. In the tank Blueprint:
   - set the hull component's mesh to the new hull;
   - add `AddOnArmorPanels` (`UAddOnArmorPanelsComponent`) and adjust its zones if the defaults do not fit;
   - make the structural plate values exclude the add-on armour.

   `RTS.AddOnArmor.ConvertBlueprint Blueprint=<asset> Hull=<new hull> Armor=Plate_SideLeft:30|...` does all three
   headless: it sets the value pin of each literal `SetArmorOfPlateType` node of the listed plates. `-DryRun` lists
   those nodes and the hull component. The editor that runs it needs the new DLL, and no older editor may have the
   Blueprint loaded. An editor without `UAddOnArmorPanelsComponent` drops the component when it saves the Blueprint.

   The tank installs the zones right before module finalization, a tick after BeginPlay. The hull armour must be
   registered by then.
4. Validate with `Tools/AddOnArmorBreak/run_panels_validate.sh` (`VEHICLE=<class>`). It shoots the left panels
   through the weapons' module damage event and checks the armour, module state and mask after each step. It also
   writes pictures to `Saved/AddOnArmorBreak/<tag>/`. Before the Blueprint has the component,
   `INJECT=<new hull>` adds the component and the hull to the spawned tank.

## Gotchas

- The panel master must not keep the hull master's opacity-mask constant. A copied opaque master can have
  `OpacityMask.UseConstant` set, and a set constant wins over a connected node. The material then compiles
  `OpacityMask = 1` and never hides anything, with no error. The authoring clears it.
- `bCanMaskedBeAssumedOpaque` is only re-derived in `PostEditChange`. A copy of an opaque master keeps `true` until
  then, and the renderer treats it as opaque. The authoring checks this.
- `RTS.AddOnArmor.DumpMaterialHlsl <material> <file>` writes the translated HLSL. Run it with `-RenderOffscreen`,
  not `-nullrhi`. Check that `PixelMaterialInputs.OpacityMask` is not a constant.
- Blueprints may clear and register the hull armour again after their module setup. That drops add-on coverage, so
  the zones install a tick after BeginPlay, right before module finalization (like turret modules).
- Debris HLSL: no `asin` in the update. With it, resting pieces froze tilted, half under the ground
  (`AddOnArmorPanels/AGENTS.md`).
- Rebuilding a `_AddOnPanels` hull in place (`BuildPanelHull` onto an existing target) replaces the mesh object in the
  open editor and nulls every loaded Blueprint's reference to it. Blueprints saved from that editor afterwards store
  `Hull = None` (2026-11-07, all six Pz III Blueprints; the hull then has no panel layout and the validation fails "a
  hull mesh with a panel layout"). After a rebuild, run `ConvertBlueprint Hull=` again on every Blueprint that uses
  the hull before saving any of them, and spawn-check the `Hull` mesh.
- Penetrating side hits also roll the ammo module. The validation turns off fires and cook-offs on its own tank.

## Panzer II F (spaced armour)

`Ger_Vehicle_PanzerII_F_SpacedArmor`: 5 panels per side, 74 x 72 cm, at |Y| 115 to 116.8, each with its bolts,
clamps and bottom stiffener. The hull is `SM_PZIIF_SpacedArmor_Hull_AddOnPanels`, with
`MI_Hull_PZII_DesertCamo_AddOnPanels` on the panels. Structural sides are 30 (upper) and 20 (lower), plus 15 add-on
armour each. The intact totals stay at 45 and 35, as before the conversion.

Converted 2026-11-05 with `RTS.AddOnArmor.ConvertBlueprint`. The run of the converted Blueprint, with no
injection, passed all 85 checks (`TAG=PzIIF_Converted`). Lower side: 20 + 15 = 35 intact, 27.5 yellow, 20 red.

Earlier validation (component and hull injected, before the conversion, so its sides were still 45 and 35):

| Step | Left zone | Hidden left panels | Lower-side armour |
|---|---|---|---|
| Intact | healthy | 0 | 35 + 15 = 50 |
| First 50 mm AP hit on the middle panel | yellow | that panel | 35 + 7.5 = 42.5 |
| 16 hits | red | 5 | 35 |
| One crew repair stage | yellow | 4 | 42.5 |
| Full service | healthy | 0 | 50 |

The right side stayed untouched throughout.

Finding, not changed: shells at the centres of the two rear panels resolve to `Plate_Rear` (15 armour), and at the
front panel to `Plate_FrontUpperGlacis`. Only the middle two panels sit over the side plates. The rear and front
hull boxes reach over the sides, so a side shell there meets 15 armour and can damage the engine. Such hits do not
reach the add-on zone.

## Panzer II F base and Radixite (Strike Division fences)

`Ger_Vehicle_PanzerII_F` (the base, trained by the Strike Division) and its children `_RadixiteAutocannon` and `_Strike`
use `StrikeFences/SM_PZIIF_Fenced_Hull_AddOnPanels`. The two partial post-apo side plates and their brackets are gone.
In their place, 5 wire-mesh grille fences per side hang from the fender lip over the tracks, in the Flak 41 Pz III fence
style (rusty posts, top tube, bottom rail, diamond wire mesh): 73 x 74 cm each, x -212 to 166, z 43.5 to 117.5, at
|Y| 115.4 to 118.6. Each fence is self-contained, so `PanelsPerSide=5` gives one hideable panel per fence. The hanger
brackets reach in under the fender and stay hull. The Blender build is
`Variants/PanzerIIF_StrikeFences/scripts/build_fences.py` (BlenderTanks).

- Zones: the defaults with **8** add-on armour each (`M_ZoneSettings` on the base's `AddOnArmorPanels`). Structural
  sides stay 30 / 20, so intact sides are 38 / 28.
- The hull has **one LOD** (6.6k tris). The source's auto LOD 2 (5 %) dropped every fence wire, so the fences would
  vanish at RTS zoom.
- Wires are 1.1 cm on the rust tile. 0.8 cm dark-steel wires, as on the Flak 41, did not show against the track shadow.
- `Ger_Vehicle_PanzerII_F_SpacedArmor` now inherits the base's component. Its own component was removed, and its
  inherited zones are overridden back to 15. Keep one panels component per class: the tank installs the first one
  `FindComponentByClass` returns.
- Radixite and Strike hold their own `Hull` mesh override, so each needed its own `ConvertBlueprint Hull=`.
- `_Strike` overrides only slot 0 (`MI_Hull_PZII_Strike`). Its fences use the desert camo panel material.
- Validation (`UT_B4Howitzer`, 2026-11-07): base `TAG=PzIIF_Fences` passed all checks, including the debris, on the final hull. Radixite and SpacedArmor passed on the first build (0.8 cm wires, same layout); Strike was not run. As on the
  spaced-armour hull, the two rear fences resolve to `Plate_Rear` and the front fence to `Plate_FrontUpperGlacis`.
- Wreck and training previews were overwritten in place on 2026-11-07, at the same asset paths:
  - `SM_Destroyed_PZII_Hull` lost its plates and got charred fences along its sagging fenders, some lost, one bent
    out, two on the ground. It has one LOD, and its sockets `Exp1` and `T_0`..`T_2` are unchanged (verified). It is
    shared: `Env_Dest_PZII` (base, Luchs placeholder), `Env_Dest_PZIIF_RadixiteAutocannon`, `BP_Env_DestroyedPzII`.
  - `SM_PanzerII_TrainingPreview` and `SM_PZIIF_RadixiteAutocannon_TrainingPreview` come from the optimised pipeline
    on the plate-free hull, with box fences added afterwards (about 2.4k tris).
  - Scripts: `build_wreck.py`, `build_training_preview.py`; UE `Saved/AgentScripts/PzIIFFences/fz_overwrite.py`.

## Panzer III (Strike Division fences)

Built 2026-11-07. The post-apo slat plates (two on the right, one on the left) and their hanger brackets are gone from
the shared Pz III hull, the FlaK 41 hull and the commander hull. In their place, 5 wire-mesh fences per side hang
from the track-guard rim over the tracks, in the FlaK 41 deck-fence style: 91.2 cm long, 4 cm gaps, x -236 to 236,
from the rim down to z 50, wire plane at |Y| 154 (the track's outer face reaches 150.4). Every fence part lies at
|Y| >= 151.2, so `PanelsPerSide=5` gives one hideable panel per fence. The guard clamps under the hanger straps stay
hull. Blender build: `Variants/PanzerIII_SDFence/scripts/build_sdfence.py` (BlenderTanks, see its `AGENTS.md`).

| Blueprints | Panel hull (`_AddOnPanels`) | Folder |
|---|---|---|
| `Ger_Vehicle_PanzerIII_J`, `_AA`, `_AA_Drilling`, `_J_RocketLauncher` | `SM_PanzerIII_SDFence_Hull` | `PanzerIII/Meshes` |
| `Ger_Vehicle_PanzerIII_AA_Flak41` | `SM_PzIIIAA_Flak41_SDFence_Hull` | `PanzerIII/Flak41` |
| `Ger_Vehicle_PanzerIII_J_Commander` | `SM_PZIII_Commander_SDFence_Hull` | `PanzerIII/CommandMeshes` |

- The fenced source meshes are duplicates of the old hulls with the Blender FBX reimported, so sockets, complex
  collision and the single LOD carry over. The old hulls are no longer used by any Blueprint.
- Commander: the camo-net flaps that hung over the sides are cut at the track-guard edge; the deck part stays.
- Zones: the defaults with **8** add-on armour each, on the components of J, AA and the commander. The FlaK 41,
  Drilling and Rocket Launcher inherit them. Structural values are unchanged, so the side plates read 30 + 8 = 38.
- Drilling and Rocket Launcher each held their own `Hull` mesh override (of the old mesh), so each needed its own
  `ConvertBlueprint Hull=`. The FlaK 41 too.
- `Ger_Vehicle_PanzerIII_J_RadixiteCannon` keeps its own hull (`SM_PanzerIIIJ_Radixite_Hull`, old plates) and lists
  `AddOnArmorPanels` in `M_RemovedInheritedComponents`, so the component it inherits from the J is destroyed at
  BeginPlay instead of reporting a missing panel layout.
- Wires are 1.1 cm on the rust tile. The first build's 0.8 cm dark-steel wires did not show against the track shadow.
- **Armour boxes, not changed:** on the J, shells into the middle and the two front fences resolve `Plate_Front`
  (50), not the side plate; only the two rear fences sit over `Plate_SideLeft`/`Right`. Such hits do not reach the
  add-on zone, so they hide no fence (the zone still empties rear-first and then forward from hits on the rear
  fences). No `Plate_SideLower*` is registered. The validation therefore reports the failures that need the middle
  fence over a covered plate.
- Validation (`UT_B4Howitzer`, 2026-11-07, tags `PzIIIJ_SDFence2`, `PzIIIFlak41_SDFence`, `PzIIICommander_SDFence`):
  rear fences read `Plate_SideLeft` 30 + 8 = 38; damage, red zone, crew repair, full service, right-side isolation and
  the debris piece passed; `RESULT FAIL` with 6 failures, all from the `Plate_Front` boxes above. The commander has a 7th:
  it repairs itself during the run (zone health 0.02 -> 0.07 between two hits), so one fence came back between hits.
- Not changed: the wrecks and the training previews still show the old plates.

## Panther II (Strike Division: Schürzen panels, side fences, one-atlas material)

Built 2026-11-07. `Ger_Vehicle_PantherII` now uses `PantherII/StrikeDivision/SM_PantherII_Strike_Hull_AddOnPanels`.
The Blender build is `Variants/PantherII_StrikeFences` (BlenderTanks); its `AGENTS.md` has the details. The Unreal scripts
are in `Saved/AgentScripts/PantherIIStrike/`.

- **Material:** the old hull had three full materials (Sheets / Hull / Parts), each with 4-5 maps at 4K, so 3 draws and
  about 15 textures. It now has one slot, `MI_PantherII_Hull_SD`, a copy of `MI_PantherD_Hull_DesertApo` on
  `MM_Ger_PostApoTank_NoBlend`. Its maps are `T_PantherII_Hull_SD_D/N/S` at 2048 (the baked old camo, AO and dust, the old
  normals, and a grey specular). The panel slot uses its `_AddOnPanels` copy.
  - The UVs: all old islands are packed at one density into the top-left atlas quadrant.
  - The fences have their own dark rust patch in that texture. The shared rust tile read silvery white through this master.
  - The faction stamps: two side stamps on skirt plate 2 (x -56) and two on the glacis. Their spots and rotations come
    from `hull_uv_info.json`. `RotationIconN` is in turns with the same sign as the island's up angle (+90 -> 0.25).
- **Panels:** the skirt was one island per side. It was cut into its 4 plates per side, which gives 8 panels: left 0-3 and
  right 4-7, rear to front. The build arguments are `MinAbsY=125 MinZ=0 MaxZ=81 MinPanelArea=5000 MaxIslandLength=112`.
  - The hangers stay hull.
  - The debris plate is panel 2, because panel 1 carries the stamp.
- **Fences:** 4 wire-mesh fences per side stand off the angled sponson plate (18.8 cm, over the stowed tools). They are hull
  geometry above `MaxZ`, so they can't be shot off.
- **Mesh:** one LOD, 12.8k tris. Bounds, sockets and complex collision (`SM_Collision_Hull_PantherII`) are the old
  hull's. The hull component keeps its (1.30, 1.25, 1.25) scale: the armour boxes go through the component transform.
- **Armour:** structural sides 50 (was 65) + 15 add-on, so the intact total is unchanged.
  - `ConvertBlueprint` now also sets literal `Make ArmorSettings` nodes (`Armor=Plate_SideLeft:50|Plate_SideRight:50`).
  - `-ClearHullMaterialOverrides` cleared the old `[None, MI_PantherII_Hull, None]` overrides. Slot 1 would otherwise
    have covered the panel slot.
- **Skirt boxes:** the scaled front and rear boxes swallowed every skirt hit. Panels 0-2 resolved
  `Plate_RearUpperGlacis` and panel 3 `Plate_FrontUpperGlacis`, so the zone never took damage. The new editor command
  `RTS.AddOnArmor.AddArmorBoxes Blueprint=<bp> Boxes=Plate:Value:MinX/MinY/MinZ/MaxX/MaxY/MaxZ|...`
  (`AddOnArmorPanelArmorBoxes.cpp`) appended two unit-scale literal boxes to `SetupArmor`'s armour array:
  `Plate_SideLeft` and `Plate_SideRight`, value 50, X -215..210, |Y| 118..145, Z 0..82 in hull-mesh space. The collision
  sides sit at |Y| 128-134. The command uses `/` between coordinates because `-ExecCmds` splits on commas, and it skips
  boxes that already exist.
- **Checks** (`UT_B4Howitzer`):
  - `run_panels_validate.sh` (`TAG=PantherII_SD2`, `PantherII_SD3`, `PantherII_SD_Final2` on the final build): RESULT
    PASS, 0 failures, death blast 7 pieces in
    2 bursts.
  - `run_armor_audit.sh` (`LIST=pantherii_sd.txt`): 0 wrong. Side probes resolve the side plates at 65, front and rear the
    glacis plates.
- **Turret (2026-11-08):** `Ger_Turret_PantherII` now uses `StrikeDivision/SK_PantherII_Turret_SD` with the material
  override cleared. Its one slot is `MI_PantherII_Turret_SD`, a copy of the hull MI with the stamps off (`EnableIcon1-4`
  0; the Panther D has no turret stamps either) and `T_PantherII_Turret_SD_D/N/S` at 1024. The KADO turret already had one
  UV set on one 4K sheet, so it was not repacked: `build_turret.py` moves every UV into the tank quadrant
  (u, v -> 0.5u, 0.5 + 0.5v), and `composite_turret.py` resizes the old maps (D = camo x AO, N = old normals, S levelled
  like the hull). At 1024 the turret has 2.5 px/cm, still above the hull's 1.77. Geometry, bones, sockets, the physics
  asset and the skeleton are unchanged (`compare_sk` max bone diff 0.000001). The FBX is exported without its root empty,
  because Interchange imports that Null as an extra root bone. Scripts: `p2t_import.py`, `p2t_bp.py` (backup
  `Ger_Turret_PantherII.backup.uasset`).
  Validation on `UT_B4Howitzer` (2026-11-08, Development DLL built that day): every step through the full service
  passes, but the run loses the tank at the death step. A projectile moving almost straight up
  (`RTS_WRD ... kill_shot=(0, -0.1, 1)`) kills it a few frames after the test's last shell, before the scripted death.
  With the original turret Blueprint restored, the same thing happened (1 pass, 1 identical loss), so this is the map or
  build, not the turret. Not investigated further.
- **Lehf turret + AutoPanther 50 mm gun (2026-11-08):** `SM_PanthII_LehfTurret` (the `Ger_Turret_PantherII_Lehf` body)
  and `SK_PantherDAuto50mm` were the other users of `M_PantherII_Turret`. They UV-map the same 4K tower sheet, so they now
  use `MI_PantherII_Turret_SD` too. Both were reimported in place, with no Blueprint change, from UV-patched FBX files
  (`fbx_uvpatch.py` in the Blender folder; the same u, v -> 0.5u, 0.5 + 0.5v move, 10 gun UVs at u = -0.01 clamped to 0
  because the master picks the tank texture with `floor(2u) == 0`). Source: the UE export of each current asset
  (`p3_smexp.py`, `p3_skexp.py`). The KADO FBX of the gun now imports 8.777x too small with either importer (its scaled
  Armature Null), and the UE export round-trips exactly. Bounds, collision, bones, sockets and the physics asset are
  unchanged (Lehf 1867 verts against 1870 before). Script `p3_import.py` (UV-range assert); backups in `backup_p3/`.
  Neither asset is used by any vehicle Blueprint or map right now.
- **Not changed:** `M_PantherII_Turret` and its 4K set are now only on the old `SK_PantherII_Turret`. Only the turret's
  AnimBP, animations, skeleton and physics asset reference that mesh, no Blueprint component does. The wrecks
  (`SM_Destroyed_PantherII_Hull`, `SM_Destroyed_PantherII_Turret`) and `SM_PantherII_TrainingPreview` still show the old
  look. The old hull and its three materials are no longer used by any Blueprint.

## Soviet BA-11 / BA-14 (fender skirts + belly plates)

Built 2026-11-07. `Rus_ArmoredCar_BA11_Soviet` and `Rus_ArmoredCar_BA14_Soviet` are new Soviet (Director of Forge)
children of the stock cars, swapped in for `Tank_Ba12` / `Tank_Ba14` by `DA_Directoriate_UnitOverrides`. They get the
forge-camo `MI_BAHull_Soviet`, Soviet turret children and `SovietArmor/SM_Ba11_Soviet_SpacedArmor_Hull_AddOnPanels`:
per side two skirts hung from the rear fender lip over the rear wheels (|Y| 121.5) and one belly plate under the
fighting compartment (|Y| 100), all in one row along X. The stock cars are unchanged. Details, rebuild steps and
pictures: `Content/RTS_Survival/Blueprints/_HullVariants/BA11/README.md`.

- Zones: component defaults, 15 add-on on 18 mm sides (33 / 25.5 / 18).
- **Armour boxes:** the stock `SetupArmor` has a 5x-scaled `Plate_Front` box over the whole car, so every shot
  resolved `Plate_Front`. The children override `SetupArmor` with the stock boxes copied verbatim (they are
  function-local defaults of the parent graph) plus two unit-scale side boxes, X -250..35, |Y| 72..140, Z -10..200
  (a wider box also caught front-mudguard and rear-corner shots). Validation: all left panels covered, damage /
  repair / service pass; the death step's debris count fails only because the Soviet test car is fog-hidden.
  Armour audit: front and rear probes resolve as on the stock car.
- The baked hulls' complex collision is the stock `SM_Ba11_Hull`, so shot-off plates catch no traces.

## Soviet T-34E (riveted skirt, own end-plate debris)

Converted 2026-11-08. `Rus_Vehicle_T34E` (desert post-apo) and its child `Rus_Vehicle_T34E_Soviet` (Soviet camo) had
riveted side skirts modelled into `SM_T-34E_Hull`, one connected piece per side (x -313 to 300, |Y| 153.5 to 159,
z 34 to 108 in mesh space; the asset is 1.2x the Blender FBX). No Blender work: `BuildPanelHull SplitX=` cuts each
side at the five rivet seams (x -192, -121, -48, 25, 96), where the mesh already has vertex columns.

- **Panels:** 12, left 0-5 and right 6-11, rear to front. Panel 0/6 is the rear plate (122 cm, angled bottom corner),
  1-4/7-10 the four 73 cm middle plates, 5/11 the front plate (204 cm, with the sloped nose). The left front plate also
  carries the small handle on top of it (`MinAbsY=146`). The sloped upper side band with its rib strips reaches inward
  to |Y| 84, so it stays hull. Arguments: `T34E_SKIRT_ARGS` in `build_panel_hulls.py`.
- **Two hulls, one source:** `SM_T-34E_Hull_AddOnPanels` (panel slot `MI_T34E_PostApo_AddOnPanels`) and
  `SM_T-34E_Hull_Soviet_AddOnPanels` (`MaterialOverrides=0:MI_T34E_Soviet`, panel slot `MI_T34E_Soviet_AddOnPanels`),
  so each tank's flying plates are in its own camo (the debris mesh takes the hull's panel material). The Soviet child
  holds its own `Hull` override (mesh + its slot 0 material override), so it got its own `ConvertBlueprint Hull=`.
  Both hulls keep the source's one LOD (5,404 tris), sockets and collision. Masked master: `M_T34E_AddOnArmorPanels`.
- **Debris:** the shared plate is middle panel 2 (`<hull>_Debris`). The four end plates fly off as themselves:
  `ExtractPanelDebris -OwnPanel` for panels 0, 5, 6 and 11 (`<hull>_Debris_Panel<i>`), 10 meshes over both hulls. The
  faction stamps sit on the upper hull side and the glacis, not on the skirt, so no debris plate carries one.
- **Armour:** structural sides 61 -> 46 (`Make ArmorSettings` literals, `ConvertBlueprint Armor=`), zones at the
  default 15, so the intact total stays 61. No `Plate_SideLower*` is registered (the component warns and covers the
  side plates only). `AddArmorBoxes` added unit-scale side boxes in the parent's `SetupArmor` (`Make Array` pins 6 and
  7): `Plate_SideLeft` / `Plate_SideRight`, 46, X -312 to 297, |Y| 140 to 175, Z 30 to 115. Every left panel centre,
  the end plates included, resolves the side plate. Not changed: the armour audit (`ArmorPlateAudit_T34E_SkirtAfter`,
  0 wrong) has one side probe off: from the right at x -252, z 80 the shell meets the collision hull at Y 126.5,
  inside the box's |Y| 140 start, and resolves the legacy `Plate_FrontUpperGlacis` box (45). Such right-rear shots do
  not reach the right zone. The left mirror probe hits the collision at |Y| 153 and resolves the side. Lowering the
  boxes' inner |Y| to about 120 would cover it (the Z 115 cap keeps the upper side band out).
- **Soviet damage reduction:** the Soviet child's BeginPlay calls `AddHealth(200, DamageReductionToAdd=5)` after 3 s.
  The validation's 1-point test shells were swallowed by that flat reduction, so no module damage resolved; the test
  now fires one point through the tank's reduction (`GetHullDamagePerShell`). Real shells are far above 5.
- **Checks** (`UT_B4Howitzer`, DebugGame DLL, `-AddOnArmorPanelsRevealTank` because the fog hides Soviet-faction test
  tanks): see the validation logs `AddOnArmorPanels_T34E_Skirts.log` and `AddOnArmorPanels_T34E_Soviet_Skirts.log`.
  Death blast: 6 bursts (rear, front and shared middle mesh per side).
- **Not changed:** the wreck and `SM_TrainingPreview_T34E` still show the skirts. The old `SM_T-34E_Hull` is no longer
  used by a Blueprint.

## Prepared vehicles

Hulls built by `Tools/AddOnArmorBreak/build_panel_hulls.py`: `<hull>_AddOnPanels` next to each source mesh. Only the
Pz II F Blueprint uses its prepared hull so far. Converting one means three Blueprint edits:
- swap the mesh (`Hull`, or `Armor` on the IS Harvest);
- add `AddOnArmorPanels`;
- move the add-on armour out of the structural plate values.

The candidates came from `RTS.AddOnArmor.SurveyPanels` over all ground-vehicle hulls, checked on side views. Not
prepared: hulls whose outer plates are something else.
- Stuart, M4A3, M10, T-28: suspension parts.
- MS-1: mudguards.
- Pz I 15 cm: the gun shield.
- Sdkfz 251 Incendiary: rocket frames.
- Flak41 Pz III AA: a wire cage plus asymmetric boxes.
- T-44 100L, Panzerjäger I: full-length fenders.

Checked = the panels were checked in game with the component and hull injected:
- **full** = the whole damage, repair and service sequence passed;
- **pictures** = intact, all-hidden and per-panel pictures look right;
- **full except service** = damage and crew repair passed; the map killed the tank during the final service step.
- **(A)** = an `RTS_BP_AssaultTank` child, checked with `-AddOnArmorPanelsStartModuleSetup` (see below).

Runs from 2026-11-05 on `UT_B4Howitzer`. Its artillery can kill a slow test tank, and its smoke can cover pictures.
On `RTS_ToCopySmall` the Brummbar falls through the ground at the test spawn and is removed at Z -1,050,000, also
without injection; the Pz II F does not.

| Vehicle Blueprints | Prepared hull (`_AddOnPanels`) | Panels per side | Kind | Panel material(s) | Checked |
|---|---|---|---|---|---|
| PanzerII_F_SpacedArmor | SM_PZIIF_SpacedArmor_Hull | 5 | plates | MI_Hull_PZII_DesertCamo | full, **converted** |
| PanzerII_F, _RadixiteAutocannon, _Strike | SM_PZIIF_Fenced_Hull | 5 | wire-mesh fences (sections) | MI_Hull_PZII_DesertCamo | full on base + Radixite (Strike not run), **converted** |
| PanzerII_Luchs_SpacedArmor | SM_Luchs_SpacedArmor_Hull | 5 | plates | MI_Hull_LuchsSpacedArmor_Camo | full |
| PanzerIII_M_SpacedArmor | SM_PanzerIIIM_SpacedArmor_Hull | 4 | plates | MI_PZIII_Hull_DesertAPO | pictures |
| PanzerIV_H_StandardPlus | SM_Hull_PzIVH_StandardPlus | 5 | Schürzen slabs | M_Brumbar_Sides | pictures |
| PanzerIV_Commander | SM_PzIV_Commander_Hull | 5 | Schürzen slabs | MI_PZIV_Hull_DesertAPO, M_Brumbar_Sides | pictures |
| Brummbar, Brummbar_DirectSiege | SM_Brummbar_Hull | 5 | Schürzen slabs | M_Brumbar_Sides | pictures (A) |
| Brummbar_LateWar | SM_Hull_Brummbar_LateWar | 5 | Schürzen slabs | M_Brumbar_Sides | pictures (A) |
| E25_Assault | SM_Hull_E25_Assault | 7 | double-faced plates | MI_PantherD_Hull_DesertApo | full (A), **converted** |
| E25, E25_Hunter | SM_E25_Hull_Skirts, SM_E25_Hunter_Hull_Skirts | 5 | plates (baked on) | MI_PantherD_Hull_DesertApo | full (A), **converted** |
| SDKFZ-140_Bunker | SM_Sdkfz140_Bunker_Hull | 6 | plates | M_Sdkfz-140_Hull | full (A) |
| E75 | SM_E75_Hull | 5 | plates | M_E75_Addons | pictures |
| PantherG | SM_Hull_PantherG | 5 | Schürzen slabs | MI_PantherG_Hull_Green, M_PantherG_Attached_Tracks | pictures |
| Hetzer | SM_Hetzer_Camo_Hull | 3 | plates | MI_Hull_Hetzer_Camo | pictures (A) |
| Hetzer_Flame | SM_HetzerFlame_Camo_Hull | 3 | plates | MI_Hull_HetzerFlame_Camo | pictures (A) |
| Hetzer_Mortar | SM_HetzerMortar_Camo_Hull | 3 | plates | MI_Hull_HetzerMortar_Camo | pictures (A) |
| JagdPanther_Bollwerk | SM_JagdPanther_Bollwerk_Hull | 4 | wire screen sections | M_JagdPanther_Hull | full except service (A) |
| ArmoredCar_Sdkfz_234_3_StummelAssault | SM_Sdkfz234_3_StummelAssault_Hull | 4 | wire screen sections | MI_Hull_Sdkfz_234_3 | full (A) |
| SturmTiger_Scatterer | SM_SturmTiger_Scatterer_Hull | 5 | cage sections | MI_SturmTiger_Hull_DesertApo | pictures (A) |
| Rus IS-1_Harvest, IS-2_Harvest | SM_IS_1_SpacedArmor_Spiked (bolt-on) | 6 | sections | MI_IS_1_Harvest | full on IS-1, see notes |
| PantherD | SM_PantherD | 6 | plates | MI_PantherD_Hull_DesertApo | full, **converted** |
| Jaguar, Jaguar_Flanker | SM_Ger_Jaguar_Hull, SM_Jaguar_Flanker_Hull | 5 | plates | MI_PantherD_Hull_DesertApo | full, **converted** |
| Rus ArmoredCar_BA11_Soviet, _BA14_Soviet | SM_Ba11_Soviet_SpacedArmor_Hull | 3 | skirts + belly plate (baked on) | MI_BAHull_Soviet | see BA section, **converted** |
| PantherII | SM_PantherII_Strike_Hull | 4 | plates (cut apart in Blender) | MI_PantherII_Hull_SD | full, **converted** |
| Rus T34E, T34E_Soviet | SM_T-34E_Hull (two targets) | 6 | one-piece skirt cut at its seams (`SplitX`) | MI_T34E_PostApo / MI_T34E_Soviet | full, **converted** |

All are `Ger_Vehicle_*` Blueprints unless marked Rus. Panel materials are `<material>_AddOnPanels` next to the hull.

**Assault tanks need vehicle-module setup first.** Every **(A)** vehicle is an `RTS_BP_AssaultTank` child. That
master never calls `BeginPlay_OnModulesComplete_FinalizeVehicleModules`; only `RTS_BP_TrackedTankMaster` does. Without
that call there are no vehicle modules, and the add-on zones install as part of module setup, so nothing happens. The
validation can start it with `-AddOnArmorPanelsStartModuleSetup`. The real fix is a call in `RTS_BP_AssaultTank` after
its armour setup, and that turns on engine, track and ammo modules for every assault tank as well.

**Assault-tank armour boxes do not fit the default zones.** Default zones cover `Plate_SideLeft` and
`Plate_SideLowerLeft`, and the right-side pair. On these vehicles the skirts sit in front of other plates, so each
needs its own zone plate types. Several also have mislabelled or missing plates:

| Vehicle | Shells into the left skirts resolve to | Plates not registered |
|---|---|---|
| Brummbar, DirectSiege | `Plate_SideLowerRight` (rear 2), `Turret_SideLeft`, `Turret_Front` (front 2) | `Plate_SideRight` |
| Brummbar_LateWar | `Plate_SideLowerRight` (rear 2), `Turret_SideRight` (!), `Turret_Front` (front 2) | `Plate_SideRight` |
| E25_Assault | `Plate_Rear` (5), `Plate_Front` (2) | `Plate_SideLower*` |
| Hetzer, Hetzer_Mortar | `Turret_Rear`, `Plate_RearUpperGlacis`, `Turret_Front` | `Plate_SideLeft/Right` |
| Hetzer_Flame | `Turret_Rear`, `Plate_RearUpperGlacis`, `Turret_Front` | - |
| SturmTiger_Scatterer | `Plate_RearUpperGlacis`, `Turret_Rear`, `Turret_SideLeft`, `Turret_Front` (2) | `Plate_SideLeft/Right` |
| JagdPanther_Bollwerk | `Plate_SideLowerLeft` (all 4, works with the defaults) | `Plate_SideLeft/Right` |

The left skirts of the Brummbar family resolving to right-side plates points at swapped or mirrored boxes in their
armour setup. The full audit of all assault tanks (`AssaultTankArmorAudit.md`) found two causes: legacy scaled boxes
cover extent × scale², so front, rear and turret boxes swallow side shots, and the Brummbar boxes have the wrong labels.

SturmTiger_Scatterer: `Turret_SideLeft` / `Turret_SideRight` already carry 15 add-on armour before any zone installs.
The zones never touch those plates; `SetAddOnArmorOfPlateType` matches the plate type exactly. So that permanent 15
comes from the Blueprint's own armour setup. Move it into a zone, or drop it, when converting.

Per-vehicle notes for the Blueprint conversion:
- No `Plate_SideLower*` is registered on Pz III M, Pz IV H StandardPlus, Pz IV Commander, E75 and Panther G. Their
  zones cover only `Plate_SideLeft` / `Plate_SideRight`. The component warns once per plate type it cannot cover,
  and skips a zone that is left with none.
- Shells on the end panels can resolve to the hull boxes instead of the sides, so they do not reach the add-on zone:
  - Pz II F: the two rear panels resolve to `Plate_Rear`, the front panel to `Plate_FrontUpperGlacis`.
  - Pz III M: one panel resolves to `Plate_Front`.
  - Pz IV H / Commander: the end panels resolve to `Plate_RearUpperGlacis` and `Plate_Front`.
- IS-1 / IS-2 Harvest: the skirts are a separate mesh, component `Armor` on `TrackMesh`. Swap that mesh, not the
  hull. The component then covers the plates of the hull mesh, through the armour setup's `MeshWithArmor0`.
  - IS-1 run (2026-11-05): damage, repair and service passed on the left zone.
  - Blueprint finding, not changed: a shell into the middle of the right skirt resolves to `Plate_SideLeft`, not
    `Plate_SideRight`. So the left zone's add-on armour shows on the right side as well, and the run fails
    "the right side keeps its armour". The IS armour setup needs its right side plate checked.
  - The end panels resolve to `Plate_RearUpperGlacis` (1), `Plate_Front` and `Plate_FrontUpperGlacis` (2).
  - No `Plate_SideLower*` is registered.
- Hetzer: the non-camo `SM_HetzerFlame_Hull` and `SM_HetzerMortar_Hull` are used by no Blueprint and were skipped.
- Panther D, Jaguar, Jaguar Flanker (converted 2026-11-07): the skirts were already modelled into the hulls. The hanger
  brackets stay on the hull when a plate flies off.
  - All three throw one debris plate, `SM_PantherD_AddOnPanels_Debris`, cut from Panther D panel 5 (front left).
  - The faction sigil (`MF_Apply4xSquareStamp`, custom primitive data 0) is drawn on the middle left plate. It keeps
    rendering through the masked panel material, and it goes when that plate is shot off. Never cut debris from the
    sigil plate: Niagara has no primitive data, so every flying plate would carry a sigil.
  - Armour: structural sides 35 on the Panther D (was 50) and 20 on the Jaguar (was 35; the Flanker inherits its
    `SetupArmor`), each + 15 add-on. Intact totals are unchanged. The sides are `Make ArmorSettings` literals, which
    `ConvertBlueprint` does not set, so they were set by hand.
  - No `Plate_SideLower*` is registered on either; the component warns and covers `Plate_Side*` only.
  - Skirt boxes (2026-11-07). At first only the middle panels sat over the side plates, because the scaled front and
    rear boxes come first in the check order and swallowed the end panels. Each `SetupArmor` now has two extra
    `Make ArmorSettings` entries (`Make Array` pins 5 and 6): a unit-scale `Plate_SideLeft` / `Plate_SideRight` box
    with an identity transform and literal Min/Max in hull-mesh space, the space of the layout's panel centres. The
    audit's plate dump lists them as index 0 and 1 (promoted boxes sort first), in actor space: the Jaguar hull sits
    16.45 cm below its actor, so its boxes show Z 63.6 to 129.6 there. Unit-scale
    boxes are tested before every scaled box, so any hit inside them resolves the side. The old scaled side boxes stay
    unchanged, so the hull above the skirts and the roof keep resolving as before. Armour values are the sides'
    own: 35 on the Panther D and 20 on the Jaguar, with the add-on set on every plate of that type.
    - Panther D: X -385 to 193, |Y| 150 to 200, Z 92 to 166. The panels span X -383 to 192, Z 100 to 158. Above
      Z of about 165 the glacis reaches the outer corners, so the box stops there.
    - Jaguar (the Flanker inherits it): X -243 to 247, |Y| 140 to 190, Z 80 to 146. The panels span X -241 to 245,
      Z 84 to 142.
    - Checks: `run_panels_validate.sh` on `UT_B4Howitzer` (`AddOnArmorPanels_{PantherD,Jaguar,JaguarFlanker}_SkirtBoxes.log`)
      reports every left panel "covered" and RESULT PASS. `run_armor_audit.sh` (`ArmorPlateAudit_SkirtBefore` /
      `SkirtAfter`) shows every front and rear probe resolving the same plate as before, 0 wrong. Side probes at
      skirt height now resolve the side plates (NOTE counts: Panther D 18 to 12, Jaguar 10 to 4, Flanker 12 to 8).
    - Not covered: the running gear below the skirts. Side shots there still resolve the front or rear boxes.
  - `Ger_Vehicle_PanzerV_III` also uses `SM_PantherD` and still has the old hull.
- E-25 family (converted 2026-11-07). `Ger_Vehicle_E25` is the parent of the Assault and the Hunter, so the panels
  component and `SetupArmor` live on it and the children only override the hull mesh.
  - E-25 and E-25 Hunter had no skirts. `Tools/AddOnArmorBreak/bake_e25_skirts.py` copies the Jaguar's plate set
    (plates, bolts, hanger brackets) onto them: 45 cm outward, 3.5 cm up, 3% shorter along X, so the plates hang under
    the fender between the mudguards (X -238 to 232, |Y| 194 to 210, Z 88 to 145). It writes `SM_E25_Hull_Skirts` and
    `SM_E25_Hunter_Hull_Skirts` (duplicates of the stock hulls, so collision carries over) and their `_AddOnPanels`.
    Both throw the Panther D debris plate. The Jaguar plate UVs carry the sigil stamp, so the middle plate shows the
    faction sigil, as on the Jaguar. On the Hunter the plates hang behind the side foliage, which stays on the hull.
  - E-25 Assault keeps its own scrap skirts (7 per side) and their own debris plate.
  - Armour: structural sides 15 (was 30) + 15 add-on, so the intact total is unchanged.
  - Skirt boxes, as on the Panther D / Jaguar: two unit-scale entries (`Make Array` pins 4 and 5) in the E-25's
    `SetupArmor`, X -241 to 235, |Y| 185 to 215, Z 84 to 154, which covers both skirt styles. Without them every side
    shot resolved `Plate_Front` or `Plate_Rear` (the legacy scaled boxes) and no plate could be shot off.
  - `RTS_BP_AssaultTank` now calls `BeginPlay_OnModulesComplete_FinalizeVehicleModules` in `SetupModules`, so the
    zones install without `-AddOnArmorPanelsStartModuleSetup`.
  - Checks: `run_panels_validate.sh` on `UT_B4Howitzer` (`AddOnArmorPanels_{E25,E25Assault,E25Hunter}_SkirtBoxes.log`):
    every left panel covered, RESULT PASS. `run_armor_audit.sh` (`ArmorPlateAudit_E25_SkirtAfter`): 0 wrong, and all
    54 front and rear probes resolve the same plate as in `ArmorPlateAudit_AfterModules` (2026-11-05). Side shells hit
    the stock collision hull at |Y| 190, inside the skirt boxes; the plates themselves have no collision.
