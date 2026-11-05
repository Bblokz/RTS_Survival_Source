// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EliteScavengersTestSubsystem.generated.h"

class ACameraActor;
class ASquadController;
class UGlobalAbility;

UENUM()
enum class EEliteScavengersTestPhase : uint8
{
	WaitingForStart,
	WaitingForSpawnedSquad,
	WaitingForParadrop,
	Finished
};

/**
 * @brief -EliteScavengersValidate: spawns one Squad_Ger_EliteScavengers through the async spawner and checks its
 * soldiers (5, 3 STG 44 + 2 Panzerfaust, health = scavenger health x 1.33, subtype), then fires the
 * GA_GerEliteScavengersParadrop template on the anchor (where the camera looks, or -EliteScavAnchorX= and
 * -EliteScavAnchorY=) and checks both dropped squads land complete. Screenshots into Saved/EliteScavengersTest.
 * Logs RTS_ELITESCAV_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UEliteScavengersTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EEliteScavengersTestPhase M_Phase = EEliteScavengersTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_DirectSpawnLocation = FVector::ZeroVector;
	bool bM_SawActiveDrop = false;
	bool bM_TookDescentShot = false;
	float M_FirstSquadSeenSeconds = -1.f;
	float M_DropFinishedSeconds = -1.f;

	// Nothing else holds the running ability; its load callbacks and timers outlive the start of the drop.
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SpawnedSquad;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_DroppedSquads;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void StartDirectSpawn();
	void OnDirectSquadSpawned(AActor* SpawnedActor);
	void TickWaitingForSpawnedSquad();
	/** @brief Squad-level checks shared by the spawned and the dropped squads. */
	void CheckSquad(ASquadController* Squad, const FString& Label);
	void StartParadrop();
	void TickWaitingForParadrop();
	void FinishParadrop();

	void ViewFrom(const FVector& Focus, float Distance, float Pitch) const;
	void RequestScreenshot(const FString& Label);
};
