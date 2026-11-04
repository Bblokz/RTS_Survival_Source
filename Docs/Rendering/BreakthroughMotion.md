# Breakthrough building motion

Breakthrough buildings are static concrete. To make them feel alive, small parts of them move through world
position offset (WPO): nets and canvas in the wind, fans, exhaust rain flaps, and later hooks and antennas. There
are no skeletal meshes, no Blueprint ticks, and nothing on the CPU.

Build script: `Source/Tools/BreakthroughBuildings/ue_bf_motion_materials.py`. All assets live in
`/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/Motion`.

## The kit
| Function | Moves | Driven by |
|---|---|---|
| `MF_BF_MotionClock` | nothing; shared by the others | Time, scaled by `MotionTimeScale` (default 1; set 0.1 in an instance to film in slow motion); Phase (a hash of the object position, so neighbours never move in step); Gate (`MPC_RTSWPO.WPOIntensity`, the trees' wind setting, clamped to 0–1); Running = 1 − `MotionStopped` (custom primitive data **16**) |
| `MF_BF_Flutter` | cloth: nets, canvas, tarps, flags | UV channel 2 = (sway, lift) weights; UV channel 3 = sway direction in local XY. A mesh without those channels does not move. Sway only pushes outwards, so a net hanging against a wall never goes into it. Lift breathes up and down. |
| `MF_BF_Spin` | fans, rotating vents | Turns about local Z through the mesh pivot. Running 0 stops it. |
| `MF_BF_Rattle` | exhaust rain flaps, hatches | Lifts the local +X edge about local Y through the pivot, in puffs. Running 0 closes it. |
| `MF_BF_Sway` | hooks, chains, antennas (not used yet) | Pendulum about the pivot, scaled by its Weight input. |

`M_BF_CamoNet` has a static switch `MotionFlutter`. `M_BF_Trim` has `MotionFlutter`, `MotionSpin` and
`MotionRattle`. All switches are off by default, so every existing user of these materials compiles exactly as
before. The instances switch one on:
`MI_BF_CamoNet_Flutter`, `MI_BF_Trim_Flutter`, `MI_BF_Trim_Spin`, `MI_BF_Trim_Rattle`.

Instance parameters (group "BF Motion"):
- Flutter: `FlutterSwayCm`, `FlutterLiftCm`, `FlutterSpeed`.
- Spin: `SpinTurnsPerSecond`.
- Rattle: `RattleLiftDeg`, `RattleRestDeg`, `RattlePuffsPerSecond`.
- Sway: `SwayAmpDeg`, `SwayRate`.

Cost: 93–125 vertex-shader instructions on the moving sections only; the pixel shader is unchanged.

## Rules for a building
- **Rigid parts** (fan, flap, hook) are their own small static mesh, with the pivot at the hub or hinge. They sit
  on a socket of the building mesh and are placed as components of the building's dressing attachment. The material
  rotates about the pivot, so nothing is baked into them.
- **Cloth that is part of the building mesh** gets its weights from the Blender build, written into UV channels 2 and
  3 (`motion_uvs` in `MechanizedDepot/Model/depot_kit.py`). UVs are used instead of vertex colours because they pass
  through FBX and Unreal without a colour-space change, and every vertex without a weight reads 0. LightMapUV stays
  on channel 1.
- Unreal's FBX importer stores every V as 1 − v. MF_BF_Flutter flips both y values back, so a weight of 0 written in
  Blender reads 0 in the shader. Without that flip, every unweighted vertex read lift 1, and the whole mesh breathed.
- Never put a Flutter instance on a mesh that has no weight channels: if UV channels 2 and 3 are missing, the
  shader reads whatever channel is there instead.
- Don't read a UV channel past the last one with `StaticMeshDescription.get_vertex_instance_uv` from Python. It
  doesn't raise an exception: it hits an engine assertion and takes the editor down.
- Virtual shadow maps are on in this project, and a primitive whose material has WPO re-renders its shadow each
  frame. Keep moving sections on small meshes. A big mesh with a few moving sections can set the component's
  `Shadow Cache Invalidation Behavior` to Rigid: its shadow then stays in the rest pose.
- A net lying on a canvas (as on the depot's tilt) breathes in step with it only while `MI_BF_CamoNet_Flutter` and
  `MI_BF_Trim_Flutter` share the same `FlutterLiftCm` and `FlutterSpeed`. Tune both together, or the net sinks into
  the canvas or floats off it.
- Stopping machines (low power): set custom primitive data 16 (`MotionStopped`) to 1 on the moving components; 0 or
  unset runs them. Wind is never stopped. Nothing writes it yet; `UEnergyComp::OnBaseEnergyChange` is the natural hook.
- Why the slot means "stopped" (measured 2026-11-03 in the Oxide Works module test): an unset custom primitive data slot
  reads 0 on a component, not the parameter's default. The kit first read the slot as Running, so a component that never
  wrote it stood still: the frames were pixel-identical until `set_custom_primitive_data_float(16, 1.0)`. Since the flip
  to `MotionStopped`, a component nobody touches moves, and no Custom Primitive Data Defaults are needed.
- Vertex colours can carry motion data too, but only as values a gamma curve leaves alone: 0 and 1 in RGB, anything in alpha
  (the Oxide Works module, `OxideWorks/Modules/Model/README.md`). Its forge materials use this kit's clock.

## Mechanized Depot (first user, 2026-11-03)
- The vault nets, the canvas tilt and the net thrown over it flutter on weights from the spec. Weights are 0 at the
  hoops, tie lines and wherever the cloth lies on the ground.
- A cooling-fan drum on the generator bunker replaces the middle roof vent. Its blades are `SM_BF_MechDepot_Fan`
  on socket `GenFan`.
- The two generator exhausts have rain flaps, `SM_BF_MechDepot_ExhaustFlap` on `ExhaustFlap1/2`.
- Tuned by eye on 2026-11-03:
  - Flutter: sway 30 cm, lift 6 cm, on both the net and the trim instance.
  - Rattle: rest 10°, lift 22°, 1.2 puffs/s.
  - Spin: 2.2 turns/s.
- `BF_MechanizedDepot_Truck`'s `BuildingMeshComp` uses Rigid shadow caching, so the cloth on the big mesh doesn't
  redraw its virtual shadow every frame. The fan and flaps are small components and keep live shadows.
- Filming the motion: put the building on its own in the editor level and turn the viewport to realtime. Set
  `MotionTimeScale` to 0.1–0.25 on the instances, unsaved, and take viewport screenshots in a burst. In a game, the
  RTS camera's smoothing runs on game time, so slowing the game makes the camera drift.
- The faction flag already waves through `M_FlagPole`, which is shared by every faction and not part of this kit.
