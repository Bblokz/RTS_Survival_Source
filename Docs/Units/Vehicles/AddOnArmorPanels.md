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

## Converting a vehicle

1. Find the panels. Panel geometry = mesh islands lying entirely at |Y| >= `MinAbsY`, within `MinZ`..`MaxZ`, and no
   longer than `MaxIslandLength` along X. Longer islands are rails and stay part of the hull.
   - Plates (`PanelsPerSide=0`): islands of at least `MinPanelArea` cm² are panels. Two panels that overlap along X
     by more than half are the two faces of one plate and merge. Smaller islands (bolts, clamps, stiffeners) join
     the panel of their side whose X range holds them.
   - Wire screens and cages (`PanelsPerSide=N`): each side's panel geometry splits into N equal sections along X.
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
| PanzerII_Luchs_SpacedArmor | SM_Luchs_SpacedArmor_Hull | 5 | plates | MI_Hull_LuchsSpacedArmor_Camo | full |
| PanzerIII_M_SpacedArmor | SM_PanzerIIIM_SpacedArmor_Hull | 4 | plates | MI_PZIII_Hull_DesertAPO | pictures |
| PanzerIV_H_StandardPlus | SM_Hull_PzIVH_StandardPlus | 5 | Schürzen slabs | M_Brumbar_Sides | pictures |
| PanzerIV_Commander | SM_PzIV_Commander_Hull | 5 | Schürzen slabs | MI_PZIV_Hull_DesertAPO, M_Brumbar_Sides | pictures |
| Brummbar, Brummbar_DirectSiege | SM_Brummbar_Hull | 5 | Schürzen slabs | M_Brumbar_Sides | pictures (A) |
| Brummbar_LateWar | SM_Hull_Brummbar_LateWar | 5 | Schürzen slabs | M_Brumbar_Sides | pictures (A) |
| E25_Assault | SM_Hull_E25_Assault | 7 | double-faced plates | MI_PantherD_Hull_DesertApo | pictures (A) |
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
