# Berlin mission: the Burrower squad (design proposal)

Status: **look picked 2026-11-22: A "Kumpel".** The owner asked for a more PBR, less flat look in line with the
project's art direction. That pass is concept round v02: `Docs/Concepts/BerlinBurrowers/v02/sheet_v02.png`, see
section 3.1. The owner approved v02 the same day and it is **built** (section 3.2). **Role decided 2026-11-22: R2,
one squad on contact** (section 4); the squad type and Blueprint exist (section 3.3). Still open: the contact
objective script itself, the reveal (V1/V2/V3), the abilities, the portrait and the voice (section 7).

- Mission flow: [BerlinMissionFlow.md](BerlinMissionFlow.md), Phase 1, BONUS "The cellar people".
- Mission background: [BerlinMission.md](BerlinMission.md).
- The enclave: `Docs/Environment/BerlinBreweryEnclave.md` (`BP_Berlin_BreweryHall` + `BP_Berlin_BreweryVaults`, placed on
  `Ger_Berlin` 2026-11-17).
- Lore: `DesignTexts/Lore.txt`, 2.5 "The Burrowers" and the Gandy-Dancers.
- Concept sheet: `Docs/Concepts/BerlinBurrowers/v01/sheet_v01.png` in the game project (small copy `sheet_v01_small.jpg`).
  Sources and tools: `D:/Blender/BlenderProjects/BlenderTanks/Infantry/GerBurrowers/` (`Burrowers_Brief.md`).

## 1. Who the Burrowers are

From the lore (2.5): when the firebreaks were detonated, what was left of the local people, scattered army units and
miners went into the underground networks of Western Europe: metro systems, salt mines, bunkers. The Gandy-Dancers are
their armoured combat engineers, "the shock-troops of excavation".

The Berlin group, as proposed here:

- **Who went down.** Berlin was the first nuke target (January 1945). The survivors were the people who were already
  underground when it hit: air-raid wardens (Luftschutz), U-Bahn and Reichsbahn track crews, Volkssturm, and the
  workers of a Friedrichshain brewery, whose deep lager cellars became the core of the settlement.
- **Thirty years below.** For three decades they have lived in the cellars and dug connections to the U-Bahn and S-Bahn
  tunnels and the bunkers under Potsdamer Platz. The digging crews are the Gandy-Dancers. They come up only at night,
  through the brewery's vents and cellar necks.
- **Why they know the patrols.** The CSD believes Berlin is dead and has never looked underground. For years the
  Burrowers have watched every Soviet patrol from the gratings and cellar windows, and they know where the Shield posts
  see and where they don't. This is the in-world reason the contact reveals the patrol routes and the detection zones.
- **How they meet the player.** These are the first surface Germans they have seen in thirty years ("they think we are
  dead"). They are wary, and they will not fight in the open next to the Reichstag. They give knowledge and maybe a few
  people, not an army.
- **Berlin rules hold.** No Radixite and no Phytophage in their look or kit (Berlin is the nuke-only map). Their
  signature materials come from what they live in: coal dark, brewery copper, rubber, rail steel, 1940s civil-defence
  kit.

## 2. How they differ from the Scavengers

The Scavengers are surface nomads; the Burrowers are cellar people. They share the body and skeleton (section 6), so
the differences have to be in silhouette, colour and kit.

| | Scavengers | Burrowers |
|---|---|---|
| Where they live | the open wasteland, in the heat | under Berlin, in the dark, for thirty years |
| Colour | sun-bleached tan and rust | dark (coal, rubber, blue-grey); one signature accent per option |
| Head | hood + gas mask with a hose | helmet or wrap instead of the hood (miner, kettle, Stahlhelm, gladiator or bandage + goggles) |
| Light | none | cap lamps, lanterns or luminous badges in most options (they live in the dark) |
| Kit | scavenged plates, licence plate, vials | digging and cellar tools: pick, maul, rail, mallet, sewer hook |
| Weapons | MP 40, Kar98, Mauser pistol (`Ger_Inf_Scavenger*`) | 1945 stock kept in the cellars: MP 40, Kar98, StG 44, Panzerfaust (all exist as weapon BPs) |
| Abilities | IdScavenge (wrecks for resources), IdRepair, field construction | knowing the ground: tunnels, listening, clearing cave-ins (6.2); IdRepair kept |

## 3. The look options (concept sheet v01)

All five are sketches on the unchanged Scavenger body: a cloth recolour plus simple shapes for the kit. Each option
has one signature that has to read at game zoom (the two right-hand tiles show a squad of four next to a Scavenger).

| Option | Signature | In short | Leans toward |
|---|---|---|---|
| **A Kumpel** | cap lamps | coal-black miners, leather helmet with an electric lamp, battery box, pick, MP 40 | scouts / diggers |
| **B Kesselvolk** | verdigris copper | the brewery people: kettle-copper helmet and plates, rubber aprons, barrel-lid shield, mallet, rifle; ties to the enclave's copper and tile | the enclave's own people |
| **C Gleisbautrupp** | rail steel + ochre track paint | the Gandy-Dancers: welded visor, rail-steel pauldrons, a rail on the back, track maul, heavy boots | armoured engineers (assault) |
| **D Luftschutz** | luminous blackout badges | air-raid wardens: blue-grey, gladiator helmet, Scavenger gas mask kept, glowing badges, slit lantern, rifle | defenders; closest to the Scavenger |
| **E Grottenolme** | pale head on a black coat | bandaged pale heads, black smoked goggles, long black rubber coats, lantern, sewer hook, MP 40 | the strangest, most "thirty years below" |

Notes for the pick:
- **B** is the obvious tie to the enclave (same copper and green as `BP_Berlin_BreweryHall`).
- **C** is the lore's Gandy-Dancer. It is the best fit if contact gives the player a squad (section 4).
- **A** and **D** are the least distinct from each other at full zoom-out (both dark with small lights). Pick at most
  one of them.
- **C**'s accent, faded ochre track paint plus dark rust, is the warmest colour on the sheet. It is yellow, not
  Radixite orange, but it is close to the Breakthrough faction's Dunkelgelb. If that is too close, the accent can become
  white or signal red-and-white barrier stripes.
- The "Zoomed out" tile uses an **approximate** camera, 42 m away at 55 degrees, not the measured game camera. Treat it
  as "far", not as the exact in-game size. At that distance A, D and E are all dark figures. Only the lamps (A), the
  badges (D) and the pale heads (E) tell them apart.
- Options can be mixed: for example a **B** enclave population with a **C** squad that joins the player (one squad
  look per role).

### 3.1 The pick: A Kumpel (v02, PBR pass)

The design is the same as v01; the materials and shapes are not:

| Part | v02 |
|---|---|
| Cloth | the Scavenger texture recoloured to charcoal canvas, so its seams, straps and wear still show; coal-dust dirt |
| Head | hood removed, the Scavenger gas mask kept (cellar gas) on a canvas balaclava, soot-black scarf |
| Helmet | waxed leather Grubenhelm: dome, narrow brim, short front peak, raised comb, all scuffed lighter at the edges |
| Cap lamp | enamel housing chipped to steel, tarnished brass bezel, warm glass; a real light in the cellar shot |
| Belt | wide leather belt, rubber battery box at the back with a cable over the helmet to the lamp, ribbed tin Draeger self-rescuer |
| Legs | conveyor-belt rubber knee pads with leather straps |
| Weapons and tools | pick carrier (forged iron pick on a hickory handle, MP 40 on the chest), two MP 40s with the pick on the back, one Kar98 |

All kit materials have the same layers: a mottled base, edge wear to a lighter or bare layer, grime in the cavities,
ash and coal dust on faces that point up, and a fine bump. In the real asset these become baked textures (the Rail
Trooper pipeline does this with `rt_bake.py`).

The light is the squad's signature. On a dark map, or at night, four cap lamps are what the player sees first. A
working cap-lamp light in game would be a small spot light or light-function decal per unit (performance to check)
or only an emissive glass; that is decided when building.

### 3.2 Built (2026-11-22)

Blender source and rebuild steps: `D:/Blender/BlenderProjects/BlenderTanks/Infantry/GerBurrowers/README.md`. Unreal build:
`Source/Tools/Burrowers/build_burrowers.py` (headless `UnrealEditor-Cmd -run=pythonscript`, idempotent). Look check:
`Source/Tools/Burrowers/kumpel_look_test_run.sh` (throwaway editor, PIE on `RTS_ToCopySmall`; pictures in
`Saved/Burrowers/look/`).

All assets are in `/Game/RTS_Survival/Blueprints/Squads/Ger/Burrowers/`:

| Asset | What |
|---|---|
| `Mesh/SK_BurrowerKumpel` | the Scavenger body with the hood removed (gas mask kept) plus the kit, 7.2k tris (kit 3.5k); on `InfantryMaster_Skeleton` with `SK_PostApoSoldier_PhysicsAsset`, so every infantry animation plays |
| `Material/MI_BurrowerKumpel_Cloth` | slot 0: child of `M_ScavengerNoCamo` with `T_BurrowerKumpel_Diffuse` (the Scavenger texture with the cloth dyed charcoal and the exposed collar dyed as dark wool) |
| `Material/MI_BurrowerKumpel_Kit` | slot 1: child of the Rail Troopers' `M_RailTrooperArmor` with `T_BurrowerKumpelKit_{BC,ORM,N,E}` baked from the v02 materials; `GlowColor` is warm lamp light, `GlowStrength` 14 |
| `Ger_Inf_BurrowerKumpel` | copy of `Ger_Inf_Scavenger` (MP 40) with the new mesh |
| `Ger_Inf_BurrowerKumpelRifle` | copy of `Ger_Inf_ScavengerRifle` (Kar98) with the new mesh |

Kit pieces:
- knitted balaclava and scarf;
- leather Grubenhelm with brim, peak, comb and a leather neck flap;
- cap lamp: enamel housing, brass bezel, and glowing glass through the emissive mask;
- battery box on the left back hip, with a cable to the lamp;
- wide leather belt, ribbed Draeger self-rescuer and rubber knee pads;
- the pick on the back, for every soldier.

Weapons come from the weapon Blueprints. Each piece is weighted to one bone; the scarf and belt take the body's own
weights.

Differences from the v02 concept:
- The pick carrier with the pick in hand is gone; the pick rides on the back, because the hands hold the weapon.
- There is no spot light from the lamp, only the emissive glass. A real light per unit is a later performance call.

Not built yet: the abilities in 6.2, the portrait and the voice.

### 3.3 The squad (2026-11-22, after the role decision)

- **C++:**
  - `ESquadSubtype::Squad_Ger_Burrowers` (value 61, appended), display name "Burrowers".
  - `Burrowers_Radixite` = 300 in `DeveloperSettings.h`: never paid; only read where a squad's worth is used.
  - Squad data in `CPPGameState`: Scavenger health, speed, resistances, Tier 1 experience and Untrained stress.
  - Abilities: the Scavenger list without IdScavenge (the Scavengers stay the harvesters); IdRepair is kept and
    three slots are free for the 6.2 abilities.
  - Development editor DLL rebuilt 2026-11-22.
- **Blueprint:** `Squad_Ger_Burrowers`, a copy of `Squad_Ger_Scavengers` with 3 MP 40 Kumpels and 1 Kar98 Kumpel.
  It keeps the Scavenger components: field construction, health, reinforcement, experience, and the Scavenger voice
  type for now.
- **Spawner row:** `RTS_BP_RTSAsyncSpawner` SetupGerSquads has `Squad_Ger_Burrowers`. A mission gives the squad
  with `AsyncSpawnActorAtLocation(FTrainingOption(UNType_Squad, Squad_Ger_Burrowers), ID, location)`.
- **Not trainable:** no training-menu option or card, on purpose (R2).
- **Test:** `Tools/Burrowers/kumpel_look_test_run.sh 600 squad`, a PIE spawn in a throwaway editor; results in
  `Saved/Burrowers/squad/`.

## 4. Ally only, or a squad for the player? (open question 1)

| | R1 Ally in the enclave only | R2 Contact gives one squad (this mission) | R3 Also trainable later |
|---|---|---|---|
| What the player gets | the reveal (section 5) | the reveal + one Burrower squad for the rest of Berlin | R2 + a barracks option or card in later missions |
| Burrowers in the world | idle, unselectable people at the enclave (dressing) | as R1, and one squad switches to the player | as R2 |
| Mesh | Scavenger mesh with a new material instance and attached props may be enough | full re-skin (rail-trooper pipeline) | as R2 |
| C++ | none (mission Blueprint: trigger, reveal, voice line) | new squad subtype, its abilities, the hand-over node | R2 + training option, costs, card entry, balance |
| Voice | 1-2 mission lines | + a squad voice set | + full set like other squads |
| Cost | low | medium (comparable to the Rail Troopers: new mesh, squad and subtype) | high |

**Decided (owner, 2026-11-22): R2,** one squad on contact. It is built so that R3 later is mostly data (a
training row and a card), not new code.

- The 2026-11-15 brewery brief (the owner's pick of the enclave) already said that making contact gives "a small
  Gandy-Dancer engineer squad" and opens an infantry-only tunnel exit in the south lane near the Potsdamer Platz
  sinkholes. The 2026-11-22 flow doc makes the reward the reveal and leaves the unit open. R2 keeps both.
- A BONUS that only gives information is easy to skip. One squad that is good at stealth makes the detour worth it.

## 5. What the reveal shows (open question 2)

The flow doc says: "until then the zones are visible only once explored". So the reveal has to show more than
exploring would, or the bonus gives nothing.

| | What appears on contact |
|---|---|
| **V1** (proposal) | All detection zones and all patrol routes, on the minimap and in the world, **explored or not**. |
| V2 | Only what the Burrowers can reach from their tunnels: the south and central lanes. The Reichstag's own zone stays unknown until explored. |
| V3 | Patrol routes only. The zones still need exploring. |

**Proposal: V1.** It is simple, it is a clear reward for an early detour, and stealth is still hard: knowing where a
zone is does not stop a harvester from being seen in it. V2 is the option if V1 makes the mission too easy in testing.

Patrol routes on the minimap: a dashed line per patrol route, drawn from the patrol definitions in the Berlin
enemy-forces data asset (flow doc 4.5). When a patrol is killed and replaced by the Reichstag factory, the route stays.

## 6. What the squad needs (if R2)

### 6.1 Base: the Scavenger squad

The Scavengers have no class of their own; they are a data setup on the generic squad classes (details in 6.4). The
Burrowers can be built the same way. The closest precedent is the **Elite Scavengers**: they keep the same mesh, get a
new material instance and a new `ESquadSubtype`, and are built by a script (`Tools/EliteScavengers/build_elite_scavengers.py`).
For R1, or a cheap R2, the Burrowers can follow that path, with the look's helmet and kit added as props on the head and
spine sockets. For the full look, use the Rail Trooper path: a new skeletal mesh on the same skeleton.

### 6.2 Abilities (proposals)

| Ability | What it does | Check against the decisions |
|---|---|---|
| **Durch den Keller** (tunnel move) | The squad enters a tunnel mouth and comes out at another one after some seconds, unseen. Mouths: the brewery vaults and the south-lane exit near the Potsdamer Platz sinkholes (2026-11-15 brief); more can be added. | Fine: a unit in the tunnel is not inside any detection zone. It does not get round decision 2, it avoids the zones. |
| **Horchposten** (listening post) | The squad stops at a grating or cellar window for a while; enemy units in a large radius show on the minimap with their heading, through the fog. Ends when the squad moves. | Fine. Helps to catch runners (flow doc 4.2) before they reach the Reichstag. |
| **Clear the cave-in** (only with look C, or as the squad's engineer ability) | Clears a collapsed gap in the south lane so that medium vehicles can pass. BerlinMission.md leaves "whether collapsed sections can be cleared" open; this answers it. | Fine; takes time in the open, so it is a stealth risk. |
| ~~Zone-immune stealth~~ | Rejected: owner decision 2 says any player unit in a detection zone raises the alarm. A unit that ignores zones would break that rule. Only if the owner wants an exception. | Conflicts with decision 2. |
| Sabotage (rail yard) | Not given to the Burrowers: the flow doc (5.3) gives the rail-yard sabotage to the Scavengers. Two squads with the same job would make one redundant. Note: the Scavengers' ability list (6.4) has no ability named sabotage; the rail-yard targets are meshes to destroy (`Tools/BerlinMap/poi/rail_yard_target_import.py`). How the Scavengers sabotage them is unverified here. | Overlap: keep it with the Scavengers. |
| **Field construction** (kept, extended) | The Scavengers already have `FieldConstructionAbilityComponent` (barbed wire, hedgehogs). The lore says the Gandy-Dancers "build the war zone", so the Burrowers keep it and add one Berlin piece, for example a timber-shored rubble barricade or a sandbagged cellar window (a firing position). The cheapest ability that fits the lore: the component exists, only a new construction type and mesh are needed. | Fine. |

Weapons: existing German weapon BPs only (`Ger_Weapon_MP40`, `Ger_Weapon_Kar98`, `Ger_Weapon_STG44`, `Ger_Weapon_PanzerFaust`).
Proposal per look: A and E MP 40, B and D Kar98, C StG 44 plus one Panzerfaust (engineers). No new weapons.

### 6.3 Build list

| Piece | R1 | R2 | Precedent |
|---|---|---|---|
| Mesh | material instance + attached props on the Scavenger mesh | `SK_Burrower` on `InfantryMaster_Skeleton`, 1-2 material slots, built in Blender on `SK_PostApoSoldier` | R1: Elite Scavengers (`Tools/EliteScavengers`); R2: Rail Troopers (`Infantry/GerRailTrooper`, `Tools/RailTroopers`) |
| Squad + units | idle dressing actors | `Squad_Ger_Burrowers` + 1-2 unit BPs, new squad subtype (appended at the end of the enum) | `Squad_Ger_RailTroopers` |
| Abilities | none | tunnel move, listening post, (clear cave-in): new C++ (read `Docs/Abilities/Abilities.md` first) | |
| Hand-over | | mission node: give the enclave squad to the player on contact | |
| Reveal | mission: zones + routes on minimap (V1/V2/V3) | same | `TrackUnitGroupOnMiniMap`, flow doc 4.1 |
| Portrait | not needed | new portrait + talking shader (A: cap-lamp flicker, B: copper glint, E: lantern light) | Scavenger portrait (breath vapour + vials) |
| Voice | `Berlin_CellarPeople` (flow doc 6) | + a squad voice set | see 6.5 |

### 6.4 The Scavenger squad in the code (as found 2026-11-22)

- **Squad:**
  - `Squad_Ger_Scavengers` is a child of `BP_SquadController` (C++ `ASquadController`, `Units/SquadController.h`).
  - Components: `FieldConstructionAbilityComponent` (barbed wire, hedgehogs), `SquadHealthComponent`,
    `SquadReinforcementComponent` and `ExperienceComponent`. Voice type `ERTSVoiceLineUnitType::Scavengers`.
- **Units:**
  - All are children of `RTS_BP_SquadUnit` (C++ `ASquadUnit`) with a `UScavengerComponent` (`Scavenging/ScavengerComponent/`)
    and a `RepairComponent`. `ASquadController::GetIsScavenger()` / `GetIsRepairUnit()` read those components.
  - Mesh `SK_PostApoSoldier` on `InfantryMaster_Skeleton`
    (`Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/`, material `M_ScavengerNoCamo` + the `ScavengerVariations` MIs).
  - The squad's units:

    | Unit | Weapon |
    |---|---|
    | `Ger_Inf_Scavenger` | MP 40 |
    | `Ger_Inf_ScavengerRifle` | Kar98 |
    | `Ger_Inf_ScavengerPistol` | Mauser |

    `Ger_Inf_ScavengerAT` (Panzerschreck) is in the folder but not in this squad.
- **Abilities:**
  - `BasicScavengerAbilities` in `Game/GameState/CPPGameState.cpp` (around line 10167): Attack, Move, Stop, Patrol,
    Retreat, SwitchWeapon, **IdScavenge**, **IdRepair**, AttackGround, PickupItem, EnterCargo.
  - Field construction comes from its component.
  - The squad uses basic infantry health, speed and resistances, and costs 250 Radixite (`DeveloperSettings.h`).
- **Scavenging:**
  - IdScavenge moves the squad to an `AScavengeableObject` (a wreck) and runs a timed scavenge for random resources.
  - `UAutoScavengeTriggerComponent` sends nearby squads that have IdScavenge to scavenge on their own.
- **Registration:**
  - `ESquadSubtype::Squad_Ger_Scavengers` (`Units/Enums/Enum_UnitType.h`). New subtypes are appended at the end,
    because training options store the numbers.
  - Cards `Card_Ger_Scavengers` / `Card_Train_Scavengers`.
  - Training row in `UTrainingMenuManager::InitAllGameSquadTrainingOptions()`.
- **Portrait:** `ERTSPortraitTypes::GermanScavengers` (breath-vapour + vials shader, `Docs/UI/ScavengerPortrait.md`).
- **Voice:** `ERTSVoiceLineUnitType::Scavengers`. The waves are in `Sounds/VoiceLines/GerFaction/GerInfantry/Scavengers/`
  (31 line folders, from Attack to UnitDies, including Scavenge and Repair).
- **Not verified:**
  - the squad size and how many of each unit;
  - which component adds the field-construction button;
  - whether the Scavengers have IdCapture.
- No Burrower or Gandy-Dancer code or asset exists yet (docs only).

What the Burrowers would keep from it:
- the generic squad and unit classes;
- MP 40 / Kar98-type weapons;
- IdRepair (engineers);
- field construction, with one Berlin piece added (6.2).

What they would change:
- **IdScavenge:** drop it, or keep it as a weaker version. The Scavengers are the harvesters; the Burrowers are guides.
- Add the abilities in 6.2.
- A new subtype, portrait type and voice type.

### 6.5 Voice

- **Mission line:** `Berlin_CellarPeople` (already in the flow doc's list), spoken by a Burrower elder through the
  advisor channel, or by the advisor reporting the contact.
- **Squad voice (R2):** a new voice set. Cheapest good option: ElevenLabs dry takes through the project's voice-changer
  tool (`VoiceLines/VoiceChanger`) with a "cellar" preset: low pass, short stone-room reverb, a little hoarseness. They
  speak an older, Berlin-tinted German-accented English; their words are about the dark, the tunnels and "up there".
- Reusing the Scavenger lines with a cellar filter is possible but makes the two squads sound the same.

## 7. Questions for the owner

1. **Look:** picked: **A Kumpel** (2026-11-22). Is the v02 PBR pass the direction to build?
2. **Role:** decided: **R2**, one squad on contact (2026-11-22).
3. **Reveal:** V1 (proposal), V2 or V3?
4. **Abilities (if R2):** tunnel move, listening post, clear the cave-in: all three, or a subset? Where are the tunnel
   mouths besides the brewery and the south lane?
5. **Size and cost:** squad of 4 (as the sketches)? Given free on contact (proposal) or bought?
