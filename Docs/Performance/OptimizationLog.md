# Render optimization log (started 2026-11-07)

Scope: Ger_FirstMission, Ger_MarderStugMission, SM_Desert_01 (survival desert oasis map).

**Main finding.** At 2560x1080 Epic the GPU frame is ~15-20 ms (contended) and material shading is a small share of it:
the base pass is ~1 ms (landscape is most of that; the oasis water is 0.06 ms, route ribbons 0.005 ms). The expensive
"shaders" are the engine's global passes: Lumen GI (DiffuseIndirectAndAO + LumenSceneLighting ~30-40%), TSR with its
200% history (~25%), deferred lighting, cascaded shadow maps. Those can't be trimmed by material edits without visible
change, which is why D1-D3 below are owner decisions. The invisible wins found so far were elsewhere: a distance-field
cache rebuild every frame (fix #1), idle pooled projectile Niagara sims (#25), and ~57 over-detailed meshes + 6 infantry
skeletal meshes (#2-#24).

## Tools (`Source/Tools/PerfAudit/`)

| Tool | What it does |
|---|---|
| `run_inventory.sh` + `perf_inventory.py` | Headless read-only inventory per map: every drawn static mesh (instances, LOD tris, LOD screen sizes, Nanite), skeletal meshes, every material (VS/PS instruction counts, samplers, blend, two-sided), landscape, Niagara, a 60 m density grid. Output `Saved/PerfAudit/inventory_<map>.json`. `BIN=DebugGame` uses the DebugGame -Cmd. |
| `inventory_report.py <Map...>` | Heaviest meshes/materials from the inventory. |
| `effective_lod_report.py <dist> <Map...>` | Triangles actually drawn at the RTS camera distance (LOD picked from screen sizes), and tris per screen size (over-detailed meshes). |
| `run_gpu_probe.sh <name> <map>` | Offscreen `-game` at Epic with `UPerfProbeSubsystem` (`-PerfProbe`, C++ in `Utils/PerfProbe/`): starts the game, waits for shaders + PSOs, holds fixed cameras, measures GPU frame ms, optional A/B console variants and ProfileGPU dumps with per-material draw events. |
| `profilegpu_report.py <name>` | Summarises the ProfileGPU dumps of a probe run. |

Facts: RTX 4070, D3D12 SM6, Lumen GI + Lumen reflections, distance fields on, non-Nanite meshes almost everywhere.
Shadows are classic cascaded shadow maps, NOT virtual shadow maps (corrected 2026-11-08): `[/Script/Engine.RendererSettings]`
sets `r.Shadow.Virtual.Enable=0`, and the `r.Shadow.Virtual.Enable=1` under "START REALISTIC SHADOW SETTINGS" sits inside
`[/Script/Engine.VirtualTexturePoolConfig]`, where the engine ignores it (probe logs set the cvar once, to 0; ProfileGPU shows
`ShadowDepths > Atlas0 6144x2048`, three 2048 cascades, ~1.2 ms contended). See D8.
Overall Quality sets every scalability group (incl. GI, reflections, AA) before the five menu groups override theirs, so
Overall Low already turns Lumen off (GI Low/Medium have no Lumen).
The settings menu exposes only ViewDistance/Shadows/Textures/Effects/PostProcess scalability groups
(`FRTSScalabilityGroupSettings`); anything behind "Low" for Lumen GI, reflections or shading needs that struct extended (C++).

## Status (2026-11-08)

- Done without visible change: 35 entries below (C++ fixes #1, #25, #35; ~160 static meshes reduced or re-LODed; 25 skeletal
  meshes given an LOD1), every one checked with in-game before/after or forced-LOD close-ups (#35 with a live battle log).
- Done for Low only: L1-L2 (static LOD distance scale, TSR presets no longer overridden at Low).
- Waiting on the owner: D3-D10 (biggest: D7 Nanite off in gameplay worlds 6-13%, D3 Lumen mesh SDF tracing off ~5%).
  Declined by the owner: D1 (GI stays full at High/Epic), D2 (TSR stays 200% at the highest settings). Nothing from D3-D10 is applied.
- What is left without owner input is small: after round 9 the heaviest meshes drawn on the three maps are background
  mountains/cliffs and single-digit-k props; materials are ~1 ms of base pass in total (no single material above ~0.1 ms in
  clean captures). The frame is Lumen (DiffuseIndirectAndAO ~3.5 ms, surface cache ~1-2 ms), TSR (~1.5 ms), Nanite fixed
  passes (~0.9 ms, D7) and CSM shadows (~0.6 ms) at 2560x1080 Epic (desert, round 3, lightly contended).
- Baselines: the 2026-11-07 table ran beside a Shorts render (absolute ms ~1.5x inflated). Round 3 (2026-11-08) ran on a much
  quieter GPU (~8 ms frames); its per-view numbers are in ab3 (Saved/PerfAudit/ab3.txt). The A/B deltas are the numbers to trust.
- Nothing is committed: the C++ and docs are in the Source submodule working tree; ~120 content packages were saved by
  commandlets, which the editor's revision-control plugin does not auto-commit. A stale editor that re-saves one of these
  packages would overwrite its reduction.

## Baselines (contended, see Status)

| Map | View | GPU frame median (ms, 1080p Epic) | Notes |
|---|---|---|---|
| Ger_FirstMission | start (-32118,32400,2952, pitch -70) | 4.99 | fog-of-war PP off (`ShowFlag.PostProcessMaterial 0`); other sessions' engine runs share the GPU, so ±0.5 ms run-to-run noise |
| Ger_FirstMission | (-4074,-21000,2950) | 3.57 | |
| Ger_FirstMission | (-28074,-21000,2950) | 3.94 | |
| Ger_FirstMission | (-28074,9000,2950) | 3.54 | |

Typical frame split (FM, 1080p Epic): Lumen DiffuseIndirectAndAO ~1.1 ms, ShadowDepths 0.5-0.8, LumenSceneLighting 0.6,
PostProcessing 0.5, DeferredLighting 0.4-0.5, BasePass only 0.13-0.29 ms. Material shading is a small share at 1080p.

## Collision check of reduced meshes

All 57 reduced meshes checked (`mesh_collision_info.py`). Complex-as-simple (collision = reduced render mesh): Luchs hull/turret, FlakLuchs
turret, RusDigIn heavy walls, HQ_BuildingMesh, Comm Center, Barracks, Armory preview — all at 20-50% where close-ups show no shape change,
so their collision shape is unchanged in practice. The 6-15% meshes all use default collision (convex hulls for gameplay); the only
complex traces in game code are landscape-channel height traces. No collision regression.
Rounds 6-7 (79 meshes): complex-as-simple are the two PostApo bridges (50/60%, drivable deck unchanged in close-ups), the live
hulls at 45-60% (StuG III, Pz IV family, Maus, Sd.Kfz. 251, Tiger desert hull) and, below 40%, only SM_Ger_HQ_Preview (placement
ghost) and SM_Destroyed_SMK_Hull (wreck) at 30%; the 10% stick mine uses default collision. No projectile
"No valid niagara component" errors in any probe log after fix #25.

## Inventory findings (2026-11-07)

- Ger_FirstMission: landscape `MI_RTS_Landscape_DesertPaintable`, 2795 components; 136 materials; heaviest PS: MI_Glass (translucent, 1267), M_Trenches 574, M_DistanceMesh 513, 15 FakeGodRay MIDs (additive two-sided, 455).
- Ger_MarderStugMission: landscape `MI_RTS_Landscape_DryCastle_Inst`, 1156 components; MM_GerSolder 564, M_RTS_BlendedRoad 555 (60 uses).
- SM_Desert_01: landscape DesertPaintable, 196 components; M_SM_OasisWater translucent 1086 PS; 688 translucent route-path strips (MI_SM_RouteTwoTrack); 2004 dead-shrub instances (masked two-sided).
- Over-detailed meshes (single LOD, tens of thousands of tris for a few dozen pixels): RadixiteCore_1/2, Electric_Pillar_With_Support_1/2, Barrier_BarbWire2, Bofors_Base, Flak88, GarrisonSandbagWall_Rus_1 and more (see Done table).

## Incidents

- 2026-11-07 14:42: my Development editor build failed to link (another session's in-progress `AAbilityPreviewDirector`), UBT deleted
  the old `UnrealEditor-RTS_Survival.dll`. Resolved 14:49: that session rebuilt it once its code compiled.

## Done: no visible loss

All LOD0 reductions use the static mesh's own LOD0 reduction settings (source model kept): revert any by setting
`percent_triangles` back to 1.0 (`mesh_reduce.py restore <mesh>`).

| # | Asset | Change | Gain | Verified | Date |
|---|---|---|---|---|---|
| 1 | Infantry weapons + squad-unit equipment (C++ `RTSRenderPerf::ExcludeSmallAttachmentsFromDistanceFields`, called in `AInfantryWeaponMaster` and `ASquadUnit` PostInitializeComponents) | Weapon/equipment static meshes made Movable and taken out of distance-field lighting and the Lumen scene. Ger_Weapon_Mauser had a non-movable mesh on an animated hand, re-added to the *cached* global distance field every frame (found with `r.GlobalDistanceField.Debug.LogModifiedPrimitives 1`) | FM start view: `UpdateGlobalDistanceField` 1.04 -> 0.23 ms, GPU frame median 4.99 -> 4.23 ms | Start-view shots: same lighting/shadows; 15 cm meshes add nothing visible to DF AO/shadows or bounce light | 2026-11-07 |
| 2 | SM_Luchs_Hull (Ger_Vehicle_PanzerII_Luchs, FM start units) | LOD0 reduction 20% | 127,177 -> 25,435 tris per hull (drawn in prepass, base, shadow, Lumen) | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 3 | SM_Luchs_TurretCasing | LOD0 reduction 25% | 40,719 -> 10,180 | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 4 | SM_InfWeapon_PSS43 (Soviet PPSh-43, every soldier carrying it) | LOD0 reduction 6% | 57,489 -> 3,449 (MP40 is 5,168) | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 5 | SM_Electric_Pillar_With_Support_1 / _2 | LOD0 reduction 20% | 23,320 -> 4,664 / 21,608 -> 4,321 | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 6 | SM_RTS_Barrier_BarbWire2 | LOD0 reduction 30% | 20,266 -> 6,079 | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 7 | SM_Bofors_Base | LOD0 reduction 35% | 18,518 -> 6,482 | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 8 | SM_Flak88 (embedded turret) | LOD0 reduction 40% | 20,610 -> 8,243 | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 9 | SM_GarrisonSandbagWall_Rus_1 | LOD0 reduction 30% | 18,280 -> 5,484 | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 10 | SM_Rus_Sandbag_DoubleStraight, SM_Rus_ConcreteSandWall, SM_powerplant_005 | LOD0 reduction 50% | 8,296 -> 4,148; 13,736 -> 6,868; 17,006 -> 8,503 | in-game close-ups (2.6x radius) before/after identical | 2026-11-07 |
| 11 | SM_BXP_37mm_Construction (construction mesh of 45 Soviet BXPs) | LOD0 reduction 8% | 151,028 -> 12,083 | in-game close-ups before/after identical | 2026-11-07 |
| 12 | SM_BXP_37mm_BuildingMesh (8 Soviet bunkers) | LOD0 reduction 10% | 125,684 -> 12,569 | in-game close-ups before/after identical | 2026-11-07 |
| 13 | SM_Flak88_BuildingMesh / SM_Flak88_ConstructionMesh (German 88 BXP) | LOD0 reduction 30% / 20% | 56,232 -> 16,870 / 76,834 -> 15,367 | in-game close-ups before/after identical | 2026-11-07 |
| 14 | SM_CommunicationCenter_BuildingMesh | LOD0 reduction 50% | 50,853 -> 25,426 | in-game close-ups before/after identical | 2026-11-07 |
| 15 | SM_Forge_Cranes_Optim | LOD0 reduction 40% | 46,756 -> 18,702 | in-game close-ups before/after identical | 2026-11-07 |
| 16 | SM_PE8_Bomber_Frame / SM_Sturmovic_Frame (Soviet aircraft) | LOD0 reduction 35% / 30% | 47,839 -> 16,744 / 51,358 -> 15,407 | in-game close-ups before/after identical | 2026-11-07 |
| 17 | SM_Destroyed_Wespe (+ _RadixiteHowitzer, _CamouflagedNest), SM_Destroyed_Sdkfz7Flak (+ _Searchlight) scavenge wrecks | LOD0 reduction 20% | ~48-58k -> ~10-11.5k each | in-game close-ups before/after identical | 2026-11-07 |
| 18 | SM_RusDigIn_HeavyWall2 / SM_RusDigIn_HeavyWall | LOD0 reduction 25% | 46,624 -> 11,656 / 38,784 -> 9,696 | in-game close-ups before/after identical | 2026-11-07 |
| 19 | SM_Skdfz-7_Hull_WithAttachments (German HQ truck) | LOD0 reduction 35% + removed 3 imported LODs that barely reduced (78k/74k/72k); revert via git | 87,381 -> 30,584 | in-game close-ups before/after identical | 2026-11-07 |
| 20 | Round 4 (21 single-LOD meshes): SM_Destroyed_HQTruck 20%, SM_BigSolar_BuildingMesh 40%, SM_NomadicMetalFurnace 40%, SM_Ger_Barracks_BuildingMesh 50%, SM_Flak88_Plateau 30%, SM_PakBunker_Construction/BuildingMesh 20%/35%, SM_SKDFZ-7_Hull / _HullTracks / SM_HQTruck_Expanded 40%, SM_GerMedFactory_BuildingMesh 50%, SM_Ger_Bxp_HQRadar_ConstructionMesh 30%, SM_FlakLuchs_TurretCasing 30%, SM_Destroyed_BM13 30%, SM_Armory_PreviewMesh 25%, SM_Destroyed_Maxim 15%, SM_Destroyed_Sdkfz251_AmmoCarrier 30%, SM_37mmFlak_Base 35%, SM_Ger_Bxp_TentRepair_BuildingMesh 40%, SM_InfWeapon_RailgunAdvanced 15% | LOD0 reduction | each 25-78k -> 4-20k (total ~790k -> ~250k) | in-game close-ups before/after identical | 2026-11-07 |
| 21 | Skeletal infantry: SK_Ger_HeavilyArmored (Ger_Inf_Mauser/Faust), SK_InfantryMaster, SK_WWII_GermanSoldier_varA (all German pit crews), SK_Rus_CortexOfficer, SK_Rus_Project24, SK_SovietBackroomHazmat | One generated LOD1 at 12% triangles from screen size 0.15 via shared `SKLOD_RTS_Infantry` (LOD0 untouched; soldiers are ~0.05 screen size at RTS zoom) | LOD1 verts: 67.5k->12.3k, 23.3k->3.7k, 26.3k->3.2k, 41.8k->6.4k, 175.8k->28.1k, 28.1k->5.0k | forced LOD0 vs LOD1 close-ups identical; automatic LOD picks LOD0 in close-up and LOD1 at RTS distance (probe lodcheck). Engine limit: a soldier never yet on screen that only casts a shadow into view keeps LOD0. SK_WWII_GermanSoldier_varA LOD0 screen size reads 0.0: harmless (selection reads LOD1's 0.15), leave it | 2026-11-07 |
| 22 | HQ_BuildingMesh (deployed German HQ) 50%, SM_Flak88_Preview2 15%, SM_RadixiteCore_1/2 15%/12%, SM_HQ_BuildingMesh (enemy/nomadic HQ) 40% + removed 3 imported LODs (186k/177k/174k), SM_OpelRefinery_BuildingMesh 50% | LOD0 reduction (were locked earlier, saved 2026-11-07 ~17:45) | 62k->31k, 60k->9k, 14.5k->2.2k / 21.8k->2.6k, 205k->82k, 40k->20k | in-game close-ups before/after identical | 2026-11-07 |
| 23 | LOD1 never reached at RTS zoom -> LOD1 screen size raised to 0.8 (LOD2+ unchanged): SM_B4Pit_SteppedSquare, SM_53KPit, SM_53KSectorPit, SM_Hull_TigerH1_ASPS, SM_Destroyed_Hull_TigerH1_ASPS, SM_Destroyed_TigerRail_RailBunker, SM_Destroyed_TigerRail_Overcharged_Hull, SM_Destroyed_Maus_AA_Hull, SM_GerCommCenter_MG42Nests, SM_PzIV_Destroyed, SM_B4ConcretePit_Sandbags, SM_BS3Pit, SM_GerMortarPit, SM_GerFlak88Pit | LOD screen sizes only (no geometry change) | drawn tris ~halved per instance (e.g. B4 concrete pit 59k -> 30k, PzIV wreck 18.5k -> 2.8k) | forced LOD0 vs LOD1 close-ups identical | 2026-11-07 |
| 24 | SM_Panzer1_Harvester_Hull 40%, SM_FlakBunkerSandBags 35%, SM_MountZil 40% | LOD0 reduction + removed imported LODs that barely reduced | 18.9k -> 7.6k, 18.4k -> 6.4k, 19.3k -> 7.7k | LOD1 (90% of LOD0) shot identical; after shots pending | 2026-11-07 |
| 25 | AProjectile (C++, `Weapons/Projectile/Projectile.cpp`): pooled tank shells | Dormant (hidden) projectiles now `DeactivateImmediate()` their Niagara component; `OnRestartProjectile` re-activates it (reset) at the launch spot. Before, every dormant shell in the pool (64 on FM) kept dispatching its GPU sim each frame | NS_75mmShell GPU dispatches per frame 128 -> 0 when no shells fly (~0.05-0.8 ms depending on load) | verified in a live duel (RTS_ToCopySmall, -VehicleDuelBench Tiger_vs_KV1 + probe dumps): fired shells draw their smoke + ribbon renderers, ~14 shell sims/frame while firing vs 128 idle before; hits recorded normally | 2026-11-07 |
| 26 | Infantry weapons (carried by every soldier of the squad type): MG34 25%, FlameThrower2 25%, DyakonovRGL 25%, Bazooka 25%, SKS 40%, Coilgun 40%, FG42 45%, MP40 45%, DS39 50%, FlameThrower3 50%, PTRS_50MM 50%, KarSniper 55%, GerRailgun 55%, PanzerShrek 60%, SVT-40 65% | LOD0 reduction (weapons are ~0.03 screen size at RTS zoom) | 3.5-10.3k -> ~2.2-2.6k each | in-game close-ups before/after identical | 2026-11-07 |
| 27 | SM_PostApo_Bridge_1 50% (x4 on Marder), SM_PostApo_Bridge_3 60%, SM_Sdkfz251_AmmoCarrier_Hull 45%, SM_DessertTiger_Hull 60% | LOD0 reduction | 46.0k->23.0k, 17.3k->10.4k, 23.8k->10.7k, 31.3k->18.8k | in-game close-ups before/after identical | 2026-11-07 |
| 28 | Vehicle skeletal meshes: SK_SKDFZ-7-Tracks, ZilTruck, SK_Sdkfz7Flak_SearchlightTurret, SK_Sdkfz7Flak_Turret, SK_Sdkfz7Flak36_Turret, SK_Ger_SFH18Anim, SK_SKDFZ-251_Wheels, SK_Bofors_WeaponSystem | Generated LOD1 at 30% triangles from screen size 0.5 via shared `SKLOD_RTS_Vehicle` (LOD0 untouched) | LOD1 verts ~35-45% of LOD0 (e.g. HQ tracks 38.3k -> 13.7k); auto LOD = LOD1 at RTS zoom | forced LOD0 vs LOD1 close-ups identical; lodcheck auto LOD1 at RTS distance | 2026-11-07 |
| 29 | SK_Ger_Aircraft_Me410, SK_Ger_Aircraft_FW_Recon, Bf109_WingAnims, HQ Crane | Generated LOD1 at 30% from screen size 1.5 via `SKLOD_RTS_Aircraft` (they stay above 0.5 at RTS zoom) | Me410 29.0k -> 10.2k verts, FW 26.7k -> 9.4k, Bf109 14.0k -> 5.4k, crane 11.9k -> 4.5k | forced LOD0 vs LOD1 close-ups identical | 2026-11-07 |
| 30 | SK_WWII_GermanSoldier_equpB, SK_Rus_ArmoredMosin, SK_Cyborg_Armored | Generated LOD1 (12% from screen size 0.15, `SKLOD_RTS_Infantry`) | LOD1 verts 9.1k->1.5k, 20.4k->2.6k, 11.5k->2.6k | forced LOD0 vs LOD1 identical; auto LOD1 at RTS distance | 2026-11-07 |
| 31 | Round 7 (60 single-LOD meshes 12-43k: wrecks 20-30%, construction/preview meshes 25%, base buildings/props 40-50%, live hulls StuG III / Pz IV / Pz IV G / Pz IV F1 spotter / Pz IV harvester / Maus AA / Maus / Sd.Kfz. 251 + mortars 40-60%, SM_Ger_AntiInfantry_StickMine 10%): SM_Destroyed_Sdkfz7Flak36 20%, SM_SKDFZ-251_Hull 40%, SM_PanzerIIIM_Destroyed 25%, SM_Hull_Maus_AA 50%, SM_2HullMaus 60%, SM_DestroyedGerMedFactory_Mount 30%, SM_GerMedFactory_Mount 50%, SM_Hull_PzIVF1_Spotter 60%, SM_Ger_Bxp_HQRepairHub_TentRepairAt50 40%, SM_PZ38_T_Destroyed_1 30%, SM_Ger_HQ_Preview 30%, SM_StugIII_BaseMesh 60%, SM_PZ4Harvester_Hull 50%, SM_Hull_PzIVG_DoubleTurret 60%, SM_PanzerIV_Hull 60%, SM_Sdkfz251_RadixiteCharged_Destroyed 30%, SM_Destroyed_SMK_Hull 30%, SM_Destroyed_Sdkfz234_3_IncendiarySupport 30%, SM_Bxp_LeFH_ConstructionMesh 25%, SM_Ger_Bxp_HqLaser_ConstructionMesh 25%, SM_Sdkfz251_HeavyAA_Destroyed 30%, SM_Experimental_PreviewMesh 25%, SM_RadixiteConverter_BuildingMeshAt25 50%, SM_Tent 40%, SM_DestroyedCommunication_Mount 30%, SM_BarracksTent 40%, Sdkfz_251_mortar 50%, SM_SmallCrane 40%, SM_Destroyed_Sdkfz234_3_StummelAssault 30%, SM_Destroyed_Sdkfz250_Howitzer 30%, SM_Rus_FactoryPlatformAt66 50%, SM_Destroyed_Sdkfz250_37 30%, SM_HetzerFlame_Destroyed 30%, SM_Ger_Bxp_HQRadar_BuildingMesh 50%, SM_Sdkfz251Mortar_GuidedMissile_Destroyed 30%, SM_Ger_Bxp_HQRadar_Radar 50%, SM_Rus_FactoryPlatform 50%, SM_WallBnk_Damaged 50%, SM_Destroyed_Sdkfz250 30%, SM_Ger_Bxp_HQRadar_BuildingMeshAt66 50%, SM_RadixiteConverter_BuildingMeshAt50 50%, SM_RadixiteConverter_BuildingMesh 50%, SM_HetzerMortar_Destroyed 30%, SM_Ger_AircraftBuildingPreview 25%, SM_MechanizedDepot_Mount 50%, SM_Rus_FactoryPlatformAt33 50%, SM_Ger_Bxp_HQRadar_BuildingMeshAt33 50%, SM_MetalDepot_BuildingMesh_NewPreview 25%, SM_Rus_FuelStorage2 50%, SM_Sdkfz251Mortar_Incendiary_Destroyed 30%, SM_MidWallBnk_Damaged 50%, SM_OpelRefinery_PreviewMesh 25%, SM_Destroyed_Sdkfz234_3 30%, SM_Destroyed_ZilTroops 30%, Sdkfz_251_MortarTW 60%, SM_T35_Destroyed 30%, SM_Ger_AntiInfantry_StickMine 10%, SM_Destroyed_Sdkfz250_AmmoCarrier 30%, SM_sdkfz251_Destroyed 30%, SM_Solar_pannel_Large_1 50% | LOD0 reduction | 1,125,490 -> 438,333 tris in total | in-game close-ups before/after identical (Maus re-shot without an aircraft shadow) | 2026-11-07 |
| 32 | SM_Rus120MortarPit (LOD1 0.25 -> 0.8), SM_Rus_ResearchCenter + _At33 + _At66 (LOD1 0.9 -> 1.5) | LOD screen sizes only (LOD1 was never reached at RTS zoom) | mortar pit 22.3k -> 7.8k drawn, research center 17.7k -> 8.8k drawn | forced LOD0 vs LOD1 close-ups identical | 2026-11-07 |
| 33 | Round 9 (fresh inventory 2026-11-08): SM_RusDigIn_HeavyWallAt50 25% (damaged state of the two walls reduced in #18), SM_PostApo_Car_1/2/3 50% (scavenge cars), SM_Industrial_Dumpsters_2 40%, SM_Destroyed_PZIII_J 50% (FM wrecks), SM_Rus_GuardMgTower 50% (Soviet MG tower BXP), SM_RusInfBunker_03 50%, SM_powerplant_003 50% | LOD0 reduction | 28.1k->7.0k, 4.7k->2.4k (x3), 6.2k->2.5k, 6.4k->3.2k, 6.8k->3.4k, 9.4k->4.7k, 7.5k->3.7k | in-game close-ups before/after identical (only passing aircraft shadows differ); collision: HeavyWallAt50 complex-as-simple at the same 25% as its siblings, the rest use simple shapes | 2026-11-08 |
| 34 | SM_TWDigIn_BP_TW_ML20 (dig-in of the SFH 18 team weapon) | LOD1 screen size 0.262 -> 0.8 (LOD1 was never reached at RTS zoom) | 35.1k -> 17.6k drawn | forced LOD0 vs LOD1 close-ups identical | 2026-11-08 |
| 35 | ASmallArmsProjectileManager (C++): the small-arms tracer Niagara system | Hidden (SetVisibility false) once no tracer has been in flight for 2 manager ticks (0.5 s); shown again on the next FireProjectile. Before, all 1024 pool slots were drawn every frame (D6: translucency + velocity draws, 0.13-0.29 ms idle on the desert) | ~0.1-0.3 ms whenever no rifle/MG fire is in flight | live main-menu battle (StartGame, DebugGame, temporary log): hidden at t=0.6 s, shown on the first shot at 29.9 s, hidden at 49.4 s when fire paused, shown again at 49.65 s, etc.; the component stays active (sim keeps running). Tracer pixels after a re-show were not captured on screen; SetVisibility only removes/re-adds the render proxy | 2026-11-08 |

## Done: Low setting only (scalability)

| # | Asset / setting | Low behaviour | Epic behaviour | Date |
|---|---|---|---|---|
| L2 | TSR cost settings (History.ScreenPercentage, History.UpdateQuality, ShadingRejection.Flickering, RejectionAntiAliasingQuality, Resurrection) moved from DefaultEngine.ini [SystemSettings] (which overrode every scalability level, so Low players paid Epic TSR) into DefaultScalability.ini [AntiAliasingQuality@2/3/Cine] | Low only: engine TSR preset (100% history, resurrection off) -> ~8% GPU at Low per the D2 measurement | Medium/High/Epic/Cine unchanged: identical project values in [AntiAliasingQuality@1/2/3/Cine] (verified per level in -game: Low 100/0, Medium + Epic 200/1) | 2026-11-07 |
| L1 | `Config/DefaultScalability.ini` (new): r.StaticMeshLODDistanceScale per View Distance level | Low only 2.0: static mesh LODs (incl. the new LOD1s) switch at half the distance; skeletal meshes already get r.SkeletalMeshLODBias 2 from the engine | Medium/High/Epic/Cine 1.0, set explicitly so switching back resets it (verified per level in -game) | 2026-11-07 |

## Pending saves (package held open by a running game; retry when no game runs)

2026-11-08: SM_MiscCrane_Welding 30% (1,552 tris at 6 cm radius, forge crane attachments) and SM_GerPit_Clip_37 30% (1,452 tris,
Flak 37 pit clip) failed to save: the owner's open editor holds both files. Re-run
`run_mesh_reduce.sh reduce r9 <mesh>=0.3` when that editor is closed (tiny gain).
Earlier: all retried saves succeeded 2026-11-07 ~18:50. Gotcha: some packages (HQ, radixite cores, pits, MountZil) load in
every game, so any running -game (also the probe's own) blocks saving them.

## A/B measurements (engine settings)

Ger_FirstMission, 4 views, 2560x1080 Epic, variants alternated in one run, 2 repeats (another session's Shorts render shared
the GPU, so absolute ms are ~1.5x inflated; the relative deltas are consistent across views):

Ger_MarderStugMission (2 views) agrees: mesh SDF off -0.6..-1.6 ms, TSR 100 -1.4..-2.4 ms, GI High ~-4.6 ms, DF shadows / shadow High ~0.

| Variant | GPU delta | Visible change at RTS camera |
|---|---|---|
| r.DistanceFieldShadowing 0 | ~0 (FM and desert) | none -> no gain, dropped |
| r.Lumen.TraceMeshSDFs 0 | FM -0.7..-1.4 ms (~5%); desert -0.5..-1.7 ms (oasis -7.5%) | none seen in FM shots; to check on oasis water / reflections |
| r.TSR.History.ScreenPercentage 200 -> 100 | FM -1.2..-2.1 ms (~8%); desert -1.1..-1.7 ms | none seen at 1:1 crops, but the 200% history is a deliberate sharpness choice |
| sg.GlobalIlluminationQuality 3 -> 2 | FM -3.6..-4.1 ms (~18%); desert -3.4..-5.4 ms (oasis view -24%) | none seen in stills; High halves Lumen probe density (possible GI noise in motion) |
| sg.ShadowQuality 3 -> 2 | ~0 | dropped |

Round 2 (FM + desert, 2 views each, 3 repeats): r.TSR.Resurrection 0 (-0.4 ms FM, -0.5 ms oasis), r.TSR.History.GrandReprojection 0,
r.TSR.History.UpdateQuality 2, r.TSR.Velocity.Extrapolation 0, r.TSR.16BitVALU 1, r.Shadow.Virtual.ResolutionLodBiasDirectional -0.5
(a no-op: VSM is off, see Facts): all within the +-0.5 ms run noise -> no change made. TSR's cost is the 200% history (D2), not its sub-settings.

Round 3 (2026-11-08, FM + desert, 4 views each, 2560x1080 Epic, 2 repeats; the GPU was much less contended: ~8 ms frames):

| Variant | GPU delta | Visible change |
|---|---|---|
| r.Lumen.TranslucencyVolume.Enable 0 | FM -0.26..-0.43 ms, desert -0.27..-0.30 ms (~3.5%) | oasis water identical within the time drift of the run; lit smoke in combat not yet checked -> D9 |
| r.LumenScene.DirectLighting.UpdateFactor 64 + Radiosity.UpdateFactor 128 (half-rate surface cache updates) | FM -0.10..-0.18 ms, desert -0.19..-0.28 ms (~2%) | none in stills; GI reacts slower to moving lights / explosions -> D10 |
| r.TSR.ShadingRejection.Flickering 0 | ~0 | dropped |
| r.TSR.RejectionAntiAliasingQuality 1 | ~0 | dropped |

Screenshot diffs grow with capture order (v1 < v2 < v3 < v4) on every view: the scene drifts during a run (sun/cloud
shadows), so pixel diffs between variants can't separate setting changes from drift; judge by eye on matched crops.

## Ideas to discuss with the owner (visible loss risk)

| # | Idea | Expected gain | Visual risk |
|---|---|---|---|
| D1 | **Declined by the owner (2026-11-08): High/Epic keep full Lumen GI.** Epic GI uses High-level Lumen settings (or a separate "Global Illumination" option in the settings menu, Low/High/Epic) | ~18% GPU at 2560x1080 on FM | stills identical; Lumen at half probe density can flicker/noise in motion |
| D2 | ~~r.TSR.History.ScreenPercentage 200 -> 100~~ **Declined by the owner (2026-11-08): the highest settings keep the 200% history.** Only Low uses the engine TSR preset (L2) | ~8% GPU | slight softness in motion; 200% was set deliberately (Jan 2025) |
| D3 | r.Lumen.TraceMeshSDFs 0 at Epic (global SDF only) | ~5% GPU | contact GI/AO near small objects and Lumen reflections on water; looked identical on FM |
| D5 | Fog of war Niagara (FX_Syst_FoWSystem): 10 "Propagate Vision" iterations over a 1024x1024 grid, ~0.6 ms each time it runs (10 Hz with the manager's FowTickRate) -> 512x512 grid or fewer iterations, at least at Effects Low | ~0.3 ms/frame avg at 60 fps, removes a 0.6 ms spike every 100 ms | softer / slower-converging fog edges |
| D6 | **Idle half done by #35** (system hidden while no tracer flies; still draws every slot during fire). Remaining: RTS_FX_Syst_Projectiles (all small-arms tracers, one particle per pool slot): renders with Niagara's DefaultSpriteMaterial (no material assigned) and draws every pool slot every frame, also with no shots in flight (indirect draws of 36k-72k instances in translucency + velocity: 0.13-0.29 ms/frame idle on the desert). Fix in the Niagara asset: kill/skip inactive slots (or cull zero-size sprites) and give it a cheap unlit material | 0.1-0.3 ms always | material choice changes the tracer look; skipping inactive slots is invisible |
| D7 | Nanite off in gameplay worlds. A/B `r.Nanite 1|0` (2560x1080, 4 repeats): FM -2.8 / -1.6 ms, desert -1.4 / -1.3 ms (6-13%): Nanite::VisBuffer 0.58 + Readback 0.46 + forced depth prepass 0.25 ms vanish and VSM shadow depths drop 1.7 -> 0.5 ms. The three maps hold almost no Nanite content (FM 1 mesh, Marder 2 solar panels, desert the translucent route ribbons), but 512 Nanite meshes are used project-wide (campaign globe outlines up to 600k tris, hangar, Kessel rocks), so a project-wide r.Nanite=0 would show fallback meshes elsewhere. Proposal: turn Nanite off on the ~97 low-poly RTS_Survival Nanite meshes (units/wrecks/farm/towers, all < 10k tris except Traeger C 44k) and set r.Nanite 0 at runtime in mission/survival worlds (keep it on for campaign + hangar). Engine detail (2026-11-08): the Nanite passes run whenever ANY Nanite mesh is loaded in memory (`FDeferredShadingSceneRenderer::ShouldRenderNanite` = `GStreamingManager.HasResourceEntries()`), drawn or not, so converting only the meshes a map places gains nothing; desert round 3 still paid Nanite::VisBuffer ~0.85 ms with only translucent route ribbons (which draw their fallback anyway) | ~1.3-2.8 ms (6-13%) | none for converted low-poly meshes; any missed Nanite mesh would show its (often 1%) fallback in game worlds |
| D8 | The ini meant to enable virtual shadow maps ("REALISTIC SHADOW SETTINGS"), but the line is in the wrong section and the game ships with cascaded shadow maps (RendererSettings `r.Shadow.Virtual.Enable=0`). Left as is: moving the line would change every shadow and the cost. Decide whether the game should use VSM (sharper, cached static shadows; more memory) or keep CSM and delete the dead line | unknown either way (CSM ShadowDepths ~1.2 ms contended at 2560x1080; VSM caching can be cheaper on static scenes, more expensive with many moving units) | shadows change look (contact detail, softness) |
| D9 | r.Lumen.TranslucencyVolume.Enable 0 at High/Epic (Lumen GI for lit translucency: smoke, dust, glass, water) | ~0.3-0.4 ms (~3.5%) | oasis water unchanged; lit smoke/dust would lose bounce light (flatter in shadowed areas). Not done: the highest setting keeps full quality |
| D10 | Half-rate Lumen surface cache updates (DirectLighting.UpdateFactor 64, Radiosity.UpdateFactor 128) at High/Epic | ~0.15-0.25 ms (~2%) | stills identical; bounce light lags behind muzzle flashes / explosions / moving lights |
| D4 | SM_Luchs_SpacedArmor_Hull (+ _AddOnPanels) 128k tris: same 20% LOD0 reduction as the plain Luchs | ~100k tris per tank | owned by the armor-panel work (baked masked panels); needs that system's panel extraction re-run |
