# Tank track trails on the landscape

Tracked tanks leave twin ruts in soft ground (grass, desert, sand and gravel layers) on landscapes using `M_RTS_Landscape_DryCastle` or `M_RTS_Landscape_DesertPaintable`.

| Part | Where |
|---|---|
| Trail writer | `Source/RTS_Survival/Environment/TankTrails/TankTrailSubsystem.h/.cpp` (`UTankTrailSubsystem`) |
| Benchmark | `Environment/TankTrails/Tests/TankTrailBenchmarkSubsystem.h/.cpp`, runner `Source/Tools/TankTrails/run_bench.sh` |
| Material chain | `MF_RTS_LS_TankTrails` (Attributes, SoftGround -> Attributes, RutCentre), built by `Source/Docs/Landscape/ls_trails_build.py`. `M_RTS_Landscape_DryCastle` calls it through `ls_master_build.run()` (`Landscape/LandscapeMaster.md`). `M_RTS_Landscape_DesertPaintable` calls it through `run_desert_paintable()` in the same file |
| Assets | `/Game/RTS_Survival/Blueprints/Environment/TankTrails/RT_TankTrails` (R8 render target, saved at 4x4), `MPC_TankTrails` |

## How it works

1. **One render target covers the whole landscape.**
   - At BeginPlay, `UTankTrailSubsystem` resizes `RT_TankTrails` to the landscape bounds at 16 cm per texel, capped at 8192.
   - On Ger_Trains that is 3938 x 5907 R8, about 23 MB of video memory.
   - It writes the bounds and texel size into `MPC_TankTrails`.
   - It resizes the target back to 4x4 when the world ends.
2. **Every tracked tank is tracked.**
   - Tanks are picked up through a scan at BeginPlay plus `OnActorSpawned`, so the tank code needed no changes.
   - Track width (20 % of hull width, clamped to 30–90 cm) and spacing come from the root component's local bounds.
3. **Drawing runs at 10 Hz in one draw call.**
   - Each tank that moved at least 25 cm adds one segment per track, from the last drawn position to the current one.
   - A segment is a full-depth strip (flat rut bottom, soft side edges in vertex alpha) plus a lead-in and a lead-out strip whose depth ramps to zero over 1.5 track widths: 18 triangles.
   - Where the trail goes on, the next segment's full-depth body covers the ramp. Where a tank stopped or reversed, the rut slopes out. Before 2026-10-27 the ends were vertical walls, and the landscape shading turned them into black dots.
   - All segments of an update go into a single `FCanvasTriangleItem`, which is one `CanvasBatchedElements` draw.
   - A jump of more than 15 m is treated as a teleport and not drawn.
4. **Fade.** Every 4 s a full-target translucent black tile fades old trails with a 300 s half-life. The 8-bit fade leaves a faint residue, which the material's smoothstep (`TankTrail_RutLow`) hides.
5. **Material.** Static switch `LS_TankTrails` sits between `MF_RTS_ColorVariation_Landscape` and the main `BreakMaterialAttributes`.
   - Only the main pass is affected; the runtime virtual texture output never contains trails, because its cached pages would need invalidating under every moving tank.
   - A custom HLSL node samples the target three times (centre, +X, +Y) inside `[branch] if (TrailEnabled > 0.5)`, using the shared clamped sampler, so it costs no sampler slot.
   - With trails off, or in the editor where nothing sets the collection, the landscape fetches nothing.
   - The rut darkens the base colour (`TankTrail_Tint`), sets roughness (`TankTrail_Roughness`) and tilts the normal along the rut slope (`TankTrail_NormalStrength`), all masked to the soft layers.
6. **Turning it on for a map.**
   - `TankTrailStrength > 0` on the landscape material tells the subsystem to run; otherwise it does nothing.
   - `MI_RTS_Landscape_DryCastle_Inst` has the switch on and strength 1, so every map on it has trails: Ger_Trains, Ger_DestroyT35, Ger_MarderStugMission and UnitTests/Ger_Trains.
   - `MI_RTS_Landscape_DesertPaintable` has it on too. That covers Ger_FirstMission, RedLine, the two start tutorials, RTS_Shorts and the test and template maps on that instance, 19 maps in all. Its hard layers (DesertRockDark, ScorchConcrete) keep no ruts.
   - The console variable `rts.TankTrails 0` turns trails off at runtime: drawing stops and the material branch is skipped.

## Measured cost (2026-10-27, Ger_Trains, 40 Panzer IV H, 1920x1080, DebugGame build, editor and other sessions running on the same machine)

Run: `run_bench.sh gpu1 -TankTrailBenchProfileGPU` drove 10 off/on pairs of 240 frames each, with tanks shuttling the whole time.

| Measure | Trails off | Trails on | Difference |
|---|---|---|---|
| GPU frame (ProfileGPU median of 10 frames) | 27.15 ms | 27.13 ms | none measurable |
| Base pass (landscape shading, ProfileGPU median) | 1.485 ms | 1.520 ms | +0.035 ms, inside the ±0.05 ms scatter of single-frame captures |
| Trail draw (`CanvasBatchedElements`, 300 triangles) | n/a | 0.01–0.12 ms, every 3rd frame | ≤ 0.04 ms per frame |
| Trail writer game-thread time | 0 | 72–123 µs per update (≈2 µs per tank) | 0.03 ms per frame |
| Median of per-pair differences (wall / game / render / GPU) | | | −0.40 / −1.32 / −0.09 / −0.14 ms |

- **What "off" means:** the off phases run the new shader with its branch skipped (`rts.TankTrails 0`), not the material from before this change. To remove the shader change completely, turn `LS_TankTrails` off in the landscape's instance.
- **Noise:** the frame-time noise from the editor and other work on the machine is several ms per phase, so the pair medians only show that the cost sits below that noise. The ProfileGPU and writer timings are the direct numbers.
- **DebugGame:** the CPU figures come from the unoptimised DebugGame game module, so expect them to be lower in Development or Shipping.
- **Scaling:** writer CPU and the triangle count grow linearly with tanks moving that update. For 200 moving tanks, expect about 0.4 ms per update in DebugGame and 3000 triangles in one draw.

## Benchmark notes

- `-TankTrailBench` on Ger_Trains starts the game, spawns the tanks on open grass next to the player start (`-TankTrailBenchAt=X,Y` to move) and holds a fixed camera.
  - The navmesh never reports idle there, because trains and crop-field modifiers keep rebuilding tiles. The benchmark waits at most 60 s, then spawns.
- Before spawning, the benchmark removes units the map placed within 120 m of the drive centre, so mission enemies don't fight the tanks. Each tick it also clears an engine pause that mission intros set outside the controller's pause flag; Ger_FirstMission does this.
- Other maps: `MAP=/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission run_bench.sh fm "-TankTrailBenchAt=5000,-29000"` puts the drive area on the flat sand east of the player start (verified 2026-10-27).
- The trail writer logs one `LogRTSTankTrails` line per map (proxies, material, strength), plus `Tank trails active: render target WxH` when it runs.
- Output: the `pairdelta` / `summary` / `phase` lines in `Saved/Logs/TankTrailBench_<name>.log`, a per-frame CSV, a capture of the trails and the exported render target in `Saved/TankTrailBench/<name>/`.
- `-TankTrailBenchProfileGPU` dumps one ProfileGPU frame per phase into the log. Compare the `BasePass` and `CanvasBatchedElements` lines.
- The older `ALandscapedeformManager` prototype (Niagara circles into a 256² RGBA16F target, read only by the old `M_RTS_Landscape` master) is unrelated and still placed on most maps.
