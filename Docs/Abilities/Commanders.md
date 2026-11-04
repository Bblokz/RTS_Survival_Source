# Commanders and global ability tiers

A commander decides the player's global abilities (later maybe also unlocked buildings). Commanders are **not
faction-locked**: every German faction can pick any of them. Portraits are rendered outside Unreal, see
`CommanderPortraits/README.md`.

## Slots and tiers

Every commander has exactly six abilities (`RTSCommanderConstants::AbilitySlotsPerCommander`), in slot order:

| Slot | Role | Tier |
|---|---|---|
| 1 | Intel / utility (recon flight, flare curtain) | I |
| 2 | Economy (harvester or resource drop) | I |
| 3 | Reinforcement drop (vehicles, infantry, support weapons) | II |
| 4 | Air strike | III |
| 5 | Signature: the commander's identity ability | III (IV for a super-weapon) |
| 6 | Super-heavy vehicle drop (needs the new heavy-lift plane) | IV |

The tier lives on the ability template (`UGlobalAbility::M_AbilityTier`, `EGlobalAbilityTier`), so an ability has
the same tier in every kit. Each mission sets the highest tier the player may use on its mission manager
(`AMissionManager::MaxPlayerGlobalAbilityTier`, default Tier IV). This is set per mission and is **independent of
the campaign's `EMissionTier`**, so two early missions can open different parts of a kit. Without a mission manager
(skirmish, survival) the whole kit is available.

Tier-capped abilities stay in the panel, greyed out, with the hover text "Not available in this operation". The
player ability manager treats the cap as an unmet requirement (`UGlobalAbilitiesManager::QueryTierAllowedForAbility`).
Enemy managers ignore the cap, and abilities fired by mission scripts (`AMissionManager::ExecuteGlobalAbility`) skip
the requirement checks, so the cap never blocks them.

The costs, building requirements and cooldowns on each template still balance an ability within a mission. As of
2026-11-03, every template's costs are empty and most cooldowns are the 5 s placeholder; filling them is part of the
data work below.

## The kits

Kits are set in `Config/DefaultGame.ini` (`[/Script/RTS_Survival.CommanderSettings]`).

| Commander (`ERTSCommander`) | 1 (I) | 2 (I) | 3 (II) | 4 (III) | 5 | 6 (IV) |
|---|---|---|---|---|---|---|
| Field Commander (`BalancedCommander`) | Recon flight | Pz IV harvester drop | Paratroopers | 3x Ju 87 | Barrage (III) | King Tiger 105 mm drop |
| Luftwaffe Liaison | Recon flight | Pz IV harvester drop | Paratroopers | 3x Ju 87 | 2x Ho 229 (III) | E-100 drop (large aircraft) |
| Barrage Marshal | Flare curtain | Radixite & metal supply drop | Pz IV spotter + Pz III drop | Single Ju 87 | Large barrage (III) | Sturmtiger drop |
| Radixite Herald | Recon flight | Radixite cache drop | Radixite Pz II + Marder drop | 2x Ho 229 | Radixite V-weapon (IV) | E-100 drop |
| Scrap Warlord | Flare curtain | Pz I harvester drop | Scavengers + support weapons drop | Me 410 strike | Air minefield (III) | Scrap JagdTiger (128 mm) drop |
| Rail Engineer | Recon flight | Radixite & metal supply drop | 2x Pz 38(t) Railgun drop | 2x Bf 110 strafing | Rail bombardment (III) | Tiger Rail drop |

Character of each kit:
- **Field Commander:** solid everywhere, peaks nowhere, cheapest Tier IV.
- **Luftwaffe Liaison:** air power and paratroopers.
- **Barrage Marshal:** artillery; strong against static defences, weaker against mobile armour.
- **Radixite Herald:** the late bloomer. No Tier III signature and two Tier IV abilities: weak in missions capped at
  Tier III, strongest at the end.
- **Scrap Warlord:** cheap and early; strongest early, weakest late. Its finale is a custom JagdTiger with the
  Maus's 128 mm KwK 44 instead of the stock laser (`Ger_Heavy/JagdTiger/Scrap128/Ger_Vehicle_JagdTiger_Scrap128`,
  untested in game).
- **Rail Engineer:** long-range kinetic anti-armour. Rail bombardment = a few high-penetration rail slugs from high
  altitude (a `GA_Barrage` template with rail weapon data). The Tiger Rail drop costs about the same as the King
  Tiger 105 mm drop.

## Proposed commanders (round 2, 2026-11-03)

Not in `ERTSCommander` or the config yet; balancing comes later. ✓ = unit/ability exists, *new* = needs work.

| Commander | 1 (I) | 2 (I) | 3 (II) | 4 (III) | 5 (III) | 6 (IV) |
|---|---|---|---|---|---|---|
| Light Bringer (laser) | Flare curtain ✓ | Power-cell drop *new* | 3x Panzerjäger I laser drop (unit ✓) | 2x Ho 229 ✓ (later a laser Ho 229) | Sonnengewehr beam sweep *new* | JagdTiger ✓ (the stock one fires the Sonnensturm 128L laser), Mammut drop *new template* |
| Feuersturm (flame) | Recon flight ✓ | Pz IV harvester drop ✓ | Feuer Sturm cyborg paradrop (squad ✓) | Incendiary Ju 87 run *new weapon setup* | Fire barrage with burning ground *new effect* | Heavy flame tank *new* (Jagdpanther flamer variant or a King Tiger flame), Mammut drop |
| Stahlläufer (mech walkers) | Recon flight ✓ | Radixite & metal supply drop *new class* | Red Spine light walker drop (✓) | Me 410 strike *new aircraft BP* | Fels Krieger medium walker drop *needs BP* | KR-12 Dachs heavy walker drop (✓) |
| Festungsbaumeister (fortress engineer) | Flare curtain ✓ | Construction supply drop (metal-heavy) *new class* | Sturm Pionieren + Pak team drop (✓) | Ju 390 carpet bombing ✓ | Air-dropped field bunker *new building-drop class* | Maus ✓, Mammut drop |
| Sturmkommandant (cyborg assault) | Recon flight ✓ | Pz IV harvester drop ✓ | Panzergrenadiere cyborg paradrop (squad ✓) | 3x Ju 87 ✓ | Sturm Kommandos + Iron Storm elite paradrop (squads ✓) | E-75 ✓, Mammut drop |

Character:
- **Light Bringer:** precise, energy-hungry, anti-armour at range. Cheap light lasers early, a weak economy slot,
  then the strongest beam weapon and the JagdTiger, whose stock weapon is the Sonnensturm 128L multi-hit laser.
  The Panther-hull laser tank (`Tank_PanzerV_III`, laser turret) is another laser heavy for later. Sonnengewehr follows the wartime
  orbital "sun gun" concept: a beam sweeps the strip the player draws (strip aim exists from the flight abilities).
- **Feuersturm:** anti-infantry and anti-bunker at short range; strong early and mid, weak against heavy armour at
  range.
- **Stahlläufer:** walkers instead of tanks at every tier; walkers are squad units, so the drops land them like
  infantry.
- **Festungsbaumeister:** the defensive commander: weakest attack, strongest in hold-the-line missions. Field
  bunkers to drop: Flakvierling bunker, rail Pak bunker.
- **Sturmkommandant:** all-infantry pressure with elite cyborg squads; weak against armour until the E-75.

New work this round needs: a power-cell (energy) drop, an incendiary strike setup, a fire barrage with lingering
fire, a building-drop ability class, a Fels Krieger walker (it has a skeletal mesh, `SK_Etasphera07`, but no
animations or unit Blueprint), and the Sonnengewehr beam ability. Built 2026-11-03: the Jagdpanther flamer
(`Ger_Heavy/JagdPanther/Flamer/Ger_Vehicle_JagdPanther_Flamer`, Pz III Flamm flamethrower, untested in game). The Me 410 strike needs no new Blueprint (`Ger_Aircraft_ME5_Airbrone` exists).

## Ability tiers

| Tier | Abilities |
|---|---|
| I | `GA_GerReconFlight`, `GA_GerFlareCurtain`, `GA_HarvesterDrop` (Pz IV harvester), `GA_GerPzIHarvesterDrop`, `GA_GerRadixiteMetalSupplyDrop`, `GA_GerRadixiteCacheDrop` |
| II | `GA_GerParatroopers`, `GA_GerSpotterPzIIIDrop`, `GA_GerRadixitePzIIMarderDrop`, `GA_ScavengersDrop`, `GA_GerPz38tRailgunDrop` |
| III | `GA_3xJu87Strafing`, `GA_Ju87Strafing`, `GA_2xHortentrafing`, `GA_Me410Airstrike`, `GA_2xBf110Strafing`, `GA_Barrage`, `GA_LargeBarrage`, `GA_GerAirMinefield`, `GA_GerRailBombardment` |
| IV | `GA_GerRadixiteNuke`, `GA_GerKingTiger105Drop`, `GA_GerE100Drop`, `GA_GerSturmtigerDrop`, `GA_GerJagdTigerDrop`, `GA_GerTigerRailDrop` |

## Cost and cooldown bands

- **Drops:** about 125% of the delivered units' training cost (no production time, lands anywhere), plus Fuel for
  the aircraft.
- **Strikes:** Fuel + Ammo, a fixed band per tier.
- **Intel:** a little Intel + Fuel.
- **Economy:** pays its cost back in about 3-4 minutes.
- **Cooldowns:** Tier I 60-90 s, Tier II 120-180 s, Tier III 180-240 s, Tier IV 300-420 s.
- **Super-heavy drops:** planned as one use per mission (needs a uses-per-mission field; not built yet).
- **Building requirements:** the building of the ability's unit line (Refinery for economy, Barracks or a vehicle
  factory for drops, HQ for intel).

## Status (2026-11-03)

Done in C++:
- `EGlobalAbilityTier`, `UGlobalAbility::M_AbilityTier`, `AMissionManager::MaxPlayerGlobalAbilityTier`, the cap in
  `UGlobalAbilitiesManager`, set by `ACPPController::SetupGlobalAbilityManager`.
- Commanders appended to `ERTSCommander`; new abilities appended to `EGlobalAbility`; six kits in `DefaultGame.ini`.
- Tests: the global ability area test checks every kit has six abilities and fits the panel; the strafe test checks
  the Luftwaffe Liaison kit.

Done in data (2026-11-03, `Source/Tools/Commanders/author_commander_abilities.py`, headless or in-editor,
idempotent; check with `verify_commander_abilities.py`):
- New templates copied from working ones: Pz IV and Pz I harvester drops, Pz 38(t) Railgun drop, scavenger drop
  (2 scavenger squads + GrW 42 mortar), single Ju 87, 2x Bf 110 (`Ger_Aircraft_BF110Airborne`), large barrage
  (40 shells, 2500 radius), rail bombardment (6 slugs, 600 radius, 400 pen, 900 damage), and the Mammut glider
  drops for Sturmtiger, JagdTiger, King Tiger 105 and Tiger Rail (copies of the E-100 `GA_GliderTankDrop`, each
  with its own tank Blueprint as override).
- Tier, costs and cooldowns on every kit ability, per the bands above. The radixite Pz II + Marder drop is costed
  from its contents (2 Pz II F + 1 Marder: 725 radixite, 225 vehicle parts).
- The existing Barrage got a real title ("Artillery Barrage").
- Script gotchas: `unreal.GlobalAbility` is the `UGlobalAbility` class, not the enum (take the enum type from the
  map keys); `dir()` doesn't list Unreal enum members; a soft class override can't be cleared from Python, only set.

Still to do:
- Templates needing new work: the radixite & metal supply drop and the radixite cache drop (a new resource-drop
  ability class). The Me 410 strike exists since 2026-11-03 (one `Ger_Aircraft_ME5_Airbrone`, the Me 410's
  airborne Blueprint).
- Building requirements on the templates (which building per ability, valid for every German faction).
- Set `MaxPlayerGlobalAbilityTier` on each mission's manager.
- Uses-per-mission for Tier IV drops.
- Commander select UI: built by a separate session (`FactionSystem/FactionSelection/CommanderSelection/`).
