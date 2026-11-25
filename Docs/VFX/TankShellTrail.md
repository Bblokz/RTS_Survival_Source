# Tank shell tracer and smoke trail

Every pooled tank shell in flight is drawn by **one Niagara system for the whole map**: a hot tracer streak, a soft
halo on the shell, and a smoke trail that stays in the air after the shell hit and went back to the pool. The same
idea as the small arms muzzle batcher ([SmallArmsMuzzleVfx.md](SmallArmsMuzzleVfx.md)).

Before (until 2026-11-25): each pooled projectile drew `NS_75mmShell` on its own Niagara component, a long
yellow-green ribbon coloured per shell type. A viewer called it "tennis balls"; the look below replaced it.
The shell's own component is stopped the moment the shell goes dormant, so a trail on it can never outlive the
shell. That is why the trail is owned by the manager.

| Part | Where |
|---|---|
| Batcher | `Weapons/SmallArmsProjectileManager/TankShellTrail/TankShellTrailBatcher.h/.cpp` (`UTankShellTrailBatcher`, a component on the projectile manager) |
| Projectile side | `AProjectile::SetupNiagaraWithPrjVfxSettings` decides per launch (`DecideIsDrawnByTankShellTrailBatcher`) and leaves the shell's own system off |
| Project settings | Project Settings → Game → **RTS Weapon VFX** → Tank Shell Trail VFX (system, on/off, per-frame sample cap) |
| Niagara system | `/Game/RTS_Survival/ImpactSfxVfx/TankShellTrail/NS_TankShellTrail` (emitters `Smoke_Trail`, `Tracer_Core`, `Glow_Head`); the folder is in `DirectoriesToAlwaysCook` |
| Materials | `M_SmallArmsMuzzleGlow` (tracer, halo) and `M_SmallArmsMuzzleSmoke` (smoke), shared with the muzzle system |
| Emitter HLSL | `Weapons/SmallArmsProjectileManager/TankShellTrail/TankShellHlsl/*.hlsl`; all values of the look are in `Style.hlsl` |
| Authoring | `SmallArmsMuzzleNiagaraAuthoring.cpp`: editor command `RTS.TankShellTrail.BuildNiagaraSystem`; headless `Source/Tools/TankShellTrail/build_tank_shell_trail.sh` (creates the asset when missing, then patches and saves) |

## Which shells use it

A launch is drawn by the batcher when all hold:

1. the weapon's projectile effect type is `EProjectileNiagaraSystem::TankShell` (the default of
   `FInitWeaponStateProjectile::ProjectileSystem`),
2. the weapon state was not created by a soldier's weapon (`AInfantryWeaponMaster`): infantry projectiles keep their own effect,
3. the batcher is ready (setting on, system loaded).

So tank guns, AT guns, hull guns, howitzers and barrage shells of that type get the look. AA shells, bazookas,
attached rockets, the railgun and small-arms tracers are untouched. When the batcher is not ready, every shell
falls back to `NS_75mmShell` on its own component.

**Fog of war:** same as before the change. Neither the old shell effect nor this one is hidden for shells fired by
units outside vision; nothing in the projectile code culls by visibility.

## How it works

1. **Batcher** (tick group PrePhysics): walks the tank projectile pool. For every active shell it remembers where
   the shell was last frame and lays evenly spaced samples on the segment flown since: tracer samples every 7 cm,
   smoke samples every 30 cm (both for a 75 mm shell, scaled by calibre, never below 0.6×), plus one halo sample at
   the shell. A new launch (the launch serial changed) starts at the muzzle. The spacing carry is kept per shell so
   samples stay even across frames.
2. **Arrays:** samples go into two `float4` user arrays. `ShotPositionAndScale`: xyz sample, w = shell kind × 10 +
   calibre scale (calibre / 75, clamped 0.35–2.6). `ShotDirection`: xyz flight direction, w = seconds since the
   shell passed the sample, so particles born in one frame fade as an even streak.
3. **Niagara:** each emitter spawns `<Emitter>_PerShot` particles per sample of its group and places them in world
   space. A particle never moves with the shell; the streak is the line of fading particles
   (length = flight speed × `TracerLife`).

Caps: 96 samples per shell per frame, `MaxTankShellTrailSamplesPerFrame` (6000) for all shells together.

## Changing the look

Edit `TankShellHlsl/Style.hlsl` (or a layer file), then run `Source/Tools/TankShellTrail/build_tank_shell_trail.sh`
with no DebugGame editor open. No C++ build is needed for value changes.

Known limits of the current look: the tracer head reads warm white-orange for every shell type; shell-type colour
was tried (see below) and hardly showed, because additive sprites this small clip to white.

## The looks that were not picked (2026-11-25)

Five looks were built and recorded side by side; look 3 "Frontlinie" became the standard. To bring one back, copy
its column into `Style.hlsl`, set the smoke spacing in `TankShellTrailBatcher.cpp` and rebuild the system.

| Value | 1 Leuchtspur | 2 Rauchfaden | **3 Frontlinie** | 4 Ostfront | 5 Schweres Kaliber |
|---|---|---|---|---|---|
| Idea | thin bright tracer, short wisp | same tracer, thin grey thread | hot core, real trail | dim tracer, dense trail | long tracer, sparks, rolling smoke |
| Smoke spacing (cm, C++) | 22 | 26 | 30 | 34 | 40 |
| TracerLife | 0.055 | 0.055 | 0.07 | 0.045 | 0.08 |
| TracerWidth | 28.8 | 26.4 | 24.0 | 19.2 | 26.4 |
| TracerTailWidth | 0.35 | 0.35 | 0.3 | 0.3 | 0.25 |
| TracerBright | 2.5 | 2.5 | 3.0 | 2.0 | 3.25 |
| TracerFadePow | 1.4 | 1.4 | 1.6 | 1.6 | 1.5 |
| TracerKindTint | 0.5 | 0.25 | 0 | 0 | 0 |
| TracerHot | 1.0, 0.93, 0.72 | 1.0, 0.93, 0.72 | 1.0, 0.96, 0.85 | 1.0, 0.9, 0.7 | 1.0, 0.85, 0.6 |
| TracerCool | 1.0, 0.45, 0.1 | 1.0, 0.45, 0.1 | 1.0, 0.4, 0.08 | 1.0, 0.35, 0.06 | 1.0, 0.22, 0.04 |
| GlowSize | 74.8 | 66.0 | 61.6 | 44.0 | 79.2 |
| GlowBright | 1.32 | 1.2 | 1.2 | 0.9 | 1.56 |
| GlowColor | 1.0, 0.7, 0.3 | 1.0, 0.7, 0.3 | 1.0, 0.72, 0.35 | 1.0, 0.65, 0.3 | 1.0, 0.5, 0.18 |
| SparkChance | 0 | 0 | 0 | 0 | 0.4 |
| SmokeLife (s) | 0.7 | 1.25 | 1.8 | 2.6 | 3.6 |
| SmokeHeMlt | 1.2 | 1.25 | 1.35 | 1.5 | 1.6 |
| SmokeGrowRate | 3.0 | 2.4 | 1.9 | 1.5 | 1.2 |
| SmokeStartSize | 19.8 | 24.2 | 28.6 | 33.0 | 39.6 |
| SmokeEndSize | 54.4 | 85.0 | 115.6 | 161.5 | 221.0 |
| SmokeSpread | 4.0 | 6.0 | 8.0 | 10.0 | 14.0 |
| SmokeWobble | 0 | 2.0 | 6.0 | 10.0 | 14.0 |
| SmokeRise | 6.0 | 9.0 | 14.0 | 18.0 | 22.0 |
| SmokeWind | 0.5 | 0.7 | 1.0 | 1.3 | 1.6 |
| SmokeFadeIn | 0.11 | 0.13 | 0.15 | 0.11 | 0.11 |
| SmokeFadePow | 1.2 | 1.2 | 1.3 | 1.4 | 1.5 |
| SmokeAlpha | 0.42 | 0.52 | 0.5 | 0.62 | 0.72 |
| SmokeHotAmount | 0 | 0.2 | 0.45 | 0.8 | 1.0 |
| SmokeColor | 0.62, 0.61, 0.58 | 0.6, 0.59, 0.56 | 0.58, 0.57, 0.54 | 0.57, 0.56, 0.53 | 0.56, 0.55, 0.52 |
| SmokeHotColor | 0.3, 0.28, 0.26 | 0.3, 0.28, 0.26 | 0.26, 0.24, 0.22 | 0.2, 0.19, 0.18 | 0.17, 0.16, 0.15 |

Two parts of the prototypes were removed from the code because look 3 does not use them.

**Shell-type tint** (looks 1 and 2; `TracerKindTint` = how much of it). At the end of `ReadShot` in `Common.hlsl`:

```hlsl
float3 KindTint = float3(1.0, 0.85, 0.45);
KindTint = Kind > 0.5 ? float3(1.0, 0.5, 0.15) : KindTint;   // HE
KindTint = Kind > 1.5 ? float3(1.0, 0.3, 0.12) : KindTint;   // HEAT
KindTint = Kind > 2.5 ? float3(0.75, 0.9, 1.0) : KindTint;   // APCR
KindTint = Kind > 3.5 ? float3(1.0, 0.4, 0.05) : KindTint;   // fire
```

and in the tracer and halo Place sections: `Color = lerp(Color, Color * KindTint * 1.4, TracerKindTint);`

**Sparks** (look 5): a fourth layer `{"Glow_Spark", "Glow", "Spark.hlsl", 1, nullptr, GlowMaterialPath, 1}` in the
authoring layer table, with this file:

```hlsl
// @section Shape
LifeFull = R0 < SparkChance ? lerp(0.25, 0.6, R3) : 0.0;

// @section Place
float2 SparkSide = (float2(Rx, Ry) - 0.5) * 2.0;
float3 SparkDrift = (Right * SparkSide.x + Up * SparkSide.y) * (55.0 * S) + Fwd * (140.0 * Rz);
float3 SparkFall = float3(0.0, 0.0, -260.0 * AgeSeconds * AgeSeconds);
Particles.Position = OriginValue + SparkDrift * (1.0 - exp(-AgeSeconds * 4.0)) + SparkFall;
float SparkSize = ShapeValue.w > 0.01 ? lerp(5.0, 9.0, Rz) * S * (1.0 - T * 0.6) : 0.0;
Particles.SpriteSize = float2(SparkSize, SparkSize);
Particles.SpriteRotation = 0.0;
Particles.Color = float4(float3(1.6, 0.55, 0.1) * 2.0, (1.0 - T) * (1.0 - T));
```

**How they were compared:** six GameMechanicsRecs takes (today's shell + five looks) of one battle with a fixed
seed, so the same timestamp shows the same shot in every take. Map Desert, anchor (-12000, -2000, 0) yaw 30, dust
sky, 27 s. German line (4 Pz IV H, 2 Tiger, 1 Brummbär) against a Soviet line (3 KV-1, 2 T-34/85, 1 KV-2), 70 m
apart, both on attack-move. Three 9 s shots: game view (pitch 52, 76 m out), side-on low (pitch 13), down range
over the German line (pitch 17). The takes and the side-by-side mosaic are in
`Saved/GameMechanicsRecs/TankShellTrail_Temp` (not in git).

Findings worth keeping: true-to-scale tracers are about one pixel wide from the RTS camera (widths ×2.4); additive
sprites clip to white fast (brightness was halved); smoke needs a fade-in of about 0.1–0.15 s or it wraps the shell
head; smoke shorter than about 0.7 s does not read at all.
