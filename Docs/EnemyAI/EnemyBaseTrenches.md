# Enemy base trenches

The enemy AI digs Soviet field trenches at its bases. It digs a trench out of every free trench anchor of its pits, and fire trench sections on an arc in front of each base, facing the player. It is its own action: it does not go through the strategic decision tree and needs no units. The trenches dig themselves, like the player's.

Code: `Enemy/EnemyController/EnemyBaseTrenchComponent` (`UEnemyBaseTrenchComponent`, settings `FEnemyBaseTrenchSettings`). The component is created by `AEnemyController` as `EnemyBaseTrenchComponent` (getter `GetEnemyBaseTrenchComponent`).

## When it runs

- **Start:** `UEnemyStrategicAIComponent::StartStrategicAIThinkingTimer` calls `StartBaseTrenchDigging`, so trenches start with the strategic AI's thinking, whatever its start condition.
  - In survival that is the grace period (`USurvivalStrategicAICoordinator`, direct control still off), so the Soviet bases dig in before the waves come.
  - Blueprints and missions can also call `StartBaseTrenchDigging` and `StopBaseTrenchDigging`.
- **Think step:** every `ThinkIntervalSeconds` (own timer). A base starts at most one trench per think step, and then waits `DigCooldownSeconds`.
- **Off:** `Settings.bEnabled = false` on the map's enemy controller.

## Which bases

1. **The strategic AI's base clusters:** `Blackboard.EnemyBasePoints`, from `FindEnemyBase_TimerRequest`. The arc's radius is the distance from the base centre to the farthest core building, plus `PerimeterMargin`, clamped to `MinimumPerimeterRadius`..`MaximumPerimeterRadius`. A base keeps its arc once the arc is planned.
2. **`AddFortifiedBase(Centre, PerimeterRadius, ThreatDirection)`:** for a map or mission spot the cluster search does not find. A zero direction faces the player's HQ.
3. **Pits far from every base** (`BaseGroupingRadius`) form a pit-only base. It digs only out of its pits and gets no arc.
   - A base from 1 or 2 that later appears within `BaseGroupingRadius` takes such a base in, with its trench count and metres (`AbsorbPitOnlyBases`).
   - This happens when the AI starts thinking before its base search has found the base.

## What it digs, in order

1. **Pit trenches:** for each free anchor of player 2 (`UFieldTrenchAnchorSubsystem::GetFreeAnchorsOfPlayer`), a straight trench of `PitTrenchLength` out of the port.
   - It is planned from the anchor (`FFieldTrenchPlanOptions::StartPort`), so it joins the pit with no cap there.
   - When its first piece is dug, the pit's neck shows, exactly as for the player's trench.
   - An anchor whose trench does not fit is not tried again.
2. **Arc sections:** the arc spans `PerimeterArcDegrees` and is centred on the player's HQ (`Blackboard.CurrentPlayerUnitCounts.PlayerHQLocation`), or on the base's own direction.
   - It is cut into sections of `PerimeterSectionLength`, with `PerimeterGapLength` gaps for units and vehicles to pass.
   - Sections are dug from the middle outwards, so the side facing the player comes first.
   - The arc is planned once the HQ is known. A section that does not fit at all is skipped.

Every trench is planned the way the player's placement plans one (`PlanBuildablePieces`, the AI's copy of `PlanBuildableTrench`), using `ARTSFieldTrench::PlanOnGround`. It is planned again, shorter, until every piece fits. A piece does not fit when:
- it leans more than `MaximumSlopeDegrees`;
- it lies further from the base centre than the arc radius (or `PitTrenchLength` for a pit-only base) plus `BuildRadiusMargin`. This is the AI's build radius.
- it overlaps anything that is not a pawn or the landscape: buildings, walls, props, other trenches. The check is a box of the trench's width, from 60 cm to 260 cm above the piece; the joined pit is ignored.
- it comes within `MinimumTrenchSpacing` of a piece of another AI trench. Planned pieces have no collision yet, so this is measured by distance.

## The trench

- `ARTSFieldTrench` (C++, no Blueprint), owned by player 2, with:
  - `SetKit(EFieldTrenchKit::Soviet)`;
  - `SetDigSecondsPerMetre(DigSecondsPerMetre)`;
  - `SetHidePlannedPieces(bHidePlannedPieces)`. Planned pieces stay hidden, so the player does not see the plan; a piece appears when its digging starts.
- **Free:** the AI has no resource pool (`FEnemyResources` only holds `WaveSupply`), so the trench gets an empty cost (`StartConstruction(Pieces, 2, {})`). `MaxTrenchesPerBase`, `MaxTrenchMetresPerBase`, `MaxTrenches` and the cooldown limit it instead.
- Cover, the anchor claim and the cap when the pit is lost all work as for the player's trench (see "Trench anchors" in [../UI/FieldDefenses.md](../UI/FieldDefenses.md)). Trenches never affect navigation: units walk through them.
- The player's HQ trench is unaffected and always German ([../Units/SovPitTrenchAnchors.md](../Units/SovPitTrenchAnchors.md)).

## Tuning

Edit `EnemyBaseTrenchComponent > Settings` on the map's `RTS_BP_EnemyController` instance (or its Blueprint defaults).

| Setting | Default | What |
|---|---|---|
| `bEnabled` | true | Off: no trenches on this map |
| `ThinkIntervalSeconds` | 5 s | How often a trench is looked for |
| `DigCooldownSeconds` | 20 s | Per base, between two trench starts |
| `MaxTrenchesPerBase` | 10 | |
| `MaxTrenchMetresPerBase` | 160 m | Budget of one base |
| `MaxTrenches` | 60 | All bases together |
| `DigSecondsPerMetre` | 1.5 | The player's trench: 1 |
| `MaximumSlopeDegrees` | 15° | As the player's placement |
| `PitTrenchLength` | 1200 cm | Out of each pit anchor |
| `BaseGroupingRadius` | 8000 cm | A pit belongs to a base within this |
| `bDigPerimeterTrenches` | true | The arc in front of the base |
| `PerimeterArcDegrees` | 140° | Centred on the player's HQ |
| `PerimeterSectionLength` / `PerimeterGapLength` | 1800 / 700 cm | |
| `PerimeterMargin` | 1500 cm | Beyond the farthest core building |
| `MinimumPerimeterRadius` / `MaximumPerimeterRadius` | 2500 / 9000 cm | |
| `BuildRadiusMargin` | 1500 cm | The AI's build radius beyond the arc |
| `MinimumTrenchSpacing` | 450 cm | From the AI's other trenches |
| `bHidePlannedPieces` | true | The plan stays hidden until dug |

## Testing

`-FieldDefenseValidateEnemyTrenches` on `UT_Nomadics` (`UFieldDefenseTestSubsystem`, log `RTS_FIELD_DEFENSE_TEST`):
- The test base is set up like this:
  - the six Soviet pits are placed as in a level, 150 m from the German HQ, out of gun range, on the first flat ground found;
  - they keep their Blueprint owner (player 2), and their anchors face the base centre;
  - the test adds the base with `AddFortifiedBase` (arc radius 50 m, facing the HQ);
  - it starts the digging through `StartStrategicAIThinkingTimer`. UT_Nomadics already starts its strategic AI at BeginPlay, so the digging is restarted with fast test settings.
- The checks:
  - each pit's anchor is on its socket and owned by player 2;
  - every pit gets a dug trench of Soviet pieces, joined to its anchor, and its neck shows;
  - at least 3 arc sections are dug in the Soviet kit, within the build radius;
  - the base stays within its caps, and all its trenches count towards it;
  - no planned piece is ever shown;
  - the player's HQ trench, started beside it, stays German.

```bash
MSYS_NO_PATHCONV=1 UnrealEditor-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_Nomadics -game -unattended -nosound -RenderOffscreen -windowed -ResX=1920 -ResY=1080 -benchmark -fps=30 -DisablePlugins=Fab -FieldDefenseValidateEnemyTrenches
```

Result on 2026-11-09 (DebugGame): PASS, 0 failed. 10 trenches (6 pits, 4 of 5 arc sections), 204 m.

## Not verified yet

- A real survival match: whether the generated Soviet bases' pits offer their anchors and dig, and how the arc sits against walls, roads and gates. Watch with `-SovietBaseWatch` (see `SurvivalMode/Docs/50_SovietBases.md`).
- How it looks: the test checks meshes and visibility, not pictures. `-FieldDefenseCapture=<folder>` in a visible window takes them.
- Arc sections can cross painted roads, which have no collision.
