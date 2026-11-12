# Deploy animation prototypes (SD HQ) — TECH DEMOS, safe to delete

Two self-contained tech demos of a GPU-only nomadic building deploy for the Strike Division HQ
(`SKDFZ-7/BuildingMeshes/HQ_BuildingMesh`). Nothing in the game uses them: no C++, no Blueprint, no edits
to any existing asset. The original HQ mesh and its materials are untouched.

Open `/Game/RTS_Survival/_Prototypes/DeployAnim/UT_DeployAnimProto` to see the original HQ next to both
prototypes. Both are set to loop (deploy for 12 s, hold, pack up for 12 s, hold). Turn on realtime in the viewport
or press Play. Only scrub mode has been seen working in captures; loop and timed mode are untested.

## Delete everything

| What | Where |
|---|---|
| Demo map | `Content/RTS_Survival/_Prototypes/DeployAnim/UT_DeployAnimProto.umap` |
| Prototype 1 (procedural WPO) | `Content/RTS_Survival/_Prototypes/DeployAnim/WPO/` |
| Prototype 2 (baked VAT) | `Content/RTS_Survival/_Prototypes/DeployAnim/VAT/` |
| Scripts | `Source/Tools/DeployAnimProto/` (this folder) |
| Work files (FBX, .blend, EXR, captures) | `Saved/DeployAnimProto/` |

`WPO/Materials` may still hold `M_DupEAL_Tmp` and `M_NewTest_Tmp`, leftovers from debugging that the editor
would not release. Delete them after an editor restart.

To keep only one prototype, delete the other one's folder. The demo map then shows an empty actor in its place,
or you can delete that actor.

## How progress is driven (both prototypes)

Custom Primitive Data on the mesh component. Only the CPU writes these values, and only once per state change:

| Index | Meaning |
|---|---|
| 0 | Mode: 0 = demo loop, 1 = scrub (index 1 = progress 0..1), 2 = timed (index 1 = start game time) |
| 1 | Scrub progress, or start time in timed mode |
| 2 | Duration in seconds (0 means 12) |
| 3 | Direction in timed mode (negative = pack up) |

To scrub by hand in the editor, select the actor and set Custom Primitive Data Defaults: [0] = 1, then drag [1].
Pack-up is the same function run backwards, so a cancel mid-deploy reverses from where it is.

## Prototype 1: procedural WPO

- **Mesh:** `WPO/SM_HQDeploy_WPO`. UV4-7 hold each piece's pivot, motion mode, start, duration, height and seed.
- **Motion:** `WPO/Materials/MF_HQDeploy_WPO`, a single Custom node (`deploy_wpo.hlsl`). It has four modes:
  - unpack from the truck in an arc, growing from small;
  - drop with a bounce;
  - rise out of the ground (the mast);
  - telescope (antennas).
- **Materials:** copies of the HQ's slot materials with this function on World Position Offset. The two
  truck-only materials keep their originals, and the chain uses a plain copy (its source is a Megascans
  Material Attributes master).
- **Tuning:** the timing and look come from `pieces.py` (category bands) and `deploy_wpo.hlsl`; no textures.

## Prototype 2: baked VAT (rigid pieces)

- **Animation source:** `Saved/DeployAnimProto/HQDeploy_VAT.blend`, with one keyframed object per piece. This is
  the file to hand-edit. The scripted first pass:
  - planks swing down on their truck-side edge;
  - sandbags and props are thrown off the truck bed with a tumble;
  - containers and structures are craned off the truck with a sway;
  - the mast and antennas hinge up from lying flat.
- **Textures:** `vat_bake.py` bakes `VAT/Textures/T_HQDeploy_VAT_Offset` (pivot offset cm + scale) and `_Rot`
  (quaternion). Each is 484 pieces x 181 frames, RGBA16F, about 700 KB of GPU memory each.
- **Mesh:** `VAT/SM_HQDeploy_VAT`, where UV4-5 hold the piece column and the pivot.
- **Shader:** `VAT/Materials/MF_HQDeploy_VAT` (`deploy_vat.hlsl`) runs two texture fetches per vertex.

## How the material copies are made (`ue_proto_materials.py`)

Each prototype has its own copies of the HQ slot materials with the deploy function on World Position Offset.
Duplicating the masters was not enough: for seven of them a `duplicate_asset` copy never compiled the added WPO,
and no flag change fixed it. So copies are built three ways:

- **Duplicated:** pump, barrels, Panther G track plates and Panzer III camo. These duplicates compile the
  offset; skinned-mesh and Niagara usage are switched off on them (those paths allow only 4 UV sets).
- **Graph exported and imported into a new material:** sandbag/deck atlas, props atlas, prop master and
  switchboard.
- **Hand-wired in `ue_special_copies.py`:** Panther II sheets/parts (team-colour camo function) and the
  camo/concrete atlas (atlas-cell functions). The UECP graph export drops the wires into material-function inputs.
- **Plain material:** the chain (its source is a Megascans Material Attributes master).

UECP's validator and node listing don't see material-function inputs either: wired call inputs show up as
"island" nodes. The vertex-shader instruction count from `MaterialEditingLibrary.get_statistics` is only a hint:
originals read 289 or 554 depending on their usage flags, and the deploy copies read 460-511. The real proof was
the scrub captures: progress 0 shows only the truck and progress 1 the whole HQ, in both prototypes. Because the
tools can't see function inputs, the look of the hand-wired copies (Panther II, camo/concrete atlas) is unverified.

**Possible look difference, unverified:** the truck and the container on its bed render with the yellow/black
Panzer III camo in both prototypes, but grey on the original HQ mesh. The original was captured once with no
shader warm-up, and grey is also what a material that is still compiling looks like. Check it in the demo map.

## Rebuild steps

Blender 4.0 runs headless. Unreal steps run in the editor (UECP `execute_python` / `execute_uecp_script`).

1. Export `HQ_BuildingMesh` to `Saved/DeployAnimProto/HQ_BuildingMesh.fbx` (StaticMeshExporterFBX).
2. Build the shared piece table: `blender -b --factory-startup -P pieces.py -- HQ_BuildingMesh.fbx <Saved/DeployAnimProto>`.
   This writes `pieces.json` and a category check render.
3. Prototype 1:
   1. `build_wpo.py`, then import with `ue_import.py`.
   2. `ue_mf_wpo.py`, then `ue_proto_materials.py` with SUFFIX `_DeployWPO`.
4. Prototype 2:
   1. `vat_animate.py` writes the `.blend`. Hand edits go into it.
   2. `vat_bake.py`. Add `--preview` for Blender frames only.
   3. Import with `ue_import.py`, then `ue_vat_textures.py`, `ue_mf_vat.py`, and `ue_proto_materials.py` with SUFFIX `_DeployVAT`.
5. Run `deploy_headless.py` in a throwaway editor (its docstring has the command line). It places the demo
   actors, saves the map and writes the scrub captures.

## Known limits (both)

- **Normals don't rotate with pieces.** Lighting on a spinning piece is slightly off until it lands; the rest pose
  is exact. This mostly affects prototype 2 and prototype 1's unpack spin.
- **Shadows and Lumen:** shadow maps follow the moving geometry. Distance-field shadows and Lumen use the rest
  pose, so lighting lags while pieces are in the air.
- **Bounds:** the component bounds scale is 3, so pieces in flight are not culled.
- **Collision:** none during the animation. This is visual only.
- **No dust or sound yet.** The piece table already has the landing times to drive pooled dust and sounds.

## Deploy effects (review of 3 setups, 2026-11-10)

- **Event timeline:** `vat_events.py` bakes each piece's first contact from the .blend into
  `Saved/DeployAnimProto/deploy_events.json`. It clusters contacts into 89 events for the HQ: deck rows, wall
  runs, structure groups, the mast. Re-run it after hand edits to the .blend.
- **Building blocks:** `VAT/FX/NS_Deploy_LandDust`, `_HeavyDust`, `_Sparks` and `_Exhaust` are stateless, lit
  smoke-puff layers plus `M_DeploySpark`. They're built with `deploy_fx_recipe.py` and
  `Source/Tools/ImpactVfx/build_systems.sh`; sprite size follows the user parameter `ImpactSpriteScale`.
  The Smoke Screen setup borrows `NS_DestructionSmoke`.
- **Borrowed dust systems don't work here:** the project's existing GPU dust systems (`NS_RTS_GroundDust*`) cull
  beyond 10 m from the camera (`MaxCameraDistance` 1000), so they never show at RTS distance.
- **Review runner:** setups live in `deploy_fx_setups.py`. `deploy_fx_headless.py` runs PIE at a fixed 30 fps:
  the same frame counter drives the VAT scrub and fires the effects, with one warm-up pass, then a recorded pass.
  `make_fx_videos.py` builds the clips.
- **Stand-in:** the review spawns one component per effect (up to 159 per deploy). Production plays the same
  timeline from pooled systems instead.
