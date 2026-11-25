// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "VehicleCrushTestSubsystem.generated.h"

class ASquadController;
class ATankMaster;

enum class EVehicleCrushTestStep : uint8
{
	StartGame,
	WaitSquads,
	Drive,
	Done
};

/**
 * @brief -VehicleCrushTest (optionally -VehicleCrushTestAt=X,Y): a held-fire Pz IV H of player 1 drives a straight
 * lane through four Mosin squads standing in it: hostile (player 2, dodging off), own (player 1), allied (player 3 on
 * player 1's team) and hostile again (player 2, every soldier dodges). Checks that only hostile soldiers are run over,
 * that some are, and that soldiers get out of the way when they react. Logs RTS_VEHICLE_CRUSH lines and quits with
 * RTS_VEHICLE_CRUSH RESULT PASS|FAIL. Map tests only: compiles to nothing without RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API UVehicleCrushTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

#if RTS_WITH_MAP_TESTS

private:
	struct FLaneSquad
	{
		FString Label;
		uint8 Player = 0;
		float LaneX = 0.f;
		TWeakObjectPtr<ASquadController> Squad;
		int32 SoldiersPlaced = 0;
	};

	EVehicleCrushTestStep M_Step = EVehicleCrushTestStep::StartGame;
	FVector M_Arena = FVector::ZeroVector;
	FVector M_Goal = FVector::ZeroVector;
	TWeakObjectPtr<ATankMaster> M_Tank;
	TArray<FLaneSquad> M_Squads;
	double M_StartRealSeconds = -1.0;
	float M_StepStartSeconds = 0.f;
	bool bM_DodgeEnabled = false;
	int32 M_Passed = 0;
	int32 M_Failed = 0;

	bool TryStartGame() const;
	void KeepGameRunning() const;
	void SpawnUnits();
	void PlaceSoldiersInLane();
	void HoldTankFire() const;
	void TickDrive();
	void Finish(const FString& Reason);
	void Check(bool bCondition, const FString& Name, const FString& Detail);
	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	float GetGameSeconds() const;

#endif
};
