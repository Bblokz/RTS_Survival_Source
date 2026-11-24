# Enemy air warnings

When the enemy fires an air global ability, the Advisor (announcer portrait, with subtitles) warns the player.

- Hook: `UGlobalAbilitiesManager::TryExecuteAbilityAtLocation` calls `UEnemyAirWarningSubsystem::OnEnemyAbilityExecuted`
  after an enemy (`IsEnemyAbilityManager`) manager fires an ability. That covers the strategic AI and the survival wave
  director. Non-air abilities stay silent.
- Settings: Project Settings > RTS Enemy Air Warnings (`UEnemyAirWarningSettings`, `[/Script/RTS_Survival.EnemyAirWarningSettings]`
  in DefaultGame.ini). Each warning lists the abilities that trigger it, the portrait and its lines (sound waves):

| Warning | Abilities | Lines |
|---|---|---|
| ScoutAircraft | GA_SovietReconFlight | EnemyAir_Scout_01..03 |
| AttackAircraft | GA_2xPE2Strafing, GA_4xPE2Strafing | EnemyAir_Attack_01..04 |
| ParatrooperPlane | GA_SovietParatroopers | EnemyAir_Paratroopers_01..05 |
| Bombers | GA_HeavyCarpet_PE8_500Gr / 1000Gr / 3000Gr | EnemyAir_Bombers_01..03 |

- Lines play from a shuffled deck per warning (every line once before repeats). The same warning is silent for
  `SameWarningCooldownSeconds` (45 s), and any warning is silent for `MinSecondsBetweenWarnings` (12 s) after another,
  because the portrait queues lines and back-to-back abilities would stack late warnings.
- Audio: `/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/EnemyAircraft` (always cooked; the
  settings are the only reference). Sources and sidecars in `BlenderTanks/VoiceLines/Announcer/AAAGerMissions/EnemyAircraft`;
  takes and tools in `VoiceLines/VoiceChanger/Output/EnemyAircraftAudition` (eleven_v4, AnnouncerFilter with glitch 0).
- Test: `-EnemyAirWarningsValidate` on `UT_EnemyAI` (`UEnemyAirWarningTestSubsystem`) checks the settings, the ability
  mapping, a played line and both cooldowns; results in `Saved/EnemyAirWarningTest/results.txt`. UT_EnemyAI's enemy has
  no air ability loaded, so there it drives the subsystem directly rather than through the manager hook.
