// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "BxpPackUpRedeployTestSubsystem.generated.h"

class ACPPController;
class ANomadicVehicle;
class IBuildingExpansionOwner;

UENUM()
enum class EBxpPackUpRedeployTestStage : uint8
{
	WaitingForStart,
	WaitingForTruckSpawn,
	WaitingForFirstDeploy,
	WaitingForBxpLoaded,
	WaitingForBxpsBuilt,
	WaitingForPackUp,
	WaitingForRedeploy,
	WaitingForReplacedBxps,
	Finished
};

/** @brief Where one attached expansion stood after its first placement, relative to the building and in the world. */
USTRUCT()
struct FBxpPackUpRedeployTestSlot
{
	GENERATED_BODY()

	FBxpOptionData Option;
	int32 SlotIndex = INDEX_NONE;
	FName SocketName = NAME_None;
	FVector WorldLocation = FVector::ZeroVector;
	FRotator WorldRotation = FRotator::ZeroRotator;
	// Expansion transform in the space of the building mesh it was placed on.
	FTransform RelativeToBuilding = FTransform::Identity;
	float BuildingYaw = 0.f;
	bool bPlaced = false;
};

/**
 * @brief -BxpPackUpRedeployValidate: spawns a German HQ truck, deploys it at yaw 90, leaves expansion slot 0 empty and
 * places a Socket expansion in slot 1 and an AtBuildingOrigin expansion in slot 2 through the real option click and
 * placement click. It packs the HQ up, redeploys it on the same spot at yaw 180 and checks that both expansions come
 * back by themselves into the same slots, on the same socket / origin, with the same placement as the first time.
 * Optional -BxpRedeployAnchorX= / -BxpRedeployAnchorY=, else where the camera looks. Log lines RTS_BXP_REDEPLOY_TEST;
 * not in shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API UBxpPackUpRedeployTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EBxpPackUpRedeployTestStage M_Stage = EBxpPackUpRedeployTestStage::WaitingForStart;
	float M_BootSeconds = -1.f;
	float M_StageSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	int32 M_PlacingSlotArrayIndex = 0;
	bool bM_StageActionDone = false;
	FVector M_Anchor = FVector::ZeroVector;

	// Index 0: the Socket expansion, index 1: the AtBuildingOrigin expansion.
	UPROPERTY()
	TArray<FBxpPackUpRedeployTestSlot> M_Slots;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_Hq;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetStageElapsed() const;
	void EnterStage(EBxpPackUpRedeployTestStage NewStage);
	void FailAndFinish(const FString& Description, const FString& Details = FString());
	void Finish();
	ACPPController* GetPlayerController() const;
	bool GetHqBxpOwner(TScriptInterface<IBuildingExpansionOwner>& OutOwner) const;

	void TickWaitingForStart();
	bool StartGameAndGetIsReady() const;
	bool FindAnchor(FVector& OutAnchor) const;
	void GivePlayerResources() const;
	void SpawnHqTruck();
	void TickWaitingForTruckSpawn();
	bool TickDeployAndGetIsBuilding(float BuildYaw);

	void TickWaitingForFirstDeploy();
	bool PickExpansionOptions();
	void StartPlacingSlot(int32 SlotArrayIndex);
	void TickWaitingForBxpLoaded();
	bool GetPlacementClickLocation(const FBxpPackUpRedeployTestSlot& Slot, FVector& OutLocation) const;
	void RecordFirstPlacement(FBxpPackUpRedeployTestSlot& Slot) const;
	void TickWaitingForBxpsBuilt();

	void TickWaitingForPackUp();
	void CheckSlotsAfterPackUp();
	void TickWaitingForRedeploy();
	void TickWaitingForReplacedBxps();
	void CheckReplacedSlot(const FBxpPackUpRedeployTestSlot& Slot);
	void CheckOriginRotationAgainstBuilding(const FBxpPackUpRedeployTestSlot& Slot, const FRotator& NowRotation,
	                                        float NowBuildingYaw);
	void CheckNoSlotLeftPacked();
};
