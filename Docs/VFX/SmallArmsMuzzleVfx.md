# Small arms muzzle flash and smoke

Every infantry small arm (rifles, SMGs, assault rifles, MGs, pistols, AT rifles) draws its muzzle flash and powder
smoke through **one Niagara system for the whole map**, the same idea as the trace impacts and the dust devils:
however many soldiers fire, the cost is one Niagara component and one CPU simulation per emitter.

Before (2026-11-12 audit): each weapon socket owned an attached Niagara component that was restarted
(`ReinitializeSystem`) on every round, which wiped the previous round's particles. The smoke emitter of the burst
system had no renderer, the rifles used the Puma armoured-car MG flash (0.2-0.25 s cone), and the only thing that
lingered was five orange sparks per shot.

| Part | Where |
|---|---|
| Batcher | `Weapons/SmallArmsProjectileManager/MuzzleVfx/SmallArmsMuzzleVfxBatcher.h/.cpp` (`USmallArmsMuzzleVfxBatcher`, a component on the projectile manager) |
| Weapon side | `UWeaponState::TryDrawBatchedSmallArmsMuzzle` / `InitSmallArmsMuzzle` in `WeaponData.cpp`, called from `CreateLaunchAndSmokeVfx` |
| Muzzle classes | `SmallArmsMuzzleClass.h/.cpp`: weapon name → Pistol / SubmachineGun / AssaultRifle / Rifle / MachineGun / AntiTankRifle |
| Project settings | Project Settings → Game → **RTS Weapon VFX** → Small Arms Muzzle VFX (system, on/off, per-frame cap, launch effects to replace, profile per class) |
| Niagara system | `/Game/RTS_Survival/ImpactSfxVfx/MuzzleVfx/NS_SmallArmsMuzzle` (emitters `Flash_Cone`, `Flash_Star`, `Flash_Glow`, `Smoke_Puff`) |
| Materials | `M_SmallArmsMuzzleFlash` (additive, `T_Muzzle_1` × particle colour × alpha), `M_SmallArmsMuzzleGlow` (additive radial glow), `M_SmallArmsMuzzleSmoke` (lit translucent on the dust devils' 2x2 puff atlas: R = density × 1.8 × alpha, G darkens, depth fade 40) — all built by `muzzle_vfx_job.py` |
| Emitter HLSL | `Weapons/SmallArmsProjectileManager/MuzzleVfx/MuzzleHlsl/*.hlsl` |
| Authoring | `SmallArmsMuzzleNiagaraAuthoring.cpp`: editor commands `RTS.MuzzleVfx.BuildNiagaraSystem` (adds missing emitters, then patches) and `RTS.MuzzleVfx.PatchNiagaraSystem`; headless `Source/Tools/MuzzleVfx/build_muzzle_vfx.sh [build|patch]` |
| Bench | `Source/Tools/MuzzleVfx/run_fight_bench.sh <label> [args]` (the trace impact fight scenario, `-TraceImpactVfxMeasure`) |

## Which weapons use it

A weapon is drawn by the batcher when **both** hold (checked once in `InitWeaponState`):

1. its weapon name has a muzzle class (`SmallArmsMuzzleClass::GetMuzzleClassForWeapon`), and
2. its own launch effect is one of `SmallArmsLaunchEffectsToBatch` (by default the three systems the
   `BPF_ImpactHelpers` small-arms functions hand out: `NS_MuzzleFlash_SmallArmsRandomBurst`, `NS_Single_MuzzleFlash`,
   `NS_PumaMG_LaunchEffect`).

So vehicle MGs keep their own effect (their weapon names have no class) even though the Puma MG shares
`NS_PumaMG_LaunchEffect`, and a weapon given a special launch effect keeps it. No weapon Blueprint was changed. Non-shipping
builds log every small arm once as `RTS_MUZZLE_VFX configured weapon=... owner=... effect=... class=N batched=0|1`.

**Burst fire:** `SingleBurst` weapons (FG 42, PPSh-41, MG 34, Fedorov and others set to "Burst Fire") only run the launch path once, when the
burst starts, because their old effect was one burst system for the whole burst. Batched weapons hand every later
round of the burst to the batcher too (`DrawBatchedSmallArmsMuzzleForBurstRound`, called from `OnSingleBurst`), so a
5-round FG 42 burst shows 5 flashes. The launch sound and the burst's shell casings still play once per burst; weapons
that keep their own effect keep one effect per burst. `RandomBurst` weapons (MG 34, MP40, ...) already ran the launch
path on every round.

A behaviour that swaps a weapon's launch effect (`UBehWeaponOverwriteVFX`) makes the weapon decide again on its next
shot: a special effect is drawn by the weapon itself, and the standard one goes back to the batcher once restored.

The launch sound and shell casings are untouched. When the batcher is not ready (system missing, setting off,
`-SmallArmsMuzzleVfxLegacy`), weapons fall back to their old per-socket effect.

**Fog of war:** the batched effect is not attached to the weapon, so a weapon whose mesh is hidden, or whose owner or
any actor it is attached to is hidden (enemy squads outside vision), draws nothing at all.

## How it works

1. **Weapon:** every round calls `TryDrawBatchedSmallArmsMuzzle(direction, muzzle location)`. The profile of its class
   gives flash size, smoke size and smoke lifetime. Smoke is throttled per weapon by time
   (`MinSecondsBetweenSmokePuffs`): an MG burst flashes on every round but puffs about 8 times a second, so a long burst
   does not stack dozens of overlapping translucent puffs (overdraw is the real cost of smoke).
2. **Batcher:** gathers the shots of a frame in two groups (Flash, Smoke) and on its next tick (TG_PrePhysics) writes
   them into two `float4` user arrays (`ShotPositionAndScale`: xyz muzzle, w size; `ShotDirection`: xyz fire direction,
   w lifetime multiplier) plus `FlashOffset/Count`, `SmokeOffset/Count` and `<Emitter>_SpawnCount`
   (= shots × `<Emitter>_PerShot`). The frame after, it sets the counts back to zero and goes idle. Same contract as
   the trace impact batcher; at most `MaxSmallArmsMuzzleShotsPerFrame` shots a frame.
3. **Niagara:** each emitter is a CPU emitter in world space with a `SpawnPerFrame` module bound to its spawn count.
   The spawn HLSL gives each particle its shot (`TypeOffset + ExecIndex * TypeCount / ExecutionCount`), stores the
   muzzle origin, the fire axis and size, and random shape values; the update HLSL places the particle every frame
   from those, so nothing is integrated and nothing drifts.
   - **Flash_Cone:** the forward tongue (mesh `SM_Muzzle_1`, orange), 45-75 ms, own length, width and roll per shot.
   - **Flash_Star:** side petals (mesh `SM_Muzzle_Part2`), 35-60 ms, dark on about a third of the shots.
   - **Flash_Glow:** soft orange camera-facing glow 14 cm in front of the barrel (30-42 cm × size), 50-85 ms; what
     reads from the RTS camera.
   - Every flash lives at least 1.5 × the frame time, so on a slow frame it is still drawn once.
   - **Smoke_Puff:** two lit grey puffs per smoke shot, blown 30-150 cm out of the barrel and slowed within a fraction
     of a second, then growing (28 → 125 cm × size), rising and drifting with a light wind for 1.3-2.4 s × lifetime.
     They stay where they were fired when the soldier moves on.

## Tuning

- Sizes and smoke per class: project settings (`SmallArmsMuzzleProfiles`), no rebuild or re-patch.
- Look (colours, lifetimes, motion): edit `MuzzleHlsl/*.hlsl`, then unload the system in any open editor and run
  `Source/Tools/MuzzleVfx/build_muzzle_vfx.sh patch` (headless DebugGame; `BIN=Development` for the Development DLL).
  The log prints a `compiled ... status=3` line per script (3 = up to date, 2 = error with the message).
- The CPU VM has **no unsigned integer maths, no `smoothstep` and no `step`**: random numbers come from `rand(1.0)`,
  fades are written out as `saturate` + `x*x*(3-2x)`.
- A new layer: add an entry to `Layers[]` in `SmallArmsMuzzleNiagaraAuthoring.cpp` (emitter name `Flash_<X>` or
  `Smoke_<X>`, HLSL file, particles per shot, mesh or sprite, material) plus its `.hlsl`, rebuild, run `build`.

## Classes and the old per-weapon multipliers

| Class | Weapons | Flash | Smoke | Smoke life | Puff at most every |
|---|---|---|---|---|---|
| Pistol | M1895 Nagant | 0.5 | 0.45 | 0.75 | every shot |
| SubmachineGun | MP40, PPSh-41, PPS-43, Korovin | 0.6 | 0.55 | 0.85 | 0.14 s |
| AssaultRifle | StG 44, FG 42, Fedorov | 0.8 | 0.75 | 0.95 | 0.14 s |
| Rifle | Kar98k (+sniper), Mauser, Mosin (+sniper), SKS, SVT-40 | 1.0 | 1.0 | 1.0 | every shot |
| MachineGun | MG 34, MP46, DP-28, DS-39, Ripper Gun | 1.0 | 0.85 | 1.15 | 0.12 s |
| AntiTankRifle | PTRS (all), M920 | 1.9 | 1.9 | 1.4 | every shot |

The rifle blueprints still pass lifetime/size multipliers to `GetSmallArmsSingleShotLaunchVfxSettings` (snipers 2.0 size,
PTRS 3.0, SVT 1.4, Nagant 0.8); the batched path ignores them and uses the class profile. Snipers fire the same
cartridge as the rifle, so they get the rifle's flash; give them their own class if a bigger sniper flash is wanted.

## Cost (2026-11-12, DebugGame `-game`, RTS_ToCopySmall, 14 squads a side, ~125 soldiers firing, 900 frames)

`run_fight_bench.sh perf_batched` against `run_fight_bench.sh perf_legacy -SmallArmsMuzzleVfxLegacy`, two runs each
(every run had the same 43-44 DebugGame error popups from an unrelated Blueprint, so the runs compare):

| | Batched | Old per-weapon components |
|---|---|---|
| p50 frame | 11.0 / 11.1 ms | 12.2 / 12.3 ms |
| p95 frame | 13.8 / 17.0 ms | 15.1 / 16.4 ms |
| Active Niagara components | 216 / 218 | 321 / 323 |
| Draw calls | 1990 / 1998 | 2222 / 2257 |

About 1.2 ms less per frame at the median, ~105 fewer active Niagara components and ~240 fewer draw calls in a
28-squad fight. The muzzle system itself has ~120-200 smoke puffs alive in that fight (four CPU emitters).

## Bench and pictures

- `Source/Tools/MuzzleVfx/run_fight_bench.sh <label> [args]` runs the trace impact fight scenario
  (`-TraceImpactVfxMeasure`) in a DebugGame game window and prints `RTS_MUZZLE_VFX bench mode=... shots=...` and the
  frame/Niagara/draw-call line, plus `RTS_MUZZLE_VFX rounds vs flashes:` per weapon and fire mode (rounds fired by
  batched weapons against flashes handed to the batcher; equal unless a weapon is hidden or the per-frame cap is hit).
  `MAP=` picks the map (default UnitTests/RTS_ToCopySmall).
- `-SmallArmsMuzzleShowcase` fires one muzzle per class in a row in front of the camera every 2 s (bursts for the
  automatic classes) and saves a picture while the flash is up and one ~0.7 s later; `-SmallArmsMuzzleShowcaseLegacy`
  fires the old effect of each class one step behind; `-SmallArmsMuzzleShowcaseRts` looks from the game camera's
  height. Use `MAP=/Game/RTS_Survival/Maps/UnitTests/Ger_Trains -TraceImpactVfxSquads=0` (daylight; without squads there
  are no error popups, which otherwise push frames to ~150 ms). Each picture logs the first particle of every emitter
  (`RTS_MUZZLE_VFX particles ...`).
