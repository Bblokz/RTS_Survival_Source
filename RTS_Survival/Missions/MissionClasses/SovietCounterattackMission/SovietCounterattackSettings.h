// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "SovietCounterattackSettings.generated.h"

class ASovietCargoTrain;
class UStaticMesh;
class UW_MissionTimer;

/**
 * Everything of the counterattack that changes with the difficulty. All times are whole seconds.
 * Wave N (counting from 1) has a random unit count in [MinUnitsFirstWave, MaxUnitsFirstWave] + (N - 1) * UnitsAddedPerWave.
 */
USTRUCT(BlueprintType)
struct FSovietCounterattackDifficulty
{
	GENERATED_BODY()

	// "Prepare to defend": seconds from the mission start until the first wave.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0", UIMin = "0"))
	int32 SecondsUntilSovietsArrive = 180;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0", UIMin = "0"))
	int32 WaveCount = 4;

	// Seconds between two waves; the "Survive" timer runs WaveCount x this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "1", UIMin = "1"))
	int32 SecondsBetweenWaves = 90;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0", UIMin = "0"))
	int32 MinUnitsFirstWave = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxUnitsFirstWave = 12;

	// Added to both bounds for every wave after the first (bounds 10-12 with 1: wave 2 is 11-13, wave 3 is 12-14).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	int32 UnitsAddedPerWave = 1;

	// The pool every spawn (centre, landing field, train) draws from; each unit picks one entry of the tanks and
	// squads together at random, so list an entry twice to make it more common.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units")
	TArray<ETankSubtype> TankTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units")
	TArray<ESquadSubtype> SquadTypes;
};

/** Where the three spawns are and how far each spawn's groups stay apart on the shared attack route. */
USTRUCT(BlueprintType)
struct FSovietCounterattackSpawnSettings
{
	GENERATED_BODY()

	FSovietCounterattackSpawnSettings();

	// Centre spawn: units spawn on a grid around this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Centre")
	FVector CenterSpawnLocation = FVector(8400.f, -1500.f, 296.f);

	// Train spawn: once the train stands at the station its units spawn on a grid around this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	FVector StationSpawnLocation = FVector(19953.f, 15593.f, 943.f);

	// Landing field: one Pe-8 lands on each pad centre (the pads cannot be destroyed). A batch flies one unit per
	// pad; the next batch comes once every aircraft of the batch has delivered and left.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing Field")
	TArray<FVector> LandingPadCenters;

	// Distance between two units of a ground spawn grid.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "300.0"))
	float SpawnSpacing = 800.f;

	// Added (X, Y) to every attack route point for that spawn's groups, so groups do not all drive to one point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Offsets")
	FVector2D CenterRouteOffset = FVector2D(0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Offsets")
	FVector2D TrainRouteOffset = FVector2D(700.f, 900.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Offsets")
	FVector2D AirDropRouteOffset = FVector2D(-700.f, -900.f);
};

/** The landed cargo plane drops of the landing field. */
USTRUCT(BlueprintType)
struct FSovietCounterattackAirDropSettings
{
	GENERATED_BODY()

	// Aircraft drop ability whose Pe-8 flies the units (copied from the global ability data asset).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	EGlobalAbility CargoPlaneAbility = EGlobalAbility::GA_SovietT34Drop;

	// Vehicles hang on ropes under the Pe-8 and are lowered onto the pad; squads come in a Pe-8 that lands.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	bool bRopeSlungVehicles = true;

	// The announcer warns of the first aircraft of every batch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	bool bWarnPlayer = true;

	// Seconds between the take-offs of the aircraft of one batch, so they do not arrive wingtip to wingtip.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop", meta = (ClampMin = "0.0"))
	float StaggerSeconds = 1.5f;
};

/** The cargo train that brings the station spawn's units. */
USTRUCT(BlueprintType)
struct FSovietCounterattackTrainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	TSubclassOf<ASovietCargoTrain> TrainClass;

	// World points of the train's rails: the north map edge first, the station last (REAR_Rail_Main down to the
	// junction, then REAR_Rail_Siding; Source/Tools/IntroMission/counterattack/train_path.json).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	TArray<FVector> PathPoints;

	// The locomotive stops at the path point closest to this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	FVector StopFrontLocation = FVector::ZeroVector;

	// Static tank meshes on the flatcars, one per vehicle the train brings, picked in turn from this list.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	TArray<TObjectPtr<UStaticMesh>> CargoTankMeshes;

	// Seconds between the train stopping and the cargo meshes turning into units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train", meta = (ClampMin = "0.0"))
	float UnloadDelaySeconds = 1.f;

	// Seconds the empty train waits at the station before it drives back out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train", meta = (ClampMin = "0.0"))
	float DepartDelaySeconds = 6.f;
};

/**
 * @brief Data of USovietCounterattackMission: per difficulty the time until the Soviets arrive, the waves and their
 * unit pool; plus the three spawns, the attack route, the timer and the objective texts.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USovietCounterattackSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	USovietCounterattackSettings();

	const FSovietCounterattackDifficulty& GetDifficultySettings(const ERTSGameDifficulty Difficulty) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", DisplayName = "New To RTS")
	FSovietCounterattackDifficulty NewToRTS;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FSovietCounterattackDifficulty Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FSovietCounterattackDifficulty Hard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FSovietCounterattackDifficulty Brutal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FSovietCounterattackDifficulty Ironman;

	// Attack-move route of every group, each spawn with its own offset; the player HQ (at the time the group sets
	// off) is added as the last point.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	TArray<FVector> AttackRoute;

	// Last route point when the player has no HQ.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	FVector FallbackHQLocation = FVector(1450.f, 300.f, 600.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "1"))
	int32 MaxFormationWidth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "0.1"))
	float FormationOffsetMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route")
	FAttackMoveWaveSettings AttackMoveSettings;

	// Seconds between a group's last unit appearing and its attack-move order (fresh squads drop early orders).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (ClampMin = "0.0"))
	float OrderDelaySeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawns")
	FSovietCounterattackSpawnSettings Spawns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawns")
	FSovietCounterattackAirDropSettings AirDrop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawns")
	FSovietCounterattackTrainSettings Train;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UW_MissionTimer> TimerWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText PrepareTimerText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText SurviveTimerText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText PrepareObjectiveTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (MultiLine = "true"))
	FText PrepareObjectiveDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText SurviveObjectiveTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (MultiLine = "true"))
	FText SurviveObjectiveDescription;

	// Shown with the number of Soviet units still alive once the Survive timer ran out.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText RemainingObjectiveTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (MultiLine = "true"))
	FText RemainingObjectiveDescription;

	// Incoming groups show as survival-style division markers on the minimap while the waves come.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bMarkAttackGroupsOnMiniMap = true;

	// Once the Survive timer ran out every remaining Soviet unit gets this icon on the minimap.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	EMinimapIconType RemainingUnitIcon = EMinimapIconType::AttackTarget;
};
