// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietCounterattackSettings.h"

#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Missions/MissionProps/SovietCargoTrain/SovietCargoTrain.h"
#include "LevelSequence.h"
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

namespace SovietCounterattackDefaults
{
	FMissionCameraTourShot MakeShot(const FVector& FocusStart, const FVector& FocusEnd, const float YawStart,
	                                const float YawEnd, const float PitchStart, const float PitchEnd,
	                                const float ArmStart, const float ArmEnd, const float Seconds,
	                                const float BlendInSeconds, const FText& Title, const FText& Subtitle)
	{
		FMissionCameraTourShot Shot;
		Shot.FocusStart = FocusStart;
		Shot.FocusEnd = FocusEnd;
		Shot.YawStart = YawStart;
		Shot.YawEnd = YawEnd;
		Shot.PitchStart = PitchStart;
		Shot.PitchEnd = PitchEnd;
		Shot.ArmLengthStart = ArmStart;
		Shot.ArmLengthEnd = ArmEnd;
		Shot.Seconds = Seconds;
		Shot.BlendInSeconds = BlendInSeconds;
		Shot.Title = Title;
		Shot.Subtitle = Subtitle;
		return Shot;
	}
}

FSovietCounterattackDigInIntroSettings::FSovietCounterattackDigInIntroSettings()
{
	using SovietCounterattackDefaults::MakeShot;
	// MarkerClass is set on the data asset (not here) so the cook sees the reference.
	// The intro map's two ridges either side of the Soviet attack route: south ridge first, then the north ridge.
	MarkerLocations = {
		FVector(6105.f, -3617.f, 791.f),
		FVector(7085.f, -3679.f, 791.f),
		FVector(7149.f, 1386.f, 531.f),
		FVector(6351.f, 1143.f, 531.f)
	};
	MarkerText = LOCTEXT("DigInMarker", "Dig in a vehicle here");
	MarkerHeldText = LOCTEXT("DigInMarkerHeld", "Position held");
	PositionsObjectiveLine = LOCTEXT("DigInPositionsLine", "Vehicles dug in at the marked positions: {Held}/{Total}");
	FSovietCounterattackResourceReward Radixite;
	Radixite.Resource = ERTSResourceType::Resource_Radixite;
	Radixite.Amount = 300;
	FSovietCounterattackResourceReward Metal;
	Metal.Resource = ERTSResourceType::Resource_Metal;
	Metal.Amount = 150;
	AllPositionsHeldReward = {Radixite, Metal};
	const FText PositionsTitle = LOCTEXT("TourPositionsTitle", "Dig-In Positions");
	TourShots = {
		MakeShot(FVector(6250.f, -3950.f, 791.f), FVector(6700.f, -3600.f, 791.f), 20.f, 5.f, 18.f, 14.f,
		         3000.f, 2600.f, 5.5f, 1.5f, PositionsTitle, LOCTEXT("TourSouthRidge", "Southern ridge")),
		MakeShot(FVector(6500.f, 1000.f, 531.f), FVector(6850.f, 1350.f, 531.f), -5.f, -20.f, 14.f, 18.f,
		         2600.f, 3000.f, 5.5f, 1.75f, PositionsTitle, LOCTEXT("TourNorthRidge", "Northern ridge")),
		MakeShot(FVector(6300.f, -1200.f, 600.f), FVector(7300.f, -1200.f, 600.f), 0.f, 0.f, 6.f, 4.f,
		         4800.f, 5000.f, 5.f, 2.f, LOCTEXT("TourGapTitle", "Hold the Gap"),
		         LOCTEXT("TourGapSubtitle", "The Soviet attack comes between both ridges"))
	};
}

FSovietCounterattackArrivalCinematicSettings::FSovietCounterattackArrivalCinematicSettings()
{
	// Sequence is set on the data asset (not here) so the cook sees the reference.
}

FSovietCounterattackBaseReinforcementSettings::FSovietCounterattackBaseReinforcementSettings()
{
	// User, 2026-11-23: two Tiger H1 + three Luchs (Breakthrough), two Panther D + two Pz III AA (Strike Division).
	BreakthroughVehicles = {
		ETankSubtype::Tank_TigerH1, ETankSubtype::Tank_TigerH1,
		ETankSubtype::Tank_PzII_Luchs, ETankSubtype::Tank_PzII_Luchs, ETankSubtype::Tank_PzII_Luchs
	};
	StrikeDivisionVehicles = {
		ETankSubtype::Tank_PantherD, ETankSubtype::Tank_PantherD,
		ETankSubtype::Tank_PzIII_AA, ETankSubtype::Tank_PzIII_AA
	};
	// VoiceLine is set on the data asset (not here) so the cook sees the reference.
}

USovietCounterattackSettings::USovietCounterattackSettings()
{
	using namespace SovietCounterattackDefaults;
	NewToRTS = Make(150, 3, 70, 6, 8, 1,
	                {ETankSubtype::Tank_BT7, ETankSubtype::Tank_T26, ETankSubtype::Tank_T70},
	                {ESquadSubtype::Squad_Rus_Mosin});
	Normal = Make(150, 4, 60, 9, 12, 1,
	              {ETankSubtype::Tank_T26, ETankSubtype::Tank_BT7_4, ETankSubtype::Tank_T70, ETankSubtype::Tank_T34_76},
	              {ESquadSubtype::Squad_Rus_Mosin, ESquadSubtype::Squad_Rus_Okhotnik});
	Hard = Make(120, 5, 60, 10, 13, 2,
	            {
		            ETankSubtype::Tank_T70, ETankSubtype::Tank_T34_76, ETankSubtype::Tank_T34_76_L,
		            ETankSubtype::Tank_T34E, ETankSubtype::Tank_SU_76
	            },
	            {ESquadSubtype::Squad_Rus_Mosin, ESquadSubtype::Squad_Rus_Okhotnik, ESquadSubtype::Squad_Rus_LargePTRS});
	Brutal = Make(90, 6, 50, 12, 15, 2,
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
