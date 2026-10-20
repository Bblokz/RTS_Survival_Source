# German technologies, batch 2

Eight researchable technologies added on 2026-10-20, and the full checklist for adding a technology.

## The technologies

| Tech (`ETechnology`) | Research at (both German doctrines) | Affects | Effect |
| --- | --- | --- | --- |
| `Tech_StereoRangefinder` | Light Factory Radixite Reactor, slot 13 | StuG, Hetzer, Pz IV/70, Jagdpanther, Jagdtiger | main gun +10% range, +15% accuracy (accuracy is clamped to 100 when firing) |
| `Tech_Tungsten88` | same slot, after the rangefinder | Tiger, Tiger H1, King Tiger, Jagdpanther, Panther II | 88 mm guns +15% penetration (also at max range) |
| `Tech_Assault_ShellHoist` | Light Factory Radixite Reactor, slot 14 | Brummbär, Sturmtiger, Pz I 15cm | main gun reload time −30% |
| `Tech_HalfTrack_ArmoredBay` | Light Factory Fuel Expansion, slot 7 | SdKfz 250/251 family | +20% hull armour; passengers bail out with half their health when the half-track dies |
| `Tech_Flak88_GroundRole` | Barracks Fuel Expansion, slot 10 | 8.8 cm Flak team weapon | adds a mode toggle; while active the 88 has +20% penetration |
| `Tech_Infantry_Kit` | Barracks Reactor Expansion, slot 10 | Jäger, Panzergrenadiere, Gebirgsjäger, LMG squad | +15% health |
| `Tech_Sniper_Ghillie` | Barracks Reactor Expansion, slot 11 | Sniper team, Gebirgsjäger (not Vultures) | +33% health (Gebirgsjäger with M42 Kit: ×1.53) |
| `Tech_Pioneer_Entrenching` | Barracks Reactor Expansion, slot 12 | Sturmpioniere, Scavengers, Medic Scavengers (`Squad_Ger_Medic`) | field defences build 35% faster |

Costs live on the research components (`Source/Tools/TechSetup/setup_tech_batch2.py`, `RESEARCH_HOSTS`).

Notes on the mechanics:

- Squad health: `ASquadController::ApplyTechMaxHealthMultiplier` keeps a product of researched multipliers that
  `InitSquadData_SetValues` applies again when the squad finishes reinforcing (it reloads health from the game state
  data then). Never scale squad health per soldier only: reinforcement wipes it.
- Half-track passengers never took damage while carried; they died with the vehicle. The troop bay therefore lets
  them bail out (`UCargo::SetPassengersBailOutOnOwnerDeath`) instead of the originally proposed "50% less damage
  inside", which would have done nothing.
- The 88's ground role is a `UModeAbilityComponent` the tech adds at runtime (`InitModeAbilitySettings` before
  registering). Its button rows are `IdActivateMode` / `IdDisableMode`, custom type 2 (`EModeAbilityType::Flak88GroundRole`).
- Weapon behaviours filter by calibre so machine guns and crew rifles are not changed (`MinimalWeaponCalibreToApplyTo`).

## Checklist: adding a technology

1. C++: append to `ETechnology` and both string switches in `Technologies.h`; append an `EBehaviourIcon`; add a
   `UTE_*` class (subtypes in the constructor; weapon techs derive from `UTE_TurretWeaponUpgradeBase`).
2. Assets in `/Game/RTS_Survival/Blueprints/TechTree/GerTechs/<Name>/`: `BP_Beh_Tech_<Name>` (buff icon, title, text;
   `UBehaviourWeapon` with multipliers and a calibre filter for weapon techs) and `TEBP_Ger_<Name>` with
   `BehaviourClassToApply` set.
3. `Buttons_DataTableb`, row `IdResearchTechnology`: `CustomTypeMapping[<enum value>]` = research title, description, icon.
4. `DA_BehaviourUI`: `BehaviourIconStyles[<EBehaviourIcon>]` = icon texture (the buff icon on the unit).
5. A `ResearchTechnologyAbilityComp` on the host building expansion, one entry per faction.
6. `RTS_BP_PlayerController` → function `GetPlayerTechs` → Make Map: `<ETechnology>` → `TEBP` class. Python cannot
   grow a Make Map node; use the editor command `RTS.TechSetup.AddMakeMapEntries` (TechTree/Editor).
7. Icons: `D:\Blender\BlenderProjects\BlenderTanks\AbilityIcons\Technologies\tools\compose_tech_icons.py`
   (hex base + in-engine render + glyph), imported to `/Game/RTS_Survival/Textures/GameUI/TechTree/GerTech`.

Steps 2 to 6 for this batch: `Source/Tools/TechSetup/setup_tech_batch2.py` (headless, idempotent; steps `icons`,
`assets`, `buttons`, `buffs`, `research`, `techmap`). Saves fail while another editor has the asset loaded; the
script reports each failure.

## Test

`Source/Tools/TechSetup/run_tech_test.sh` runs `UTechBatchTwoTestSubsystem` (`-TechBatchTwoValidate`, compiled only
with `RTS_WITH_MAP_TESTS`) on RTS_ToCopySmall: it spawns one unit per affected family through the async spawner,
researches the techs through `UPlayerTechManager` and checks every effect (45 checks), including that health survives a
reinforcement re-init, Vultures stay unchanged, machine guns are untouched, the Flak mode applies and restores, and
half-track passengers bail out at 50%. If the controller map lacks a tech the test applies the TEBP directly and
prints `path=direct`.
