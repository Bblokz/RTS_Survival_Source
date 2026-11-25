// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSOptimizationDistance.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTSOptimizer.generated.h"

class ACPPController;
class UVehicleFireFeedbackComponent;

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

	UPROPERTY()
	bool bPauseAnimations = false;
};

USTRUCT()
struct FRTSVehicleFireFeedbackOptimizationSettings
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UVehicleFireFeedbackComponent> FireFeedbackComponent = nullptr;

	UPROPERTY()
	float BaseTickInterval = 0.f;
};


/**
 * @brief Added to actors whose component update cost should scale with camera visibility and distance.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSOptimizer : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSOptimizer();

	void SetOptimizationEnabled(const bool bEnable);
	ERTSOptimizationDistance GetCurrentOptimizationDistance() const;
	bool GetIsInFOV() const;

	/**
	 * @brief Far out of view the owner's skeletal meshes stop ticking, which freezes montages. Gameplay that waits for a
	 * montage to end (a harvester's harvest montage) turns this on while it plays, so the montage still advances and
	 * fires its end; only montages tick then, the anim graph does not.
	 * @param bKeepTicking True while such a montage plays.
	 */
	void SetKeepMontagesTicking(const bool bKeepTicking);

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
	void StopSkeletalMeshAnimations();

	virtual bool ShouldAlwaysConsiderOwnerInFOVAtCloseRange() const;

private:
	UPROPERTY()
	TArray<FRTSTickingComponent> M_TickingComponentsNotSkeletal = {};

	UPROPERTY()
	TArray<FRTSOptimizePrimitive> M_PrimitiveComponents = {};

	UPROPERTY()
	TArray<FSkeletonOptimizationSettings> M_SkeletalMeshComponents = {};

	UPROPERTY()
	TArray<FRTSVehicleFireFeedbackOptimizationSettings> M_VehicleFireFeedbackComponents = {};

	FTimerHandle M_OptimizationTimer;

	void OptimizeTick();

	float M_OptimizeInterval = 0.f;

	ERTSOptimizationDistance M_PreviousOptimizationDistance = ERTSOptimizationDistance::None;

	// Set by SetKeepMontagesTicking: far out of view, skeletons keep ticking their montages instead of stopping.
	bool bM_KeepMontagesTicking = false;

	// Far-out-of-view skeleton settings; respects bM_KeepMontagesTicking.
	void UpdateSkeletonsForOutFovFar();

	ERTSOptimizationDistance GetOptimizationDistanceToCamera() const;

	UPROPERTY()
	TObjectPtr<ACPPController> M_PlayerController = nullptr;

	void BeginPlay_SetPlayerController();
	void BeginPlay_SetupComponentReferences();


	void TickingComponentDetermineOptimizedTicks(
		UActorComponent* TickingComponent);

	void UpdateVehicleFireFeedbackComponentsForInFOV() const;
	void UpdateVehicleFireFeedbackComponentsForOutFOVFar() const;
};
