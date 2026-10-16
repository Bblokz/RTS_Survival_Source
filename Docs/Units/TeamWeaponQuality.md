# Team weapon quality control

Status: 2026-10-16. Covers ground contact, crew post keeping while pushed, and the towed gun's sway.
Code: `RTS_Survival/Units/TeamWeapons/` (`TeamWeapon`, `TeamWeaponController`, `TeamWeaponMover`,
`CrewFormation/`, `TowSway/`), test in `Units/TeamWeapons/Tests/TeamWeaponQualityTestSubsystem`.

## The test (`-TeamWeaponQualityValidate`)

One unattended run checks every system touched here. It spawns five team weapon squads through the async spawner and
two trucks with a gun in tow through `AMissionManager::SpawnTowedTeamWeapon`, then:

| Subject | Steps | What is measured |
|---|---|---|
| Squads (PaK 40, ZiS-3, 80 mm and 120 mm mortar, leFH 18) | deploy at spawn, push out along a lane (starts with a 135° turn), push back (U-turn) | weapon origin and footprint corners against the ground, operators to their crew position, operators/guards walking in front of the gun, arrival |
| Towed guns (SdKfz 250 + PaK 38, SdKfz 250 + 53-K) | drive an S-curve, stop, detach, deploy, push 15 m, re-tow, drive | yaw/pitch/roll of the gun relative to the truck hull, rates, hitch drift, wheel height over the ground, then the deployed and pushed checks above |

Lanes are picked automatically: from the test centre the test searches the most hilly walkable area
(`-TWQualitySearchCm`, default 400 m) and per subject the lane with the most height change.

```
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject <map> -game -unattended -nosound -RenderOffscreen -windowed
  -ResX=1280 -ResY=720 -DisablePlugins=Fab -benchmark -fps=30 "-ExecCmds=r.TextureStreaming 0"
  -TeamWeaponQualityValidate [-TWQualityAt=X,Y] [-TWQualityDir=<pictures>] [-TWQualitySearchCm=<radius, 0 = off>]
  [-TWQualitySquads=Squad_Ger_PaK40,...] [-TWQualityTows=Tank_Sdkfz250:Squad_Ger_PaK38,...] [-TWQualityLegCm=3600]
  -abslog=<log>
```

Grep the log for `RTS_TWQC`: `metric` lines carry min/mean/p90/p95/max, `check=PASS|FAIL` lines the verdicts,
`tow_status` every change of the towed gun's state and links, `stall` the mover state when a pushed gun stands still,
and one `RESULT` line at the end. `Source/Tools/TeamWeaponQuality/run_twqc.sh <label> <map> [args]` runs it and writes a summary.

Notes:
- `RTS_ToCopySmall` needs `-TWQualityAt=16796,-15808`; its player start has no ground under it. Without a usable
  start the test falls back to a random navmesh point after 15 s.
- `RTS_ToCopySmall` and `UT_Nomadics` are flat (under 60 cm relief over 140 m); slopes need another map.
- Only subtypes with an async spawner training option can be used: the GrW 42 and the Kettenkrad have none.
- Unseen units are kept unthrottled every frame (optimizer off, movement and pose ticking), otherwise their slower
  ticks read as crew lagging behind.

## What was wrong (baseline run on unchanged code)

| Area | Finding | Evidence |
|---|---|---|
| Crew on the gun | The crew snap traced the landscape channel, which the gun mesh blocks: operators were teleported onto the trails/shield | operator feet 85 cm (PaK 40), 89 cm (ZiS-3), 216 cm (leFH) above the ground |
| Crew leading the gun | Pushed moves gave every soldier his own path offset from the crew centre in world space; on a turn the "behind" offset ends up in front | ZiS-3 operators in front of the gun 47 % of the time, leFH 100 %, operators 2.7–4 m off their posts |
| Guards | Guards walked their own paths and could stay 30–37 m behind; after deploying they stood on the gun or between the crew | PaK 40 guards 31 m from their gun after a leg |
| Towed gun in the air | Attach offset 150 cm at hook height: the gun hung 66 cm above the ground, its trail inside the truck | wheel gap 66 cm for the whole drive |
| Towed gun stiff | Rigid attach: no reaction to turns, ground or acceleration | pitch/roll 0.0° over the whole drive |
| Deploy finishing on the truck | A gun hooked while deploying kept its deploy timer; it went `Ready_Deployed` on the truck, ordered its crew out of the cargo and broke the tow link | state `Ready_Deployed` while attached, detach refused |
| Detach refused | A mission tow in the truck's spawn frame swapped Tow→Detach before the truck had added its Tow ability | `DetachTow` returned `AbilityNotAllowed` |
| Pose on the truck | A deploy montage still playing when the gun was hooked ended later and put the deployed pose back | trails spread while towed |
| Turning while pushed | An in-place turn still running when a move started turned the gun against the mover; the gun stalled in `Moving` | yaw drifting 0.05°/s against a 20°/s turn, leg never finished |

## What changed

- **Ground contact** (`ATeamWeapon::ConformToGround`): the origin goes onto the ground and the gun tilts to a plane
  fitted through four footprint corners (derived from the mesh bounds, or `M_GroundConformSettings`). Gameplay keeps
  reading only the yaw; the controller copy stays yaw-only. Called at spawn, deploy start, every pushed step, every
  in-place turn step and after detaching. The ground trace (`TryGetGroundHeightAt`) steps through units and the gun.
- **Crew snap**: uses the same trace, so it can no longer land on the gun; soldiers are snapped upright.
- **Pushed crew formation** (`FTeamWeaponPushedCrewFormation`): operators keep their crew positions, guards an arc
  behind the gun, by direct velocity requests each frame instead of paths. The gun waits for operators that fall
  behind (`WaitForCrewDistanceCm`), creeps on after `MaxWaitForCrewSeconds`, and a soldier stuck far off his post
  is put back on it. Large heading errors are turned on the spot (`MaxTurnWhileMovingDegrees`). Soldiers that get
  another order (evasion, cargo) are left alone until it ends. Tuning: `M_PushedCrewSettings` on the weapon.
- **Guards** are moved off crew positions (and back near the gun) every 1.5 s while deployed; an armed operator
  bumped off his post is snapped back.
- **Tow sway** (`FTeamWeaponTowSwaySimulation`, visual only): the gun pivots about the hook in the truck hull's frame,
  trails into turns like a trailer (follow-the-leader axle), is pitched and rolled onto the ground under its wheels
  with damped springs, nods on acceleration and leans in turns. Tyres may press in by `TyreCompressionCm` and hop up
  a little, never sink. With `bPlaceTrailEndOnHook` a short attach offset is lengthened to the trail's reach so the
  trail end sits on the hook. Detaching keeps the swung position, upright and on the ground. Tuning:
  `M_TowSwaySettings` on the weapon.
- **Tow robustness**: hooking cancels a pending deploy/pack timer, post actions, turns and pushes; deploy/pack timers
  ignore a towed gun; the truck's deferred ability setup starts on Detach when it already tows; hooking stops pack and
  deploy montages without their end callbacks.

## Results (2026-10-16, DebugGame, `-RenderOffscreen`)

| Map | Run | Checks |
|---|---|---|
| `UnitTests/RTS_ToCopySmall` (flat), `-TWQualityAt=16796,-15808` | baseline, unchanged code, first test version | 120 pass / 37 fail (2 subjects could not spawn; fewer checks than now) |
| same, `-TWQualitySearchCm=0` | final | **267 pass / 0 fail** |
| `GerMissions/Ger_FirstMission` (hills: 10.7 m relief, lanes up to 24°) | final | 265 pass / 2 fail (near the thresholds, below) |

The baseline ran before the hill search existed, so there is no slope baseline; on flat ground the old code already
kept the gun origin on the ground. The slope rows below are final-only.

| Metric | Baseline (flat) | Final flat | Final hills |
|---|---|---|---|
| Operator feet above ground when deployed | 85–216 cm (on the gun) | ≤ 2.2 cm | ≤ 7.6 cm |
| Operator distance to his post while pushed, p90 | 270–405 cm | 30–35 cm | 32–35 cm |
| Operators in front of the moving gun | ZiS-3 47 %, leFH 100 % | 0 % | 0 % |
| Guards in front of the moving gun | up to 97 % | 0 % | 0 % |
| Pushed gun origin over the ground, p95 | 0 | ≤ 0.1 cm | ≤ 3 cm, tilted up to 27° |
| Towed gun wheels over the ground | 66 cm the whole drive | p90 ≈ 10 cm | p90 ≈ 9 cm |
| Towed gun swing / pitch / roll vs. truck | 0 / 0 / 0 (rigid) | ±25–40° yaw, ±5–10° pitch/roll | up to ±60° yaw, pitch/roll follow the slope |
| Detach after a mission tow | refused (`AbilityNotAllowed`) | works | works |
| Gun packs/turns with no order while deployed | (not measured) | never | never |

Remaining hills misses: the towed PaK 38's wheels sink below the ground in 10 % of the samples by more than 15 cm
(p10 −16 cm), and the 80 mm mortar stopped 5.2 m from its lane end (4 m allowed). Pictures of the final runs are in
`Saved/TWQuality/final4_flat` and `final4_hills`; `TeamWeaponQuality_BeforeAfter.png` next to this file compares three of
them with the baseline.

## Open items

- Re-towing (`TowActor` on a gun 15 m away) did not hook in 3 of about 10 runs; the truck's approach (drive to
  the gun, retry while farther than `AcceptanceRadiusToTow`) never got close enough. Truck tow logic was not changed.
- The towed gun's pitch target is taken from the ground under the unpitched axle; on steep banks the pitched axle
  stands up to ~30 cm closer to the truck, so the wheels can sit 10–30 cm too low there. A second ground probe at the
  pitched axle would fix it.
- One-frame pops (a single sample of 80–90 cm wheel gap) were seen in a few drives, once as the truck set off.
- Mixed-faction subjects (German and Soviet) are enemies by their Blueprint owners; the test silences the subject
  guns. In normal play a gun turning to a target packs, turns and redeploys: that is the existing combat flow.

## Known content issues (not code)

- `Ger_TW_Pak38`: its packed pose keeps the trails spread; on the truck the trails stand apart.
- `Ger_ArmoredCar_Sdkfz250`: one `cargo` socket sits outside the hull; a riding crew member hovers beside the truck.
- Every team weapon uses the BP_TeamWeapon tow attach offset (150, 0, 0); the sway lengthens it to the trail, but a
  per-gun offset would place the hook exactly.
