# Soviet megaphone broadcasts (`UMegaphoneBroadcastComponent`)

A megaphone that keeps broadcasting random Russian lines until it is destroyed. Built 2026-11-16 for
`Ger_FirstMission`, and made to drop into any level.

## Use it on a level

| You want | Place |
|---|---|
| Generic Directorate broadcasts | `Rus_Bxp_Megaphone_Broadcast` |
| The same, plus the "fuel ration cut" notice (comes up about 1 in 4) | `Rus_Bxp_Megaphone_Broadcast_FuelRation` |
| A silent megaphone, or the alarm-system megaphone (the `Ger_FirstMission` depot) | `Rus_Bxp_Megaphone` (no line sets) |

All three are in `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/`. To give one placed
megaphone a different mix, select it, then its `MegaphoneBroadcast` component, and edit `Line Sets` in the Details
panel. A new theme is a new `UMegaphoneLineSet` data asset (right-click > Miscellaneous > Data Asset >
MegaphoneLineSet).

## What it does

- From BeginPlay it waits `FirstLineDelayRange` (2-8 s), then plays one line and waits for the line's length plus
  `PauseBetweenLinesRange` (8-16 s). It then plays the next line, and so on.
- **Random, never back to back.** Each pick is weighted random over every line of every set. A set's `SelectionWeight`
  multiplies the chance of each of its lines. The line that just played is skipped whenever there is another one.
- **Stops when the megaphone dies.** It is bound to the owner's `OnUnitDies` (`IHealthBarOwner`). The line that is
  playing fades out in 0.15 s. A dead megaphone never starts again.
- **Stops for the siren.** `Rus_Bxp_Megaphone`'s `PlaySiren` event calls `StopBroadcast` before it starts the siren,
  so an alarm megaphone that also has lines goes quiet when the alarm sounds.
- **Silent while not standing.** While the owning expansion is being placed, built, packed or unpacked, it retries
  every 5 s. Map-placed megaphones never get an owner and broadcast normally.
- **Where the sound comes from.** The lines play from the component, which sits at the horn (z 480 on the 5.2 m
  post). They play with `Att_Propaganda` (30 m full volume, then a 70 m fall-off, not spatialised) in the
  `RTS_SClass_VoiceLine` class (the voice-line volume slider).
- **Out-of-earshot timing.** Lines are timed by wave length, not by `OnAudioFinished`, so a megaphone out of earshot
  keeps its rhythm.
- **Blueprint calls:** `StartBroadcast`, `StopBroadcast`, `SetLineSets`, `GetIsBroadcasting`. Leave
  `bStartOnBeginPlay` off to start one from a mission graph.
- **Logging:** `log LogMegaphoneBroadcast Verbose` prints a `MEGAPHONE <actor> plays <wave>` line for every line
  played.

## Code and assets

- C++:
  - `Source/RTS_Survival/Audio/MegaphoneBroadcast/MegaphoneBroadcastComponent.h/.cpp`
  - `MegaphoneLineSet.h/.cpp` (`FMegaphoneLine` = Sound + EnglishText)
- Line sets: `/Game/RTS_Survival/Sounds/VoiceLines/RusFaction/Propaganda/Megaphone/`
  - `DA_MegaphoneLines_Directorate`: 9 lines, weight 1;
  - `DA_MegaphoneLines_FuelRationCut`: 1 line, weight 3.
  - The waves are in the same folder.

## The lines (Russian, the English in the data asset)

| Wave | Russian | English |
|---|---|---|
| Megaphone_Directorate_WestSilent | Внимание, гарнизон Западного сектора. Директорат напоминает: Запад молчит. | Attention, Western Sector garrison. The Directorate reminds you: the West is silent. |
| Megaphone_Directorate_DisciplineIsLife | Дисциплина — это жизнь. Щит держит. | Discipline is life. The Shield holds. |
| Megaphone_Directorate_GuardChange | Смена караула в восемнадцать ноль-ноль. Дозиметры сдать дежурному. | Guard change at eighteen hundred. Hand your dosimeters to the duty officer. |
| Megaphone_Directorate_RiverWater | Напоминание: воду из реки не пить. Только из цистерны. | Reminder: do not drink the river water. Only water from the tanker. |
| Megaphone_Directorate_SuitCheck | Защитные костюмы проверять перед каждым выходом. Без исключений. | Check your protective suits before every patrol. No exceptions. |
| Megaphone_Directorate_OrderIsSurvival | Слава Директорату. Порядок — это выживание. | Glory to the Directorate. Order is survival. |
| Megaphone_Directorate_Rations | Паёк выдаётся по нормам Директората. Жалобы не принимаются. | Rations are issued by Directorate norms. Complaints will not be accepted. |
| Megaphone_Directorate_CrystalsForbidden | Внимание. Кристаллы не трогать. Нарушители будут наказаны. | Attention. Do not touch the crystals. Offenders will be punished. |
| Megaphone_Directorate_SectorCalm | Западный сектор спокоен. Бдительность не ослаблять. | The Western Sector is calm. Do not relax your vigilance. |
| Megaphone_FuelRationCut | Внимание. Колонна снабжения задерживается. Норма топлива сокращена до особого распоряжения. | Attention. The supply column is delayed. Fuel ration cut until further notice. |

## New lines

- Voice: ElevenLabs Alexandr Vlasov, Russian, eleven_v4, stability 0.5, measured official delivery.
- Treatment: "B · Across the yard", the user's pick on 2026-11-16 from `VoiceLines/VoiceChanger/Output/MegaphoneAudition`.
  - Horn band 420 Hz-3.5 kHz, drive, metallic horn comb.
  - Echoes off walls at 180/330/520/780 ms.
  - A 2.2 s outdoor tail.
- Steps:
  1. Add the line to `VoiceLines/VoiceChanger/megaphone_lines.json`.
  2. Run:

     ```bash
     python VoiceLines/VoiceChanger/megaphone_lines.py batch <name>
     ```

     It whisper-checks every take in Russian and remakes failures up to 3 times. It writes
     `BlenderTanks/VoiceLines/SovietPropaganda/Megaphone/<name>.wav` plus `_Old.wav`, `.script.txt` and
     `.translation.txt`.
  3. Import the wav into the folder above with sound class `RTS_SClass_VoiceLine`.
  4. Add it to a line set.

## Test

`Source/Tools/Megaphone/megaphone_broadcast_headless_test.py`: a throwaway editor on `RTS_ToCopySmall`, PIE, three
megaphones. It passed 12/12 on 2026-11-16:

- lines play, with no back-to-back repeats and several distinct lines;
- the plain megaphone stays silent;
- `PlaySiren` stops the broadcast, and nothing plays afterwards;
- death (`TriggerDestroyActor`) stops it, and nothing plays afterwards.

Traps found while testing:

- Editor-world edits to an SCS component instance did not survive the PIE copy; set test values on the PIE actor.
- `TakeFatalDamage` on a test-spawned BXP once reported "No valid hp component" and never killed it.
- Time the checks in game seconds.

## Placed

`Ger_FirstMission` (saved 2026-11-16 by `Source/Tools/Megaphone/place_first_mission_megaphones.py`, outliner folder
`Mission/Megaphones`):

- `MEGA_PatrolBase`: patrol base inside the west gate, at (500, -17700, 668).
- `MEGA_FinalCamp`: final-road camp west of the tents, at (12400, -30000, 291).

A map argument on the editor command line was ignored once (StartGame opened instead), so the script loads and checks
the map itself.
