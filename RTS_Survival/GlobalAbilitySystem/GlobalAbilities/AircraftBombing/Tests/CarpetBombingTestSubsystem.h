// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "CarpetBombingTestSubsystem.generated.h"

class AAircraftMaster;
class UGlobalAbility;

/**
 * @brief -CarpetBombingValidate on a mission map (UT_Nomadics): fires the Ju 390 carpet bombing through the player's
 * own global ability manager at the camera's ground point, with two T-34s on the run, and checks the bombs fall from
 * the target onwards (away from the bomber's spawn edge), none back toward it, and the run hits the tanks.
 * Writes Saved/CarpetBombingTest/results.txt and quits. Only built with RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API UCarpetBombingTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(const float DeltaTime) override;
	// The controller pauses mission maps at start; the test unpauses it.
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	enum class EPhase : uint8
	{
		WaitingForStart,
		WaitingForBomber,
		Bombing,
		Finished
	};

	EPhase M_Phase = EPhase::WaitingForStart;
	double M_PhaseStartSeconds = -1.0;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> M_Bomber;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_AircraftOfTheMap;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_Tanks;

	// Last seen location of each falling pooled bomb; a bomb hidden again landed there.
	TMap<TWeakObjectPtr<AActor>, FVector> M_BombLastSeen;
	TArray<FVector> M_BombImpacts;

	FVector M_Target = FVector::ZeroVector;
	FVector M_BomberSpawn = FVector::ZeroVector;
	float M_CarpetLength = 0.f;
	bool bM_AbilityEnded = false;
	// World seconds when the ability ended; bombs still on the bomber stay visible, so the test waits a fixed time.
	double M_AbilityEndedSeconds = 0.0;
	TArray<FString> M_Lines;
	int32 M_Failed = 0;

	void SetPhase(const EPhase NewPhase);
	double GetPhaseSeconds() const;
	void StartBombing();
	void TickWaitingForBomber();
	void TickBombing();
	void SpawnTanksOnRun();
	void TrackBombImpacts();
	void Evaluate();
	void Check(const bool bCondition, const FString& Name, const FString& Detail);
	bool FindGround(const FVector2D& XY, FVector& OutGround) const;
};
