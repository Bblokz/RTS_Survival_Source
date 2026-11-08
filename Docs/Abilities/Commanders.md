# Commanders and global ability tiers

A commander decides the player's global abilities (later maybe also unlocked buildings). Commanders are **not
faction-locked**: every German faction can pick any of them. Portraits are rendered outside Unreal, see
`CommanderPortraits/README.md`.

## Slots and tiers

Every commander has exactly seven abilities (`RTSCommanderConstants::AbilitySlotsPerCommander`, round 3,
2026-11-05), in slot order:

| Slot | Role | Tier |
|---|---|---|
| 1 | Intel / utility (recon flight, flare curtain, spies infiltration) | I |
| 2 | Small reinforcements (one squad, a light vehicle pair, a field gun or pit) | I |
| 3 | Economy (harvester or resource drop) | I |
| 4 | Reinforcements (vehicles, infantry, support weapons) | II |
| 5 | Air strike | III |
| 6 | Signature: the commander's identity ability | III (IV for a super-weapon) |
| 7 | Super-heavy vehicle drop (Mammut glider) | IV |

The slot tiers live in `RTSCommanderConstants::SlotMinTiers` / `SlotMaxTiers`; the area test checks every kit against
them. The tier itself lives on the ability template (`UGlobalAbility::M_AbilityTier`, `EGlobalAbilityTier`), so an
ability has the same tier in every kit. Each mission sets the highest tier the player may use on its mission manager
(`AMissionManager::PlayerGlobalAbilitySettings.MaxAllowedTier`, struct `FMissionPlayerGlobalAbilitySettings`, default
Tier IV). The same struct has `bDisableResourceCosts`: the player's abilities then cost no resources (cooldowns still
apply, the tooltip shows no cost). This is set per mission and is **independent of
the campaign's `EMissionTier`**, so two early missions can open different parts of a kit. Without a mission manager
(skirmish, survival) the whole kit is available.

Abilities above the cap are hidden: the player ability manager does not load them (`LoadGlobalAbilities`), so the
side panel collapses their slots and nothing can execute them (2026-11-07; before that they were greyed out).
`QueryTierAllowedForAbility` stays as a safety net.
Enemy managers ignore the cap, and abilities fired by mission scripts (`AMissionManager::ExecuteGlobalAbility`) skip
the requirement checks, so the cap never blocks them.

The costs, building requirements and cooldowns on each template still balance an ability within a mission.

The in-game panel (`W_BP_GlobalAbilityPanel`) has eight 96-unit buttons stacked on the left edge; a kit fills the
first seven and the eighth stays collapsed. The commander select screen lists the twelve commanders in 60-unit
entries (3 apart), so the whole list fits without a scroll bar, and shows the selected kit's seven abilities on the right.

## The kits

Kits are set in `Config/DefaultGame.ini` (`[/Script/RTS_Survival.CommanderSettings]`), written by
`Source/Tools/Commanders/write_commander_kits.py` (the kit table lives there; it re-reads the shared ini and replaces
only the `CommanderSettingsByType` line). `ERTSCommander` keeps its order: the round-3 commanders are appended.

| Commander (`ERTSCommander`) | 1 (I) | 2 (I) | 3 (I) | 4 (II) | 5 (III) | 6 (III/IV) | 7 (IV) |
|---|---|---|---|---|---|---|---|
| Field Commander (`BalancedCommander`) | Spies infiltration | Marder + scavenger drop | Pz IV harvester | 2x Vultures + 2x Panzergrenadiere paradrop | 3x Ju 87 | V2 rocket (IV) | E-50 |
| Luftwaffe Liaison | Recon flight | Bf 109 strafing | Pz IV harvester | 2x Vultures + 2x Panzergrenadiere paradrop | Luftlande-Sturm | 2x Ho 229 (III) | E-100 |
| Barrage Marshal | Flare curtain | Flak 88 airdrop | Radixite & metal supply | Howitzer battery airdrop | Single Ju 87 | Large barrage (III) | Sturmtiger |
| Radixite Herald | Recon flight | 2x Radixite Pz II | Radixite cache | Radixite Pz III + Radixite Wespe | 2x Ho 229 | Radixite V-weapon (IV) | E-100 |
| Scrap Warlord | Spies infiltration | 2x Elite Scavengers paradrop | Pz I harvester | 2x Elite Scavengers + 2x Pak 40 | Me 410 | Air minefield (III) | Scrap JagdTiger (128 mm) |
| Rail Engineer | Recon flight | Rail Troopers paradrop | Radixite & metal supply | 2x Pz 38(t) Railgun | 2x Bf 109 strafing | Rail bombardment (III) | Tiger Rail |
| Light Bringer (`LightBringer`) | Flare curtain | Light Bringers paradrop | Pz IV harvester | 3x Panzerjäger I laser | 2x Ho 229 | Sonnengewehr | JagdTiger (Sonnensturm laser) |
| Feuersturm (`Feuersturm`) | Recon flight | 2x Feuer Sturm paradrop | Pz IV harvester | 2x Pz III Flamm | Incendiary Ju 87 | Fire barrage | Jagdpanther flamer |
| Stahlläufer (`Stahllaeufer`) | Recon flight | Red Spine walker drop | Radixite & metal supply | 2x Red Spine walker drop | Me 410 | *placeholder:* Artillery Barrage (III) | Maus |
| Festungsbaumeister (`Festungsbaumeister`) | Flare curtain | Dual Panzerschreck pit drop | Construction supply | 2x Sturm Pioniere + Pak 40 pit | Ju 390 carpet bombing | Field bunker drop (III) | Maus |
| Sturmkommandant (`Sturmkommandant`) | Recon flight | Iron Storm paradrop | Pz IV harvester | 2x Panzergrenadiere + 2x Iron Storm paradrop | 3x Ju 87 | V2 rocket (IV) | E-75 |
| Sky-Drop Quartermaster (`SkyDropQuartermaster`) | Massive flare | Puma, landed | Vehicle parts supply | 2x Flak Luchs + StuG III, landed | Ju 87 G tank busters | Field workshop drop (III) | E-50 |

Template keys, slot by slot:

| Commander | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| Field Commander | `GA_GerSpiesInfiltration` | `GA_GerMarderScavengerDrop` | `GA_HarvesterDrop` | `GA_GerParatroopers` | `GA_3xJu87Strafing` | `GA_GerV2Rocket` | `GA_GerE50Drop` |
| Luftwaffe Liaison | `GA_GerReconFlight` | `GA_Bf109Strafing` | `GA_HarvesterDrop` | `GA_GerParatroopers` | `GA_GerLuftlandeSturm` | `GA_2xHortentrafing` | `GA_GerE100Drop` |
| Barrage Marshal | `GA_GerFlareCurtain` | `GA_GerFlak88Drop` | `GA_GerRadixiteMetalSupplyDrop` | `GA_GerSFH18Drop` | `GA_Ju87Strafing` | `GA_LargeBarrage` | `GA_GerSturmtigerDrop` |
| Radixite Herald | `GA_GerReconFlight` | `GA_GerRadixitePzIIDrop` | `GA_GerRadixiteCacheDrop` | `GA_GerRadixitePzIIIWespeDrop` | `GA_2xHortentrafing` | `GA_GerRadixiteNuke` | `GA_GerE100Drop` |
| Scrap Warlord | `GA_GerSpiesInfiltration` | `GA_GerEliteScavengersParadrop` | `GA_GerPzIHarvesterDrop` | `GA_GerEliteScavengersPakDrop` | `GA_Me410Airstrike` | `GA_GerAirMinefield` | `GA_GerJagdTigerDrop` |
| Rail Engineer | `GA_GerReconFlight` | `GA_GerRailTroopersParadrop` | `GA_GerRadixiteMetalSupplyDrop` | `GA_GerPz38tRailgunDrop` | `GA_2xBf110Strafing` | `GA_GerRailBombardment` | `GA_GerTigerRailDrop` |
| Light Bringer | `GA_GerFlareCurtain` | `GA_GerLightBringersParadrop` | `GA_HarvesterDrop` | `GA_GerPzJagerLaserDrop` | `GA_2xHortentrafing` | `GA_GerSonnengewehr` | `GA_GerJagdTigerLaserDrop` |
| Feuersturm | `GA_GerReconFlight` | `GA_GerFeuerSturmParadrop` | `GA_HarvesterDrop` | `GA_GerPzIIIFlammDrop` | `GA_GerIncendiaryJu87` | `GA_GerFireBarrage` | `GA_GerJagdPantherFlamerDrop` |
| Stahlläufer | `GA_GerReconFlight` | `GA_GerRedSpineDrop` | `GA_GerRadixiteMetalSupplyDrop` | `GA_GerRedSpinePairDrop` | `GA_Me410Airstrike` | `GA_Barrage` (placeholder) | `GA_GerMausDrop` |
| Festungsbaumeister | `GA_GerFlareCurtain` | `GA_GerPanzerschreckPitDrop` | `GA_GerConstructionSupplyDrop` | `GA_GerSturmPionierenPakPitDrop` | `GA_HeavyCarpet_Ju390B_250Gr` | `GA_GerFieldBunkerDrop` | `GA_GerMausDrop` |
| Sturmkommandant | `GA_GerReconFlight` | `GA_GerIronStormParadrop` | `GA_HarvesterDrop` | `GA_GerPzGrenIronStormParadrop` | `GA_3xJu87Strafing` | `GA_GerV2Rocket` | `GA_GerE75Drop` |
| Sky-Drop Quartermaster | `GA_GerMassiveFlare` | `GA_GerPumaDrop` | `GA_GerVehiclePartsSupplyDrop` | `GA_GerFlakLuchsStugDrop` | `GA_GerJu87GTankBusters` | `GA_GerFieldWorkshopDrop` | `GA_GerE50Drop` |

User decisions (2026-11-04): two Tier IVs on Field Commander and Sturmkommandant are fine; duplicate super-heavies
are fine; Sturmkommandant without elite cyborgs is fine; the Scrap Warlord's slots 2 and 4 may overlap. Abilities with
mixed delivery use several aircraft (composite drops). Buildings come down on a scaled-up parachute. Field guns must be
parachutable.

Character of each kit:
- **Field Commander:** solid everywhere, peaks nowhere; spies for intel, the V2 rocket as signature and the E-50 as
  its super-heavy (the King Tiger 105 mm drop is in no kit since 2026-11-04).
- **Luftwaffe Liaison:** air power and paratroopers; the Luftlande-Sturm is two Ju 87 dives followed by two Vulture
  squads under parachutes.
- **Barrage Marshal:** artillery; strong against static defences, weaker against mobile armour. Its guns come down
  under cargo chutes.
- **Radixite Herald:** the late bloomer: weak in missions capped at Tier III, strongest at the end.
- **Scrap Warlord:** cheap and early; strongest early, weakest late. Its finale is a custom JagdTiger with the Maus's
  128 mm KwK 44 instead of the stock laser (`Ger_Heavy/JagdTiger/Scrap128/Ger_Vehicle_JagdTiger_Scrap128`).
- **Rail Engineer:** long-range kinetic anti-armour. Rail bombardment = a few high-penetration rail slugs from high
  altitude (a `GA_Barrage` template with rail weapon data). The two-squad Rail Trooper landing
  (`GA_GerRailTroopersDrop`, Tier II) is in no kit; the kit uses the one-squad paradrop in slot 2.
- **Light Bringer:** precise, energy-hungry, anti-armour at range. The stock JagdTiger fires the Sonnensturm 128L
  multi-hit laser. Sonnengewehr follows the wartime orbital "sun gun" concept: a beam sweeps the strip the player
  draws.
- **Feuersturm:** anti-infantry and anti-bunker at short range; strong early and mid, weak against heavy armour at
  range.
- **Stahlläufer:** walkers instead of tanks; walkers are squad units, so the drops land them like infantry (Ju 390
  landing, no parachutes).
- **Festungsbaumeister:** the defensive commander: weakest attack, strongest in hold-the-line missions.
- **Sturmkommandant:** all-infantry pressure; weak against armour until the E-75. His STG squad is Iron Storm (user
  decision 2026-11-05): cyborg storm troopers with STG 44s, alone in slot 2 and with two Panzergrenadier squads in
  slot 4. See "Iron Storm templates" below.
- **Sky-Drop Quartermaster** (round 3d, 2026-11-05; portraits `T_Commander_ArmorDrop*`): logistics. Everything
  arrives by air and his armour keeps fighting: landed armoured cars and tanks, vehicle parts by parachute, Ju 87 G
  cannon Stukas against tanks, a parachuted field workshop that repairs and rearms nearby vehicles (with two Pak 40
  squads to defend it) and the E-50 by Mammut glider. See "Sky-Drop Quartermaster templates" below.

## Pending

- **Placeholders** (marked in `write_commander_kits.py`): Stahlläufer slot 6 waits for a
  Fels Krieger walker unit (now `GA_Barrage`). `SK_Etasphera07` exists, but no animations or unit Blueprint.
- **Red Spine:** `Squad_Ger_RedSpine` is a squad since 2026-11-05: `Squads/Ger/Walkers/RedSpine/Squad_Ger_RedSpine`
  (3 `Ger_Inf_Walker_RedSpine` with MP46s, 480 HP each on heavy infantry armour, 650 R + 250 M training cost) with an
  async spawner row; no barracks offers it yet. The drops are authored by `Tools/RedSpine/author_red_spine_drops.py`
  (about 125% of the squads' radixite training cost) and checked by `-RedSpineValidate`.
- Every kit ability has a template since 2026-11-05 03:09 (71 entries; the Sonnengewehr, fire strike, composite,
  resource, parachute and Elite Scavenger templates come from their own workstreams' scripts). A missing template
  shows as a failed `kit_ability_has_template` check and in the `KIT_MISSING_TEMPLATES` line of the area test.
- The Feuer Sturm paradrop squad is `Squad_Ger_FeuerSturm` (confirmed by the user, 2026-11-05).
- **Sky-Drop Quartermaster:** in game only through the headless DebugGame test so far. Icons are placeholders (the
  massive flare shows the flare curtain icon, the Puma drop the Pz III spotter icon, the vehicle parts drop the air
  minefield icon). The vehicle parts payback assumes Scavengers (other infantry gets a third) and about 2 radixite per
  vehicle part: user to confirm. The Puma drop lands one Puma at 225 R; the slot's 90 s cooldown is the Tier I top.
- Real icons: every round-3 template carries the icon of the template it was copied from (see below).
- The round-3 data-only drops that no feature test fires (five glider drops, four landed Ju 390 drops, three paradrops,
  the single Bf 109) are fired by `-KitDropsValidate` (`GlobalAbilitySystem/Tests/KitDrops/KitDropsTestRunner.cpp`, run
  with `Source/Tools/KitDrops/run_kit_drops_test.ps1`): click path and payment, ability ended once, the promised tanks or
  squads owned by the player, released and on the ground, the Bf 109 firing at an enemy T-34. The file has no UCLASS and
  compiles to nothing without `RTS_WITH_MAP_TESTS`, so plain Shipping never contains it.

## Ability tiers

| Tier | Abilities |
|---|---|
| I | `GA_GerReconFlight`, `GA_GerFlareCurtain`, `GA_GerSpiesInfiltration`, `GA_HarvesterDrop` (Pz IV harvester), `GA_GerPzIHarvesterDrop`, `GA_GerRadixiteMetalSupplyDrop`, `GA_GerRadixiteCacheDrop`, `GA_GerConstructionSupplyDrop`, `GA_GerMarderScavengerDrop`, `GA_Bf109Strafing`, `GA_GerFlak88Drop`, `GA_GerPanzerschreckPitDrop`, `GA_GerRadixitePzIIDrop`, `GA_GerEliteScavengersParadrop`, `GA_GerRailTroopersParadrop`, `GA_GerLightBringersParadrop`, `GA_GerFeuerSturmParadrop`, `GA_GerRedSpineDrop`, `GA_GerMassiveFlare`, `GA_GerPumaDrop`, `GA_GerVehiclePartsSupplyDrop`, `GA_GerIronStormParadrop` |
| II | `GA_GerParatroopers`, `GA_GerSpotterPzIIIDrop`, `GA_GerRadixitePzIIMarderDrop`, `GA_ScavengersDrop`, `GA_GerPz38tRailgunDrop`, `GA_GerRailTroopersDrop`, `GA_GerSFH18Drop`, `GA_GerPakPitDrop`, `GA_GerEliteScavengersPakDrop`, `GA_GerSturmPionierenPakPitDrop`, `GA_GerRadixitePzIIIWespeDrop`, `GA_GerPzJagerLaserDrop`, `GA_GerPzIIIFlammDrop`, `GA_GerRedSpinePairDrop`, `GA_GerFlakLuchsStugDrop` (Tier II since 2026-11-05), `GA_GerPzGrenIronStormParadrop` |
| III | `GA_3xJu87Strafing`, `GA_Ju87Strafing`, `GA_2xHortentrafing`, `GA_Me410Airstrike`, `GA_2xBf110Strafing`, `GA_Barrage`, `GA_LargeBarrage`, `GA_GerAirMinefield`, `GA_GerRailBombardment`, `GA_GerLuftlandeSturm`, `GA_GerFieldBunkerDrop`, `GA_HeavyCarpet_Ju390B_250Gr`, `GA_GerIncendiaryJu87`, `GA_GerFireBarrage`, `GA_GerSonnengewehr`, `GA_GerJu87GTankBusters`, `GA_GerFieldWorkshopDrop` |
| IV | `GA_GerRadixiteNuke`, `GA_GerV2Rocket`, `GA_GerKingTiger105Drop` (in no kit), `GA_GerE100Drop`, `GA_GerSturmtigerDrop`, `GA_GerJagdTigerDrop`, `GA_GerTigerRailDrop`, `GA_GerE50Drop`, `GA_GerE75Drop`, `GA_GerMausDrop`, `GA_GerJagdTigerLaserDrop`, `GA_GerJagdPantherFlamerDrop` |

## Spies Infiltration (`GA_GerSpiesInfiltration`)

A Tier I intel ability without aiming (`UGA_SpiesInfiltration`): the panel button fires it at once. It picks up to 6
units of the opponent that its owner cannot see (enemy units the fog hides; for an AI owner, player units without
the in-enemy-vision tag), one soldier per squad, and attaches a 600 cm vision bubble (`AGlobalAbilityVisionBubble`)
to each for 25 s, so the reveal follows the unit. The picks are random; they are spread at least one reveal radius
apart before nearby units fill the rest, and units revealed by the previous use go last. Cooldown 180 s, cost 200
radixite (`author_spies_infiltration.py`, also in `author_commander_abilities.py` BALANCE). Scrap Warlord and Field
Commander slot 1. Icon: `spies_infiltration` in `compose_ability_icons.py` (soldier with field glasses, `Spy_Spot` render).
With no hidden enemy units it still fires and starts its cooldown. Because it skips the aim, the controller's activate
sounds (`AnnouncerSoundOnActivate`, `Sound2DOnActivate`) never play: put its sound on `Sound2DOnExecute`. Candidates
are filtered by `IsHidden()`, so a unit hidden for another reason (outside the fog grid at a map edge) can take a pick.

## Cost and cooldown bands

- **Radixite only (user decision 2026-11-05):** every global ability costs radixite and nothing else: no vehicle
  parts, metal, fuel or ammo. `Source/Tools/Commanders/make_costs_radixite_only.py` enforces it on the data asset
  (keeps each template's radixite, drops the rest; re-run it read-only to list every cost) and every authoring script
  writes radixite-only costs. Composite drops charge on the parent; their children stay cost-free.
- **Drops:** about 125% of the radixite in the delivered units' training cost (no production time, lands anywhere).
  Variants without a cost of their own are costed as their base unit.
- **Strikes:** a fixed radixite band per tier.
- **Intel:** a little radixite.
- **Economy:** pays its radixite back in about 3-4 minutes.
- **Cooldowns:** Tier I 60-90 s, Tier II 120-180 s, Tier III 180-240 s, Tier IV 300-420 s.
- **Super-heavy drops:** planned as one use per mission (needs a uses-per-mission field; not built yet).
- **Building requirements:** the building of the ability's unit line (Refinery for economy, Barracks or a vehicle
  factory for drops, HQ for intel).

## Round-3 data-only templates

Authored by `Source/Tools/Commanders/author_commander_abilities.py` (`ROUND3_NEW`, `ROUND3_EDITS`,
`ROUND3_BALANCE`; `SCOPE = "round3"` touches only these keys). Run it headless on the DebugGame build under the DA
lock; `verify_commander_abilities.py` lists every kit slot with class, tier, cost and cooldown.

| Template | Class (copied from) | Contents | Tier | Cost | Cooldown |
|---|---|---|---|---|---|
| `GA_GerE50Drop` | `UGA_GliderTankDrop` (`GA_GerE100Drop`) | `Ger_Vehicle_E50` | IV | 1450 R | 360 |
| `GA_GerE75Drop` | `UGA_GliderTankDrop` | `Ger_Vehicle_E75` | IV | 1800 R | 360 |
| `GA_GerMausDrop` | `UGA_GliderTankDrop` | `Ger_Vehicle_Maus` | IV | 1875 R | 360 |
| `GA_GerJagdTigerLaserDrop` | `UGA_GliderTankDrop` | stock `Ger_Vehicle_JagdTiger` (Sonnensturm laser) | IV | 1700 R | 360 |
| `GA_GerJagdPantherFlamerDrop` | `UGA_GliderTankDrop` | `Ger_Vehicle_JagdPanther_Flamer` | IV | 750 R | 360 |
| `GA_GerRadixitePzIIDrop` | `UGA_AircraftDrop` (`GA_GerSpotterPzIIIDrop`) | 2x `Ger_Vehicle_PanzerII_F_RadixiteAutocannon` | I | 500 R | 90 |
| `GA_GerRadixitePzIIIWespeDrop` | `UGA_AircraftDrop` | `Ger_Vehicle_PanzerIII_J_RadixiteCannon` + `Ger_Vehicle_Wespe_RadixiteHowitzer` | II | 650 R | 150 |
| `GA_GerPzJagerLaserDrop` | `UGA_AircraftDrop` | 3x `Ger_Vehicle_PanzerJager_Laser` | II | 375 R | 150 |
| `GA_GerPzIIIFlammDrop` | `UGA_AircraftDrop` | 2x `Ger_Vehicle_PanzerIII_Flamm` | II | 1000 R | 180 |
| `GA_GerRedSpineDrop` | `UGA_AircraftDrop` | 1 `Squad_Ger_RedSpine` (3 walkers), landed Ju 390 | I | 825 R | 90 |
| `GA_GerRedSpinePairDrop` | `UGA_AircraftDrop` | 2 `Squad_Ger_RedSpine` (6 walkers), one landed Ju 390 | II | 1625 R | 150 |
| `GA_GerRailTroopersParadrop` | `UGA_Paratroopers` (`GA_GerEliteScavengersParadrop`) | 1 `Squad_Ger_RailTroopers` | I | 950 R | 90 |
| `GA_GerLightBringersParadrop` | `UGA_Paratroopers` | 1 `Squad_Ger_LightBringers` | I | 1000 R | 90 |
| `GA_GerFeuerSturmParadrop` | `UGA_Paratroopers` | 2 `Squad_Ger_FeuerSturm` | I | 1250 R | 90 |
| `GA_Bf109Strafing` | `UGA_AircraftStrafing` (`GA_Ju87Strafing`) | 1 `Ger_Aircraft_BF110Airborne` | I | 200 R | 90 |
| `GA_GerParatroopers` (edit) | `UGA_Paratroopers` | 2 Vultures + 2 Panzergrenadiere | II | 3000 R | 180 |
| `GA_GerSFH18Drop` (edit) | `UGA_Paratroopers` (parachute payloads) | 2 `Squad_Ger_SFH18_150mm` (15 cm sFH 18 guns on 3 cargo chutes each; was 2x leFH 18 until the sFH 18 squad existed) | II | 1875 R | 180 |
| `GA_HeavyCarpet_Ju390B_250Gr` (edit) | `UGA_AircraftBombing` | carpet length 50 m (was 500 m) | III | 800 R | 240 |

Notes:
- The Ju 390 drops of variants set the tank subtype and the variant Blueprint as `TankClassOverride`: the spawner
  row of a subtype holds the stock tank. The glider drops replace the E-100 override with their own tank, the stock
  JagdTiger included.
- **`Ger_Aircraft_BF110Airborne` is a Bf 109:** it uses the Bf 109 mesh and AnimBP (there is no Bf 110 model), so
  `GA_2xBf110Strafing` flies two Bf 109s; since round 5 (2026-11-05) its title and description say so ("2x Bf 109
  Strafing"). The enum key and the Blueprint name keep "Bf110" (config and the data asset store the key). `Ger_Aircraft_BF109` itself starts landed (`bM_StartedAsLanded`) and would
  take off first, so the single Bf 109 strafing uses the airborne one.
- The Ju 390 carpet run was 500 m (`CarpetBombingLength=50000`), against 30 m on every PE-8 template and a 60 m
  longest aimable strip: a stray zero, now 50 m. The bomber shuttles between the run's ends until its bombs are gone,
  so the length only sets the spread.
- The paradrop copies build their settings from text: a struct from `get_editor_property` can alias the memory of the
  template it was copied from. `Tools/ParachutePayloads/author_parachute_payload_drops.py` restores
  `GA_GerParatroopers` to one Vulture squad whenever it runs: re-run this script after it.
- Placeholder icons (need real ones): the glider drops show the E-100 Mammut icon, the Ju 390 drops the Pz III spotter
  icon, the paradrops the paratrooper icon, the Bf 109 strafing the 3x Ju 87 icon.

## Sky-Drop Quartermaster templates (round 3d, 2026-11-05)

Slots 1-4 are authored by `Source/Tools/SkyDrop/author_sky_drop_abilities.py` (headless DebugGame, DA lock; touches
only these four keys, fingerprints every other template before and after the save; report in
`Saved/SkyDropTest/author_sky_drop_<time>.txt`). Slots 5 and 6 come from their own workstreams (`Tools/Ju87G`,
`Tools/FieldWorkshop`); the E-50 is the round-3 glider drop.

| Slot | Template | Class (copied from) | Contents | Tier | Cost | Cooldown |
|---|---|---|---|---|---|---|
| 1 | `GA_GerMassiveFlare` | `UGA_FlareCurtain` (`GA_GerFlareCurtain`) | one flare, circle aim: 60 m vision and light radius (curtain flare 20 m), light 4x, flare 2.5x, burns 75 s (30 s) | I | 200 R (intel band, round 4) | 90 |
| 2 | `GA_GerPumaDrop` | `UGA_AircraftDrop` (`GA_GerSpotterPzIIIDrop`) | a Ju 390 lands 1 `Ger_ArmoredCar_Puma` (`Tank_Puma`) | I | 225 R (Puma 180 R) | 90 |
| 3 | `GA_GerVehiclePartsSupplyDrop` | `UGA_ResourceDrop` (`GA_GerRadixiteMetalSupplyDrop`) | 4 crates (`SM_ScavObj_VehicleParts1`) of 450 vehicle parts (was 25), 50 s each: 1800 VP at x1, Scavengers x3: 5400 VP | I | 400 R | 90 |
| 4 | `GA_GerFlakLuchsStugDrop` (existing) | `UGA_AircraftDrop` | 2 Flak Luchs + 1 StuG III, landed (unchanged) | II (was I) | 975 R (2x 240 + 300 R; was empty) | 150 (was 5) |
| 5 | `GA_GerJu87GTankBusters` | `UGA_AircraftStrafing` (Ju 87 G workstream) | two Ju 87 G with 3.7 cm cannon pods, APCR through 120 mm (round 4), no tech requirement | III | 650 R | 240 |
| 6 | `GA_GerFieldWorkshopDrop` | `UGA_CompositeDrop` (Field workshop workstream) | parachuted repair depot whose radius also makes every allied unit reload faster (round 4) + 2 Pak 40 squads | III | 1500 R | 240 |
| 7 | `GA_GerE50Drop` | `UGA_GliderTankDrop` | E-50 by Mammut | IV | 1450 R | 360 |

Notes:
- **Massive flare:** a single flare of `UGA_FlareCurtain` (`ShaderMarkerCount` 1) hangs over the middle of the flown
  strip (code change in `GA_FlareCurtain.cpp`; the 5-flare curtain is unchanged). With circle aim the strip is
  centred on the click (`MaxStripLength` 10 m), so the flare hangs over the clicked point; the target area is the
  curtain's shader at strip length 0 (a 60 m disc with one flare marker). Ignition height stays 12 m (below the
  camera). The burn particle effect is not scaled with `MeshScale`.
- **Vehicle parts payback (round 4):** at the user's rate of 1 radixite = 3 vehicle parts, the 1800 VP a normal (x1)
  squad gets are worth 600 R for 400 R, 1.5x like the other supply drops (see "Round 4 tuning" below); Scavengers get
  three times that. Scavenging adds the parts with `UPlayerResourceManager::AddResource` like wreck salvage.
- `FUnitCost::ResourceCosts` is `EditDefaultsOnly`: Python cannot set it on a template; the script rebuilds the cost
  struct from its exported text.
- Test: `-GlobalAbilityAreaValidate -GlobalAbilityKitsOnly -GlobalAbilitySkyDrop` (`-GlobalAbilityAreaCapture=` and
  `-GlobalAbilityKitPictures=` for pictures) checks the kits, then fires the massive flare through the click path,
  the Puma drop and the vehicle parts drop once each.

## Round 4 tuning (2026-11-05, user decisions; Tuning agent)

Applied by `Source/Tools/Tuning/run_round4_tuning_headless.py`, which re-runs each feature's own authoring script (so a
later re-run of any of them keeps these values) and fingerprints every other template. Costs stay radixite only.

| Template | Change | Cost before -> after | Cooldown |
|---|---|---|---|
| `GA_GerJu87GTankBusters` | APCR penetrates 120 mm over the whole cannon range (`UAircraftCannonPodsComponent::CannonArmorPen` on the Ju 87 G Blueprint; the shared `Flak36_37MM` data keeps 45/31 mm raw = 67.5/46.5 APCR); 24 rounds per cannon and the wing MG 15s kept; the `Tech_HarvesterSpeed` requirement copied from `GA_3xJu87Strafing` removed | 650 R (unchanged) | 240 |
| `GA_GerEliteScavengersParadrop` | 250 R per squad; the 2 Panzerfaust soldiers carry a Mauser backup (switch-weapon button); the AI counts the squad as anti-tank (`Global_GetIsAntiTankSquad`) | 1150 R -> 500 R | 90 |
| `GA_GerSonnengewehr` | no friendly fire: burns only the ordering player's enemies (`FSonnengewehrSettings::bBurnOwnUnits` false replaces `AffectedUnits`) | 800 R -> 675 R (as the Panzer Grenadiers) | 240 |
| `GA_GerFieldWorkshopDrop` | "rearm" = only a radius in which every allied unit (vehicles, infantry, team weapons) reloads faster (reload time x0.7); magazine top-up removed; repair kept; pack-up shows two small waves of grey dust (`NS_DestructionSmoke`) instead of five construction-smoke puffs | 1500 R (unchanged) | 240 |
| `GA_GerRadixiteMetalSupplyDrop` | per crate 30 R + 20 M -> 90 R + 60 M (x1): 360 R + 240 M = 600 for 400 at x1 (1.5x); Scavengers x3 | 400 R | 90 |
| `GA_GerConstructionSupplyDrop` | per crate 10 R + 40 M -> 30 R + 120 M (x1): 120 R + 480 M = 600 for 400 at x1 (1.5x) | 400 R | 90 |
| `GA_GerRadixiteCacheDrop` | unchanged: a 1200 R deposit for 500 R (2.4x; mined by harvesters, no squad multiplier) | 500 R | 90 |
| `GA_GerVehiclePartsSupplyDrop` | per crate 25 -> 450 VP (x1): 1800 VP = 600 R for 400 at 1 R = 3 VP (1.5x) | 400 R | 90 |
| `GA_GerMassiveFlare` | same as the other intel abilities: 200 R / 90 s (curtain 200 R / 90 s, recon 150 R / 60 s, spies 200 R / 180 s) | 200 R (unchanged, now explicit) | 90 |

- **Exchange rates:** 1 radixite = 3 vehicle parts (user). The project has no metal rate; the resource drops count
  metal 1:1 with radixite. Crates have no lifetime and auto-scavenging stays off.
- **Reading of the cut-off sentence:** every resource drop returns more than its radixite cost would buy, even for a
  normal x1 squad; tuned to about 1.5x at x1. `-ResourceDropValidate` empties the crates with a Sturm Pioniere squad
  (x1) and checks the gain beats the cost.
- Spies' 180 s cooldown is above the Tier I band (60-90 s); left as it is.

## Iron Storm templates (round 4, 2026-11-05)

The Sturmkommandant's STG squad is Iron Storm (`Squad_Ger_IronStorm`, `Squads/Ger/EliteInfantry/IronStorm/`: four
`Ger_Inf_IronStorm_STG` cyborgs with STG 44s, elite health on heavy infantry armour, 700 R + 300 M training cost).
Both templates are authored by `Source/Tools/IronStorm/author_iron_storm_paradrops.py` (headless DebugGame, DA lock;
touches only these two keys, fingerprints every other template before and after the save; report in
`Saved/IronStormTest/author_iron_storm_<time>.txt`) and checked by `-IronStormValidate`
(`Units/Squads/Tests/IronStorm`).

| Slot | Template | Class (copied from) | Contents | Tier | Cost | Cooldown |
|---|---|---|---|---|---|---|
| 2 | `GA_GerIronStormParadrop` | `UGA_Paratroopers` (`GA_GerEliteScavengersParadrop`) | 1 `Squad_Ger_IronStorm` | I | 875 R (700 x 1.25) | 90 |
| 4 | `GA_GerPzGrenIronStormParadrop` | `UGA_Paratroopers` (`GA_GerEliteScavengersParadrop`) | 2 `Squad_Ger_PanzerGrenadiere` + 2 `Squad_Ger_IronStorm`, one transport | II | 3450 R ((2 x 675 + 2 x 700) x 1.25) | 180 |

Notes:
- Both show the paratrooper icon (placeholder) and keep the source's transport, parachutes, sounds and marker; the
  four-squad drop takes `GA_GerParatroopers`' landing spread when it is wider.
- Until 2026-11-05 the async spawner's `Squad_Ger_IronStorm` row held `Squad_Ger_SturmCommando`, so anything that
  spawned Iron Storm by subtype (the Breakthrough and Strike Division barracks option, `DA_TrainingLayout`, survival
  supply) got a Sturm Kommando squad, which its construction script stamps `Squad_Ger_SturmKommandos`. The row now
  holds `Squad_Ger_IronStorm`; those paths now train the real Iron Storm squad.
- A squad Blueprint's subtype comes from its construction script (`Squad Subtype` set with a literal), not from the
  CDO; Iron Storm's CDO default now matches it too.

## Status (2026-11-05)

Done in C++:
- `EGlobalAbilityTier`, `UGlobalAbility::M_AbilityTier`, `AMissionManager::PlayerGlobalAbilitySettings`, the cap in
  `UGlobalAbilitiesManager`, set by `ACPPController::SetupGlobalAbilityManager`.
- Twelve commanders in `ERTSCommander` (Sky-Drop Quartermaster appended 2026-11-05), seven slots with their tier
  ranges in `RTSCommanderConstants`, twelve kits in `DefaultGame.ini`.
- Tests: `-GlobalAbilityAreaValidate` starts with the kit checks: every commander has a kit with title, description
  and portrait, seven abilities, none twice, each with a template whose tier fits the slot; it logs
  `RTS_ABILITY_TEST KIT <commander> slot=<n> <ability> class=... tier=... cost=... cooldown=...` per slot.
  `-GlobalAbilityKitsOnly` stops after the kit checks; `-GlobalAbilityKitPictures=<folder>` draws the ability panel
  (Field Commander kit) and the commander select screen (Light Bringer, then Sky-Drop Quartermaster, the last
  entry) into PNGs with `FWidgetRenderer`. `-GlobalAbilitySkyDrop` adds the Sky-Drop steps after the kit checks. The strafe test checks the Luftwaffe Liaison kit.

Done in data (2026-11-03 and 2026-11-05, `Source/Tools/Commanders/author_commander_abilities.py`, idempotent; check
with `verify_commander_abilities.py`):
- Round 2: harvester drops, Pz 38(t) Railgun drop, scavenger drop, single Ju 87, 2x Bf 109 (`GA_2xBf110Strafing`), large barrage, rail
  bombardment, and the Mammut glider drops for Sturmtiger, JagdTiger, King Tiger 105 and Tiger Rail.
- Round 3: the table above.
- Script gotchas: `unreal.GlobalAbility` is the `UGlobalAbility` class, not the enum (take the enum type from the
  map keys); a soft class override can't be cleared from Python, only set; copy structs between templates through
  `export_text` / `import_text`.

Still to do:
- The pending items above.
- Building requirements on the templates (which building per ability, valid for every German faction). For now
  (2026-11-07) no template has a technology or unit/building requirement.
- Set `PlayerGlobalAbilitySettings` on the other missions' managers. Done: Ger_FirstMission Tier I + free costs,
  Ger_MarderStugMission Tier II with costs (`Tools/Round5Fixes/set_mission_tier_caps.py`, one editor run per map).
- Uses-per-mission for Tier IV drops.

## Round 5 fixes (2026-11-05)

- **No technology gates on templates.** `GA_3xJu87Strafing` carried `RequiredTechnology = Tech_HarvesterSpeed` (a copy
  bug): without that tech the click did nothing, and every template copied from it inherited the gate. Every template
  now has `Tech_NONE` except `GA_GerJu87GTankBusters` (Tuning agent). Unit requirements are kept. Every authoring
  script that copies `m_ability_requirements` from another template clears the technology after the copy.
  Data pass: `Source/Tools/Round5Fixes/author_round5_fixes.py`.
- **Walkers leave the landed Ju 390 by its cargo door.** `FAircraftDropGlobalAbilitySettings::bSquadsWalkOutOfCargoDoor`
  (default off): the squads of a landed drop appear one after another (1.5 s apart) at the aircraft mesh's
  `CargoDoor` socket, projected to the navmesh, and get an ordinary move order to their spot on the
  `RadiusSpawnSquads` circle; from then on they are normal units. The aircraft stays landed until 4 s after the last
  squad appeared. Without the socket the squads leave from under the aircraft (error logged). The Ju 390 mesh
  (`SK_JU309B_Animated`, no collision) has the socket under the rear fuselage. On for `GA_GerRedSpineDrop`,
  `GA_GerRedSpinePairDrop` and `GA_GerFelsKriegerDrop`.
- **Mission caps:** `Ger_FirstMission` (the first German mission; its victory continues to `Ger_MarderStugMission`)
  allows Tier I only; `Ger_MarderStugMission` (StuG / Marder) up to Tier II. Other missions keep Tier IV.
- **`GA_2xBf110Strafing`** is titled "2x Bf 109 Strafing" (see the note above).
- Test: `-Round5FixesValidate` (`GlobalAbilitySystem/Tests/Round5Fixes`): template checks, a Ju 87 strike clicked with
  no technology researched, the sFH 18 firing beyond the leFH 18's range, and the cargo-door walk-out of the Red Spine
  pair drop (screenshots in `Saved/Round5FixesTest`). On a mission map `-Round5FixesValidate -Round5MissionCaps=TierI`
  checks that map's cap instead.
