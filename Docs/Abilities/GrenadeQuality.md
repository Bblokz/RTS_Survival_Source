# Grenade quality pass (2026-10-16)

Squad grenades looked like they hit but did little: AT grenades exploded on tanks and nothing happened, and
anti-infantry grenades hurt one or two soldiers. A benchmark (below) measured every grenade before and after the
fixes.

## What was wrong

| Problem | Effect |
|---|---|
| The AOE falloff measured from the epicenter to the victim's **actor origin** (`FRTS_AOE::DealDamageVsRearArmorInRadiusAsync`). | A tank's origin is its hull centre, 1.5-3 m from a grenade that lands against its side or rear. With the AT grenade's 125 cm radius every tank took **0 damage**, even with the grenade touching the hull. |
| The sweep returns one hit per colliding component. | A tank (hull + tracks) was damaged once per component. |
| `SK_Ger_HeavilyArmored_Skeleton` had no `grenade` socket. | The Faust squad (Steel Fist assault squad) cancelled **every** throw ("Grenade socket missing"). |
| Falloff exponent 1.5 on all anti-infantry grenades. | A soldier 1.5 m from a stick grenade took about 20 of 125 damage. |
| Grenades exploded the moment they landed. | Nobody could react; no time to see them land. |

## What changed

- **Hull distance.** `EAoeFalloffDistance::ClosestCollision` (opt-in, trailing parameter): distance to the closest
  point of the hit collision (`GetClosestPointOnCollision`, the bounds box for complex-only meshes such as the KV-1)
  and one damage application per actor. Grenades and molotov burns use it; artillery, bombs and every other caller keep
  `ActorOrigin`. `FRTS_AOE::GetFalloffDistance` / `GetRearArmorDamageMultiplier` expose the same numbers to tests.
- **Fuse after landing.** `FGrenadeComponentSettings::FuseSecondsAfterLanding`: the grenade lies (or sticks) this long
  and then explodes where it is now. 0 keeps the old instant explosion (molotov). Grenades that land where a vehicle
  drove away settle onto the ground.
- **Sticky grenades.** `FGrenadeComponentSettings::bStickToVehicles` (off by default): the grenade attaches to the
  closest armored enemy vehicle hull within 150 cm of its landing spot and explodes on it. A sticky grenade ordered at
  a point within 300 cm of an enemy vehicle follows that vehicle during its flight, as long as the vehicle stays within
  2x the throw range at release. A tank ordered at from out of range is chased to where it was.
- **Grenade lifecycle.** `EGrenadeActorState` (Idle, InHand, Flying, Fusing, Burning). Grenades still flying, fusing
  or burning when their squad dies finish and then destroy themselves (the pool used to be destroyed in
  `BeginDestroy`, now in `EndPlay`).
- **Missing socket fallback.** A soldier mesh without the `grenade` socket holds the grenade at its right hand bone and
  reports the content gap once per mesh, instead of cancelling the throw.
- **Events for tests.** `AGrenadeActor::OnGrenadeLanded` / `OnGrenadeExploded`.

Assets changed: the four squad Blueprints below, a `grenade` socket on `SK_Ger_HeavilyArmored_Skeleton` (copied from
`SK_GerArmored_Skeleton`: `hand_r`, same offset), and the grenade card texts in `Buttons_DataTableb` (keys 1-3 of
`IdThrowGrenade`, which also mention the fuse and sticking now). Tools: `Source/Tools/GrenadeBench`
(`run_bench.sh`, `apply_grenade_tuning.py`, `update_grenade_card_texts.py`).

## Tuned settings

| Squad (component) | Radius | Damage | Exponent | Fuse | Sticks |
|---|---|---|---|---|---|
| Faust, Hazmat sniper team (HEATGrenade, GerAtGrenade) | 125 | 300 | 1.5 | 1.0 s | yes |
| Jager trupp (StickGrenade, GerStickGrenade) | 250 (was 225) | 90 (was 125) | 1.0 (was 1.5) | 1.5 s | no |
| Light Bringers (SmallGrenades, SmallGrenade) | 200 (was 150) | 60 (was 75) | 1.0 (was 1.5) | 1.5 s | no |
| Mosin (MolotovGrenade, RusMolotov) | 200 | 30 + burn | 1.0 | 0 | no |

AT damage stays at the designed 300: with the hull distance a stuck grenade deals it in full, scaled by the rear armor
(full damage below 75 rear armor, none above 125). If a volley proves too strong in play, `Damage` is the one knob.

## Before and after (benchmark)

| Scenario | Before | After |
|---|---|---|
| 2 AT vs T-70 (rear 25) | 0 % | destroyed |
| 2 AT vs T-34/76 (rear 45) | 0 % | 83 % |
| 2 AT vs Panzer IV H (rear 50) | 0 % | 83 % |
| 2 AT vs KV-1E (rear 70) | 0 % | 43 % |
| 2 AT vs T-34 driving at 6 m/s | 0 % | 83 %, both stuck |
| 2 AT (Hazmat sniper team) vs T-34 | 0 % | 83 % |
| Player's 2 AT vs enemy T-34, standing / driving | not measured | 83 % / 83 %, all stuck |
| 3 stick grenades vs 5-man squad | 5 hurt, 0 killed, 297 damage | 4-5 hurt, 1-3 killed, 440-615 damage |
| Player's 3 stick grenades vs 6 Soviet riflemen | not measured | 6 hurt, 0-3 killed, 600-890 damage |
| 6 small grenades vs 5-man squad | 1 hurt, 1 killed | 3-5 hurt, 2-3 killed |
| 2 molotovs vs 5-man squad | 4 hurt, 1 killed | 2-4 hurt, 1-3 killed |

"After" is the range over four full runs; the last one ran without overrides on the saved Blueprints (all 16
scenarios pass). "Before" is the old code path; the Faust throws only happen at all thanks to the socket fix.

## The benchmark

`UGrenadeBenchmarkSubsystem` (`GrenadeComponent/Tests`, not in shipping), `-GrenadeBenchmark`:
16 scenarios, each on its own navmesh spot: AT grenades (Faust, Hazmat) against light, medium, heavy and a driving
tank and against infantry; stick, small and molotov against a German 5-man squad; anti-infantry grenades against
tanks for comparison; and the player's own Faust and Jager squads against an enemy T-34 (standing and driving) and
enemy Soviet riflemen. Every scenario logs fuse, stuck count, rear armor and multiplier, health dealt, soldiers in the
radius and hurt, and the distance source (`collision` / `bounds` / `origin`), and checks the tuned expectations.

```
Source/Tools/GrenadeBench/run_bench.sh <run name> [-GrenadeBenchOnly=AT_,Stick] [-GrenadeBenchOverride="GerStickGrenade:Damage=80,AoeRange=260;GerAtGrenade:FuseSecondsAfterLanding=0.5"]
```

Overrides set any `FGrenadeComponentSettings` property on the spawned squads, so tuning needs no Blueprint save.
`-GrenadeBenchInfantry=<class path>` swaps the infantry target. Output: `Saved/GrenadeBench/<run>/GrenadeBenchmark.csv`
plus pictures; the run takes about a minute on `RTS_ToCopySmall`.

How the scenarios are isolated (each of these produced wrong numbers first):

- Most scenarios have an enemy-owned thrower squad and player targets; the `Player*` scenarios the other way round. Soldiers keep their own Blueprint's owner and
  collision side; the grenade takes its victims from the squad's owner.
- Soviet soldiers collide as enemies by Blueprint (`Rus_Inf_Mosin`), so the infantry target is German
  (`Squad_Ger_Scavengers`).
- The enemy unit ability AI is switched off for the run, or idle thrower squads grenade the waiting targets.
- Target turrets and all rifles are held silent every frame; target soldiers are held in place.
- The aim point is the median soldier position; soldiers more than 10 m from it are left out.

## Not covered by the benchmark

- A tank destroyed while a grenade is stuck to it (UE detaches the grenade; it explodes where it is), and a squad
  dying with grenades in the air (they finish and destroy themselves). Both are handled in code, not measured.
- A sticky grenade ordered at a tank out of throw range: the squad walks to where the tank was and follows it only
  while it stays within twice the throw range at release.

## Found on the way (not changed)

- `Rus_Inf_Mosin` soldiers collide as enemies by Blueprint (the German campaign's enemy). `Rus_Squad_Mosin_Soviet`
  uses separate `_Soviet` soldier Blueprints, likely the player-side variant, so this is not a bug.

- `ASquadController::FindIdealSpawnLocation_GetNextGridPoint` has 8 grid points; squads with more soldiers (e.g.
  `Rus_Squad_LargeMixBasic`, 12) put the rest on the grid origin.
- `FRTS_AOE::StartAsyncSphereSweep` does nothing for radii below 85 cm; the benchmark fails a grenade below it.
