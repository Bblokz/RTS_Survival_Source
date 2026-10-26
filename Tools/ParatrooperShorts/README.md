# Paratrooper meadow shorts

The two independent presets are `SovietParatrooperBattle` and
`GermanTwinBomberParatrooperBattle` in `ShortsCreator/Jobs`. Both use the saved
`ParatrooperMeadow` section of `RTS_Shorts`, render at 1080 × 1920 / 60 fps,
and stop at the battle's end or 120 seconds. The stage's temporary double-range
behaviour applies to each participating vehicle, squad and gun pit.

`create_german_preset.py` rebuilds only the German preset. Its armour uses the
regular Soviet vehicle Blueprints. Germany owns the two JU 390 passes and each
pass uses the existing German ability's single Vulture squad. Shared ability
templates, aircraft assets, scenery and other job presets are not edited.

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

Inspect the actual captured JPGs for visual review. On this workstation the UECP
`render_anim_sequence_thumbnail` action crashed `UECPAnimationExt`; avoid that
preview action and preserve any existing editor recovery files.
