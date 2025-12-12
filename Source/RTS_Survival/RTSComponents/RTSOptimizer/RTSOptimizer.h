// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSOptimizationDistance.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTSOptimizer.generated.h"

class ACPPController;

USTRUCT()
struct FRTSTickingComponent
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UActorComponent> TickingComponent = nullptr;

	UPROPERTY()
	float BaseTickInterval = 0.f;

	UPROPERTY()
	float CloseOutFovInterval = 0.f;

	UPROPERTY()
	float FarOutFovInterval = 0.f;
};

USTRUCT()
struct FRTSOptimizePrimitive
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> PrimitiveComponent = nullptr;

	UPROPERTY()
	bool bCastContactShadow = false;

	UPROPERTY()
	bool bCastDynamicShadow = false;
};


USTRUCT()
struct FSkeletonOptimizationSettings
{
	GENERATED_BODY()
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Skeleton = nullptr;
	
	UPROPERTY()
	EVisibilityBasedAnimTickOption BasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	UPROPERTY()
	bool bCastDynamicShadow = false;

	UPROPERTY()
	bool bCastContactShadow = false;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSOptimizer : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSOptimizer();

	void SetOptimizationEnabled(const bool bEnable);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void DetermineOwnerOptimization(AActor* ValidOwner);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;


	virtual void InFOVUpdateComponents();
	virtual void OutFovCloseUpdateComponents();
	virtual void OutFovFarUpdateComponents();

	virtual void DetermineCharMovtOptimization(UCharacterMovementComponent* CharacterMovementComponent);
	virtual void DetermineChildActorCompOptimization(UChildActorComponent* ChildActorComponent);

	void AddSkeletonToOptimize(USkeletalMeshComponent* Skeleton);
	void AddPrimitiveComponentToOptimize(UPrimitiveComponent* PrimitiveComponent);

private:
	UPROPERTY()
	TArray<FRTSTickingComponent> M_TickingComponentsNotSkeletal = {};

	UPROPERTY()
	TArray<FRTSOptimizePrimitive> M_PrimitiveComponents = {};

	UPROPERTY()
	TArray<FSkeletonOptimizationSettings> M_SkeletalMeshComponents = {};

	FTimerHandle M_OptimizationTimer;

	void OptimizeTick();

	float M_OptimizeInterval = 0.f;

	ERTSOptimizationDistance M_PreviousOptimizationDistance = ERTSOptimizationDistance::None;

	ERTSOptimizationDistance GetOptimizationDistanceToCamera() const;

	UPROPERTY()
	ACPPController* M_PlayerController = nullptr;

	void BeginPlay_SetPlayerController();
	void BeginPlay_SetupComponentReferences();


	void TickingComponentDetermineOptimizedTicks(
		UActorComponent* TickingComponent);
};
