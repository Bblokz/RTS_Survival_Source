// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "IronStormTestSubsystem.generated.h"

class ACameraActor;
class ASquadController;
class UGlobalAbility;

UENUM()
enum class EIronStormTestPhase : uint8
{
	WaitingForStart,
	WaitingForSpawnedSquad,
	WaitingForParadrop,
	Finished
};

/**
 * @brief -IronStormValidate: spawns one Squad_Ger_IronStorm through the async spawner (its row must give the Iron Storm
 * Blueprint: 4 cyborgs with STG 44s at the squad data health), then fires GA_GerIronStormParadrop (1 Iron Storm) and
 * GA_GerPzGrenIronStormParadrop (2 Panzergrenadiere + 2 Iron Storm) and checks every dropped squad lands complete,
 * owned by player 1, with radixite-only costs and slot-fitting tiers. Anchor: -IronStormAnchorX= -IronStormAnchorY=
 * (else where the camera looks). Screenshots into Saved/IronStormTest. Logs RTS_IRONSTORM_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UIronStormTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EIronStormTestPhase M_Phase = EIronStormTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_DropAnchor = FVector::ZeroVector;
	FVector M_DirectSpawnLocation = FVector::ZeroVector;
	FBox M_LandscapeBounds = FBox(ForceInit);

	// Index into the drop cases of the drop in flight.
	int32 M_DropIndex = 0;
	bool bM_SawActiveDrop = false;
	bool bM_TookDescentShot = false;
	float M_FirstSquadSeenSeconds = -1.f;
	float M_DropFinishedSeconds = -1.f;

	// Nothing else holds the running ability; its load callbacks and timers outlive the start of the drop.
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SpawnedSquad;

	// Every squad in the world before the current drop; the drop's squads are the ones not in here afterwards.
	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_KnownSquads;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	FBox FindLandscapeBounds() const;
	FVector KeepInsideLandscape(const FVector& Location) const;

	void StartDirectSpawn();
	void OnDirectSquadSpawned(AActor* SpawnedActor);
	void TickWaitingForSpawnedSquad();
	/** @brief Iron Storm checks shared by the spawned and the dropped squads: subtype, soldiers, STG 44s, health. */
	void CheckIronStormSquad(ASquadController* Squad, const FString& Label);

	void StartParadrop(int32 DropIndex);
	/** @brief Tier, cooldown band and the radixite-only cost of the template about to fire. */
	void CheckDropTemplate(const UGlobalAbility* Ability, int32 DropIndex);
	void TickWaitingForParadrop();
	void FinishParadrop();
	/** @return Squads in the world that existed before the current drop started. */
	bool GetIsKnownSquad(const ASquadController* Squad) const;
	void CheckDroppedSquadLanded(ASquadController* Squad, const FString& Label);

	void ViewFrom(const FVector& Focus, float Distance, float Pitch) const;
	void RequestScreenshot(const FString& Label);
};
