// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "BreakthroughBunkersTestSubsystem.generated.h"

class ABuildingExpansion;
class ACPPController;
class ANomadicVehicle;
class UTrainerComponent;
enum class ERTSResourceType : uint8;
struct FTrainingOption;
struct FTrainingQueueItem;

UENUM()
enum class EBreakthroughBunkersTestStage : uint8
{
	WaitingForStart,
	WaitingForTruckSpawn,
	WaitingForDeploy,
	PlacingNextBunker,
	WaitingForBunkerBuilt,
	Finished
};

/**
 * @brief -BreakthroughBunkersValidate: the Breakthrough doctrine's concrete bunkers through the HQ's field defense flow.
 * The player is set to the Breakthrough doctrine, a German HQ is spawned and deployed; its Barriers tab must show the
 * rail Pak bunker and the Flak 88, Flakvierling and Pz 38(t) bunkers within the nine visible slots, each bunker must
 * need the Communication Center. Each bunker is then planned on free ground near the HQ as the placement click does,
 * built by the queue, and checked: its type from the Blueprint, the price taken, procedural holes and its turret.
 * Optional -BtBunkersAnchorX= / -BtBunkersAnchorY=. Log lines RTS_BTBUNKERS_TEST; not in shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API UBreakthroughBunkersTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EBreakthroughBunkersTestStage M_Stage = EBreakthroughBunkersTestStage::WaitingForStart;
	float M_BootSeconds = -1.f;
	float M_StageSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	bool bM_StageActionDone = false;
	int32 M_BunkerIndex = -1;
	int32 M_LastTraceSecond = -1;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_PlannedSpot = FVector::ZeroVector;
	int32 M_RadixiteBeforePlan = 0;
	int32 M_MetalBeforePlan = 0;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_Hq;

	UPROPERTY()
	TWeakObjectPtr<UTrainerComponent> M_Trainer;

	// Diagnostics: the expansion under test and its last logged state.
	UPROPERTY()
	TWeakObjectPtr<ABuildingExpansion> M_Traced;

	FString M_TracedState;

	void TickWaitingForStart();
	void TickWaitingForTruckSpawn();
	void TickWaitingForDeploy();
	void TickPlacingNextBunker();
	void TickWaitingForBunkerBuilt();

	void CheckBarriersTab();
	void CheckBuiltBunker(ABuildingExpansion& Bunker);
	EBuildingExpansionType GetCurrentType() const;
	FTrainingQueueItem MakeQueueItem(const FTrainingOption& Option) const;
	bool FindValidSpot(FVector& OutSpot) const;
	ABuildingExpansion* FindBunkerNear(const FVector& Spot) const;

	bool StartGameAndGetIsReady() const;
	bool FindAnchor(FVector& OutAnchor) const;
	void GivePlayerResources() const;
	int32 GetResource(ERTSResourceType ResourceType) const;
	ACPPController* GetPlayerController() const;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	void EnterStage(EBreakthroughBunkersTestStage NewStage);
	float GetNow() const;
	float GetStageElapsed() const;
	void FailAndFinish(const FString& Description, const FString& Details = FString());
	void Finish();
};
