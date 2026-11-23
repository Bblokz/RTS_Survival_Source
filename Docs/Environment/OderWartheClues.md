# Oder-Warthe Act 1 clue spots: prop sets

Two clue spots for `Ger_OderWarthe` (decision: `Source/Docs/Campaign/OderWartheMissionFlow.md`, section 11). Each is one
Blueprint with its props, decals and a slot for the portrait-line trigger. Neither is placed in the map yet.

The user picked these concepts on 2026-11-22: wagon **A "Sealed teplushka"** and camp **A "Field tent station"**. The
concept sheets are in `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Clues/Concepts/v01`.

## Blueprints

All paths are under `/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Clues/`.

### BP_OW_Clue_QuarantineWagon

A 1946 Soviet two-axle covered wagon at the rail yard's buffer stop.

- **Wagon:** sealed. Doors are wired shut with a lead seal, and two planks are nailed across the door in an X. A typed
  order is nailed on the door.
- **Stencils (decals):**
  - КАРАНТИН / СЕКТОР ЛЕБУС (quarantine / Sector Lebus).
  - ЗЕРНО ЗАРАЖЕНО / «ФИТОФАГ» / НЕ ВСКРЫВАТЬ (grain contaminated / "Fitofag" / do not open).
  - НКПС 41-7719 / 20 т / 1946 (wagon number).
  - A white cross on the roof, so the RTS camera reads it.
- **Platform props:** grain sacks, one burst sack with mouldy purple grain, a barrier at the ramp slope and a quarantine
  board.
- **Origin:** the loading ramp actor `RailYard_Ramp` (`ow_rail.ramp_transform()`). To place the Blueprint, give it
  the same location, yaw and z as the ramp. In the ramp's local frame:
  - The track centre runs along Y at X = -260.
  - The buffer stop face is at Y = +775, and the ground is at z = 0.
  - The platform deck is at 125, on the +X side, which is the side the RTS camera sees.
  - The wagon is coupled against the buffer, with the rail top at z = 37.
  - It is 10.4 m over the buffers, so it clears turnout T1 (20 m from the buffer).

### BP_OW_Clue_SamplingCamp

An abandoned algae sampling camp.

- **Tent area:** an army ridge tent and a field table with a rack of sample jars. Glass carboys of purple lake water
  sit in slat crates. There are also crates stencilled ОСТОРОЖНО / СТЕКЛО · ПРОБЫ (careful / glass / samples), a drum
  and a cold fire pit.
- **Shore and water:** a rowing boat pulled up on the mud, with an oar and a purple spill. In the water stand a
  red/white staff gauge and a float line.
- **Signs:**
  - ВНИМАНИЕ! ВОДОЁМ ЗАРАЖЁН (attention, the water is contaminated).
  - КУПАТЬСЯ И БРАТЬ ВОДУ ЗАПРЕЩЕНО (no bathing, no taking water).
  - ПУНКТ ОТБОРА ПРОБ № 3 (sampling point no. 3).
- **Origin:** world (-7000, -5400, 0). This is the Mittelsee waterline (water z = 0) on the west shore, about 25 m
  west of the island causeway near its south end. Local +Y points to the water.
- **Prop heights:** baked from the map heightmap at that spot by `ue_clues_build.py` (`ground_z`). The camp sits on a
  1.7 m terrace, a 10 m slope and a mud strip 0.3 to 0.5 m above the water. If the spot moves, rerun the build with a
  new `CAMP_ORIGIN`.

### Trigger slot (both Blueprints)

- `TriggerSlot` (scene component) holds a ChildActorComponent `PortraitTrigger`. It is a
  `BP_ScriptTrigger_PortraitLine` with portrait `GERMAN_ANNOUNCER` and no voice line yet.
- Radius: 1300 cm for the wagon (on the yard ground in front of the platform) and 1200 cm for the camp (on the terrace).
- Set the voice line on the child actor template when it exists. An unset line fires silently.

## Assets

| What | Where |
|---|---|
| Meshes | `Clues/Meshes/SM_OW_Clue_*`: Teplushka, GrainSpill, Barrier, SignBoard, SignPost, RidgeTent, FieldTable, JarRack, JarsLoose, Carboy, StaffGauge, FloatLine, FirePit, AlgaePuddle, Oar |
| Collision | Teplushka, RidgeTent and SignBoard keep UCX hulls (block, and the wagon and tent affect nav); the rest are walk-through dressing |
| Materials | `Clues/Materials/MI_OWClue_*`: M_OW_Kit children (wagon paint, roof tar, canvas on the Greek_island fabric texture, glass, grain, ash, red/white paint, rope); `MI_OWClue_AlgaeSample` is a child of `MI_Phytophage_BlackPlum_Solid`, tinted to the Mittelsee algae |
| Decals | `Clues/Decals/MI_OWClueDecal_*`: children of the Ore Mountains `M_OreCliffDecal` (BCA/RMAO textures, same convention) |
| Reused | Kit MIs (`MI_OW_Wood`, `RustSteel`, `DarkSteel`, `Void`, `Granite`); `SM_GrainBag_1/2` (Modular_MedievalTown_WI); `SM_CrateSmall`, `SM_Barrel` (MilitaryBaseCamp); `SM_Boat_02` (Greek_island, scaled 0.6/0.45/0.55) |

## Tools

- **Decal textures:** `Source/Tools/OderWartheMap/clues/gen_ow_clue_decals.py` writes to `Saved/OderWarthe/clues/decals`.
  Run it with `PYTHONUTF8=1`, because the source has Cyrillic text.
- **Concepts:** `D:/Blender/.../OderWarthe/Clues/_Tools`, which holds `render_cp.py`, `compose_cp.py` and `cp_parts.py`.
- **Meshes:** `D:/Blender/.../OderWarthe/Clues/Model/clue_build.py` (Blender 5.0). It uses the kit library
  `Source/Tools/OderWartheMap/assets/ow_blender_lib.py`. Wood UVs are swapped so the grain follows each plank.
  `clue_preview.py` renders a Blender preview.
- **Unreal:** `Source/Tools/OderWartheMap/clues/ue_clues_build.py` (commandlet). Steps:
  textures, materials, meshes, blueprints. It writes `Saved/OderWarthe/clues/ow_clues.json`.
- **Review:** `ue_clues_review.py` runs in a throwaway editor. It builds the ramp, the track and the real shore patch
  200 m above `RTS_ToCopySmall`, with a water sheet on `MI_OderWarthe_AlgaeWater`. It saves nothing, and the shots go
  to `Saved/OderWarthe/clues/review`.
