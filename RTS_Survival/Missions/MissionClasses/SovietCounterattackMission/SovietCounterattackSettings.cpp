// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietCounterattackSettings.h"

#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Missions/MissionProps/SovietCargoTrain/SovietCargoTrain.h"
#include "RTS_Survival/Missions/MissionWidgets/W_MissionTimer.h"

#define LOCTEXT_NAMESPACE "SovietCounterattack"

namespace SovietCounterattackDefaults
{
	FSovietCounterattackDifficulty Make(const int32 Prepare, const int32 Waves, const int32 Between, const int32 Min,
	                                    const int32 Max, const int32 Added, const TArray<ETankSubtype>& Tanks,
	                                    const TArray<ESquadSubtype>& Squads)
	{
		FSovietCounterattackDifficulty Difficulty;
		Difficulty.SecondsUntilSovietsArrive = Prepare;
		Difficulty.WaveCount = Waves;
		Difficulty.SecondsBetweenWaves = Between;
		Difficulty.MinUnitsFirstWave = Min;
		Difficulty.MaxUnitsFirstWave = Max;
		Difficulty.UnitsAddedPerWave = Added;
		Difficulty.TankTypes = Tanks;
		Difficulty.SquadTypes = Squads;
		return Difficulty;
	}
}

FSovietCounterattackSpawnSettings::FSovietCounterattackSpawnSettings()
{
	// The four Pe-8 pads of the intro map's landing field (Source/Docs/Campaign/IntroMissionRear.md); z 0 = the
	// ground under the centre is traced at runtime.
	LandingPadCenters = {
		FVector(20446.f, -16317.f, 0.f),
		FVector(19673.f, -13939.f, 0.f),
		FVector(16927.f, -17461.f, 0.f),
		FVector(16154.f, -15083.f, 0.f)
	};
}

USovietCounterattackSettings::USovietCounterattackSettings()
{
	using namespace SovietCounterattackDefaults;
	NewToRTS = Make(240, 3, 100, 6, 8, 1,
	                {ETankSubtype::Tank_BT7, ETankSubtype::Tank_T26, ETankSubtype::Tank_T70},
	                {ESquadSubtype::Squad_Rus_Mosin});
	Normal = Make(180, 4, 90, 9, 12, 1,
	              {ETankSubtype::Tank_T26, ETankSubtype::Tank_BT7_4, ETankSubtype::Tank_T70, ETankSubtype::Tank_T34_76},
	              {ESquadSubtype::Squad_Rus_Mosin, ESquadSubtype::Squad_Rus_Okhotnik});
	Hard = Make(150, 5, 90, 10, 13, 2,
	            {
		            ETankSubtype::Tank_T70, ETankSubtype::Tank_T34_76, ETankSubtype::Tank_T34_76_L,
		            ETankSubtype::Tank_T34E, ETankSubtype::Tank_SU_76
	            },
	            {ESquadSubtype::Squad_Rus_Mosin, ESquadSubtype::Squad_Rus_Okhotnik, ESquadSubtype::Squad_Rus_LargePTRS});
	Brutal = Make(120, 6, 80, 12, 15, 2,
	              {
		              ETankSubtype::Tank_T34_76_L, ETankSubtype::Tank_T34E, ETankSubtype::Tank_T34_85,
		              ETankSubtype::Tank_SU_85, ETankSubtype::Tank_KV_1
	              },
	              {
		              ESquadSubtype::Squad_Rus_RedHammer, ESquadSubtype::Squad_Rus_LargePTRS,
		              ESquadSubtype::Squad_Rus_Okhotnik
	              });
	Ironman = Brutal;

	AttackRoute = {FVector(8400.f, -1500.f, 296.09375f), FVector(4137.f, -1752.f, 532.f)};

	AttackMoveSettings.HelpOffsetRadiusMltMax = 1.2f;
	AttackMoveSettings.HelpOffsetRadiusMltMin = 1.0f;
	// Keep pushing on to the HQ instead of holding a waypoint for as long as anyone there fights.
	AttackMoveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint = 25.f;

	PrepareTimerText = LOCTEXT("PrepareTimer", "Prepare to defend");
	SurviveTimerText = LOCTEXT("SurviveTimer", "Survive");
	PrepareObjectiveTitle = LOCTEXT("PrepareTitle", "Prepare to Defend");
	PrepareObjectiveDescription = LOCTEXT("PrepareDescription",
	                                      "The Soviets will try to retake the ridge. Prepare your defences before "
	                                      "their attack begins.");
	SurviveObjectiveTitle = LOCTEXT("SurviveTitle", "Survive");
	SurviveObjectiveDescription = LOCTEXT("SurviveDescription",
	                                      "Soviet forces attack from the train station, the landing field and the "
	                                      "east. Hold out until the timer runs out.");
	RemainingObjectiveTitle = LOCTEXT("RemainingTitle", "Kill remaining Soviet forces");
	RemainingObjectiveDescription = LOCTEXT("RemainingDescription",
	                                        "The Soviet attack is broken. Destroy the Soviet units still on the map; "
	                                        "they are marked on the minimap.");
}

const FSovietCounterattackDifficulty& USovietCounterattackSettings::GetDifficultySettings(
	const ERTSGameDifficulty Difficulty) const
{
	switch (Difficulty)
	{
	case ERTSGameDifficulty::NewToRTS:
		return NewToRTS;
	case ERTSGameDifficulty::Normal:
		return Normal;
	case ERTSGameDifficulty::Hard:
		return Hard;
	case ERTSGameDifficulty::Brutal:
		return Brutal;
	case ERTSGameDifficulty::Ironman:
		return Ironman;
	default:
		return Normal;
	}
}

#undef LOCTEXT_NAMESPACE
