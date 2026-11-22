// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ForgeStationTestSubsystem.generated.h"

class ABerlinForgeStation;
class AEnemyStrongpoint;
class UAnimMontage;
class UMission_BerlinForgeStation;

/**
 * @brief -ForgeStationValidate on Ger_Berlin: checks the Forge survey station bonus end to end. The garrison from the
 * difficulty's data asset, the target meshes, the dressing, the rim work circuits and their montage, the sedation
 * cycle, the trigger ring (a player unit is moved near), the wrecks when the targets die, the mission completion, the
 * archive entry and the reinforcement drop. Writes Saved/ForgeStationTest/results.txt and quits.
 * Only built with RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API UForgeStationTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(const float DeltaTime) override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	enum class EStep : uint8
	{
		WaitingForStart,
		CheckSetup,
		CheckGarrison,
		WatchWork,
		MovePlayerUnit,
		CheckPlate,
		KillTargets,
		CheckWrecks,
		CheckComplete,
		CheckReinforcements,
		Finished
	};

	EStep M_Step = EStep::WaitingForStart;
	double M_TestStartSeconds = -1.0;
	int32 M_Failed = 0;
	TArray<FString> M_Lines;

	UPROPERTY()
	TWeakObjectPtr<ABerlinForgeStation> M_Station;

	UPROPERTY()
	TWeakObjectPtr<AEnemyStrongpoint> M_Strongpoint;

	UPROPERTY()
	TWeakObjectPtr<UMission_BerlinForgeStation> M_Mission;

	UPROPERTY()
	TObjectPtr<UAnimMontage> M_WorkMontage;

	// Seen while watching: the most circuits working at once, a work montage playing, the sedation phases.
	int32 M_MaxWorkingCircuits = 0;
	bool bM_SawWorkMontage = false;
	TSet<uint8> M_SeenSedationPhases;
	bool bM_PlateShownBeforeMove = false;
	// -ForgeStationShots: lit captures of the working station and of the wrecks (Saved/Screenshots).
	bool bM_TakeShots = false;
	int32 M_ShotsTaken = 0;
	int32 M_CameraPlacedForShot = INDEX_NONE;
	bool bM_MovedPlayerUnit = false;

	double GetTestSeconds() const;
	void StartTest();
	void TickSteps();
	void CheckSetup();
	void CheckGarrison();
	void WatchWork();
	void ReportWork();
	void TakeShotIfDue(const double Seconds);
	void LookAtStation(const FVector& LocalCameraOffset);
	void MovePlayerUnit();
	void KillTargets();
	void CheckWrecks();
	void CheckComplete();
	void CheckReinforcements();
	int32 GetExpectedDefenders() const;
	bool GetIsAnyWorkMontagePlaying() const;
	void Check(const bool bCondition, const FString& Name, const FString& Detail);
	void Evaluate();
};
