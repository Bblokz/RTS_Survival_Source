# Gun pit fire feedback: the gun bucks, the crew brace

Every gun pit reacts to a shot with two procedural layers on top of its animations:
- the gun **kick**: the carriage jolts back, rocks about its spades and throws the muzzle up, then settles;
- the crew **brace**: every man hunches, ducks his head and turns it away from the blast.

Both are driven from C++ and need no extra animation assets. They layer over whatever already plays: the gun's recoil montage, the crew's idle or their loading drill. So the drill timing and the round in a man's hands are never disturbed.

It is wired into every pit except the DShK:
- the German Pak 40, leFH 18 and sFH 18;
- the Soviet 53-K and BS-3;
- the B-4 and ML-20;
- the Soviet Ampulomet pit;
- the German Puppchen pit and Panzerschreck foxhole.

The DShK keeps its own per-shot gunner shake; its crew do not brace (`CrewFireFlinchStrength` 0), because it fires in bursts.

## How it runs

`UGunPitCrewComponent::OnGunFired` (called from each turret's `PlaySingleFireAnimation`) plays the gun's fire montage as before. It then calls:

- `UGunPitAnimInstance::PlayFireKick()` on the gun.
  - It drives the impulse response of a damped spring (`FrequencyHz`, `DampingRatio`), scaled so its first swing peaks at 1.
  - From that it sets `FireKickTranslation`, `FireKickRotation` and a muzzle jump added to `CradlePitchRotation`.
- `UGunPitCrewAnimInstance::PlayFireFlinch(CrewFireFlinchStrength)` on every crew member.
  - The brace rises in 0.07 s, holds to 0.25 s and is gone by 0.95 s.
  - Each man gets a random 0–0.06 s delay and ±20 % strength, and turns his head to a random side.
  - The hunch tips the body over the direction he faces, read from his root bone, so the shell handler braces correctly mid-walk.

## Anim blueprint wiring (`Tools/GunPits/build_fire_feedback.py`)

**Gun anim blueprints.** One or more additive component-space Modify Bone nodes go between the elevation node and the root yaw node:

`Local To Component → elevation → kick bone(s) → root yaw → Component To Local`

The nodes read `FireKickRotation` and `FireKickTranslation`. The kick bones carry the whole gun but not the root bone, because the root bone's pit-origin socket holds the crew, crate and rounds, which must stay still:

| Pit | Kick bone(s) | Jolt cm | Rock ° | Pivot behind cm | Muzzle jump ° | Hz |
|---|---|---|---|---|---|---|
| Pak 40 | `B_Mantlet`, both wheels and both legs (they hang straight off the root, so no rock) | 6 | 0 | — | 2.0 | 5 |
| leFH 18 | `B_Base` | 5 | 0.8 | 260 | 2.0 | 4 |
| sFH 18 | `ROOT` | 6 | 0.9 | 320 | 2.5 | 3.5 |
| 53-K | `Wheels` | 5 | 1.0 | 220 | 2.0 | 5.5 |
| BS-3 | `Wheels` | 7 | 1.0 | 300 | 2.5 | 4 |
| B-4 | `B_Turret` (the crane, cart and rack on `B_Root` stay) | 3 | 0.4 | 60 | 1.5 | 3 |
| ML-20 | `B_Turret` | 4 | 0.6 | 60 | 2.0 | 3.5 |
| Ampulomet | `Gun` (tube, grips and sight; the tripod and crates on `Traverse` stay) | 2.5 | 0.5 | 50 | 1.2 | 7 |
| Puppchen | `Gun` (barrel, breech and grips; the carriage and crate stay) | 3 | 0.6 | 60 | 1.5 | 6 |
| Panzerschreck | `Gun` (the tube on the gunner's shoulder; a rocket hardly kicks) | 1.8 | 0.4 | 40 | 0.8 | 7 |

These numbers are the `FireKick` class defaults of each anim blueprint. `GunFrameSocket` names the pit-origin socket: `PitOrigin`, `PitOrigin53K` or `PitOriginBS3`. The kick's back direction and rock pivot are read from that socket in the reference pose, so guns whose root bone is turned or scaled (Pak, leFH, sFH) need no extra settings.

**Crew anim blueprint.** `AnimBp_B4Pit_Crew`, shared by every pit crew, gets this chain after its DefaultSlot:

`Local To Component → spine_01 → spine_03 → head → Component To Local`

The three nodes read `FlinchLowerSpineRotation`, `FlinchUpperSpineRotation` and `FlinchHeadRotation`.

The script is idempotent. It finds earlier kick nodes as the nodes between the elevation and yaw nodes, deletes them and rebuilds. It cannot save a package that another editor holds open, and reports that.

## Testing

The pit tests check the reaction after the first shot (`GunPit/Tests/GunPitFireReactionWatch`):

- `<pit>_gun_bucks_on_shot`: the kick bone moves at least 1.5 cm within 0.7 s.
- `<pit>_crew_brace_on_shot`: the head of an idle crewman (spotter or commander) moves at least 4 cm against his pelvis. Without the brace an idle head moves about 0.4 cm, so the check cannot pass by accident.
- In the B-4 scenario (`-B4HowitzerValidate`, `-B4HowitzerPit=ML20`) the whole crew starts the crane drill at the shot, so only the gun kick is checked (`pit_gun_bucks_on_shot`).
  - The watch starts with the gun's fire montage. The scenario counts a pit shot when the reload montage starts, 0.3 s later, by which time the kick has mostly settled.
- The Ampulomet pit has no idle crewman, but both men stand idle at the shot: the drill starts at the next fire attempt, after the 1 s cooldown. Its brace check watches both, so the gunner's own firing shake counts as well.

With `-GerPitCapture=<dir>` the scenario also takes pictures at the shot and 0.10 s and 0.25 s after it (`<pit>_shot_*`).

Last runs (2026-10-15, DebugGame) all passed:

| Pit | Gun kick bone moved (cm) | Idle crew head vs pelvis (cm) |
|---|---|---|
| Pak 40 | 5.7 | 11.6 |
| leFH 18 | 5.8 | 11.5 |
| sFH 18 | 7.7 | 11.1 |
| 53-K | 6.3 | 12.4 |
| BS-3 | 8.2 | 11.7 |
| B-4 | 3.4 | — |
| ML-20 | 4.7 | — |
| DShK | (no kick, no brace) | — |
| Ampulomet | 5.9 | 20.8 (gunner and loader; the gunner's firing shake adds to the brace) |
| Puppchen | 7.1 | 19.9 (gunner and loader, as for the Ampulomet) |
| Panzerschreck | 4.6 | 13.7 (gunner and loader) |
