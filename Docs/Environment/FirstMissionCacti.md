# German first mission cactus dressing

Added 84 cactus actors in 24 loose patches to `Ger_FirstMission`, using 16 existing Blueprint variants and 18 resulting meshes. The map now has 148 cactus actors including the original 64. The added actors are named `LandscapeCactus_Pxx_yy` in `Environment/Cacti_Landscape`.

The mix contains 34 small bushes, 37 medium bushes, 7 large bushes and 6 taller desert cacti. Bushes use both green and blue variants. Uniform scale varies between 0.88 and 1.13, with varied yaw. Existing Blueprint assets were used as-is, retaining their destruction and natural-cover behavior.

Most patches extend existing cactus growth areas. Additional groups occupy suitable dry flats and gentle terrain around the wider landscape. Placement requires a landscape collision hit, ground height between 90 and 1600 cm, and a surface normal Z of at least 0.96. Plant roots sit 2.5 cm into the terrain; the tall meshes extend up to about 21 cm below their actor pivot, keeping their bases buried.

Placement excludes road and rail spline corridors by 1000 cm plus the estimated plant radius, mission spawn/waypoint locations by 1800 cm plus radius, and expanded building/bridge footprints. Plant spacing accounts for bush size and existing cacti. The twelve cloud-linked landscape rays remain in place.

Coordinates, Blueprint paths, scales, actor labels and resulting meshes are recorded in `FirstMissionCactusPlacements.json`. Ground contact was inspected and road/mission-point clearances were checked after spawning. No Blueprint graph, shared material or C++ changes were made.

The pre-placement map is backed up at `Saved/CodexBackups/FirstMissionCacti_20261018/BeforeCacti_000226.umap`. Preview images are `Saved/Screenshots/CodexLighting/FirstMission_Cacti_Before.png`, `FirstMission_Cacti_After.png` and `FirstMission_Cacti_Close.png`.
