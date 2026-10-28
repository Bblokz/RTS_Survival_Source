# Paratrooper meadow shorts

The independent presets are `SovietParatrooperBattle`,
`GermanTwinBomberParatrooperBattle` and `GermanPanzergrenadierArtilleryBattle`
in `ShortsCreator/Jobs`. They use the saved
`ParatrooperMeadow` section of `RTS_Shorts`, render at 1080 × 1920 / 60 fps,
and stop at the battle's end or 120 seconds. The stage's temporary double-range
behaviour applies to each participating vehicle, squad and gun pit.

`create_german_preset.py` rebuilds only the German preset. Its armour uses the
regular Soviet vehicle Blueprints. Germany owns the two JU 390 passes and each
pass uses the existing German ability's single Vulture squad. Shared ability
templates, aircraft assets, scenery and other job presets are not edited.

`create_panzergrenadier_preset.py` builds the artillery variant with two
Panzergrenadier drops, Tiger H1s, Panther Ds and 150 mm sFH18 pits against
regular T-34-76s, Forge IS-1s with HEAT and the B-4 203 mm rear pit. The project
has separate leFH18 and sFH18 assets; this variant uses sFH18 to match 150 mm.

## Three-battle queue

`create_balanced_batch.py` prepares three independent five-tank-per-side jobs:

- `ParatrooperFair01_GermanVultures_Mediums`: two German Vulture drops;
  Panther D/Panzer IV G against regular and HEAT T-34-85 groups.
- `ParatrooperFair02_SovietMosin_HeavyArmour`: two Soviet Mosin drops;
  Tiger H1/Panther G/Panzer IV H against T-34-85, IS-1 (HEAT), IS-2 and SU-85 (APCR).
- `ParatrooperFair03_SovietROKS_Assault`: two Soviet ROKS assault drops;
  Panzer III J/StuG III against regular T-34E and T-70 (APCR).

Each side has four squads including its drops and one rear gun pit. These are
roughly matched compositions, not a promise of equal outcomes. All retain the
approved camera route, two-second aircraft intro, full HD at 60 fps, and 120-second cap.

`install_rounds_integration.py` installs the existing `forge_rounds.py` tool
from the other project into RTS_Survival. The universal `run_shorts.launch`
hook annotates every render, including paratrooper battles, before writing
the native job snapshot. It verifies cached Blueprint hashes and refreshes
the round table when needed. Mixed regular/special groups are matched even
when faction headings and aircraft/drop rows appear in the column.
`test_balanced_batch.py` checks the actual launcher path and these three jobs.

To keep existing queue histories independent, start RTS_Survival's dashboard
with `--port 8766 --state-dir ShortsCreator/Dashboard/ParatrooperQueueData`
(use an absolute state path when launching from another working directory).
`queue_balanced_batch.py` enqueues the three jobs once, without duplicates.
The current queue is at <http://127.0.0.1:8766/>.

## Formation controls

An optional segment field allows several flights or a drop belonging to the
opposing army:

```json
"ParatrooperFormation": {
  "OwningPlayer": 1,
  "LandingTrailLength": 7200,
  "JumpIntervalSeconds": 0.8,
  "Passes": [
    {
      "DropOffset": {"X": 7200, "Y": -900, "Z": 0},
      "AircraftStartOffset": {"X": 500, "Y": 0, "Z": 3500}
    },
    {
      "DropOffset": {"X": 7200, "Y": 900, "Z": 0},
      "AircraftStartOffset": {"X": 500, "Y": 0, "Z": 4300}
    }
  ]
}
```

Distances are centimetres. `DropOffset` is relative to the stage actor;
`AircraftStartOffset` is relative to that drop point. Each pass executes the
configured real global ability once. The camera frames both aircraft for the
two-second intro, then follows the battle camera route.

`LandingTrailLength` distributes the soldiers' landing points from the rear
drop point along the flight direction. The existing spiral adds local spacing,
and each point is projected onto navigation and traced to the landscape.
`JumpIntervalSeconds` controls the time between individual jumps.
An optional `SquadSubtypes` array overrides the squad list on every pass, for
example `["Squad_Ger_PanzerGrenadiere"]` drops one Panzergrenadier squad from
each aircraft. An absent or empty array inherits the ability template's list.

Omitting the formation preserves the original single flight over the midpoint,
the job's owning player, the template's jump interval and circular landing zone.
Zero values also inherit those defaults. Overrides apply only to the private
ability copy owned by the render's director. Drop controllers and their soldiers
are absorbed into the correct battle side so targeting, cover, victory checks
and range modifiers continue to work.

The JU 390 mesh has a checker material in its propeller slot. That triggers the
Shorts renderer's placeholder cleanup for the entire mesh. Capture preparation
replaces only that runtime slot with the existing German propeller blade material,
then keeps the aircraft pose updating offscreen. Neither the original mesh nor
its Blueprint or materials are saved or modified.

Run `test_preset.py` for preset validation. `audit_german_render.py TAKE_FOLDER`
checks the delivered video against the actual aircraft, jump, ownership,
placement, range and camera events in the render log.
`audit_panzergrenadier_render.py TAKE_FOLDER` checks the artillery variant,
including both three-man Panzergrenadier drops and all three gun pits firing.

If actual capture frames show a white meadow despite intact material inputs,
`refresh_ground_material.py` backs up and recompiles the existing dedicated
ground material. Run it in an isolated editor commandlet with
`-run=pythonscript -AllowCommandletRendering`; its graph and RVT connections are
preserved, and the map is not saved. The first recovery copy remains under
`Before/GroundShaderRefresh`.

Inspect the actual captured JPGs for visual review. On this workstation the UECP
`render_anim_sequence_thumbnail` action crashed `UECPAnimationExt`; avoid that
preview action and preserve any existing editor recovery files.
