// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "ReinforcementPoint.generated.h"

class UMeshComponent;
enum class ETriggerOverlapLogic : uint8;

/**
 * @brief Defines a socket-based reinforcement location on a mesh.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UReinforcementPoint : public UActorComponent
{
	GENERATED_BODY()

public:
	UReinforcementPoint();

	/**
	 * @brief Initialize the reinforcement mesh and socket.
	 * @param InMeshComponent Mesh component that owns the socket.
	 * @param InSocketName Socket name on the mesh to use for reinforcements.
	 * @param OwningPlayer Index of the player that owns the reinforcement point.
	 * @param ActivationRadius Radius used for reinforcement activation.
	 * @param bStartEnabled Toggle to start with trigger overlaps enabled.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitReinforcementPoint(UMeshComponent* InMeshComponent, const FName& InSocketName,
	                            int32 OwningPlayer, float ActivationRadius, bool bStartEnabled = true);

	/**
	 * @brief Toggle the reinforcement trigger overlaps without destroying the sphere.
	 * @param bEnable True to enable overlaps, false to disable.
	 */
	void SetReinforcementEnabled(bool bEnable);

	/** @return True when the reinforcement trigger is currently enabled. */
	bool GetIsReinforcementEnabled() const;

	/**
	 * @brief Resolve the world-space location of the configured reinforcement socket.
	 * @param bOutHasValidLocation Set true when a valid mesh and socket were found.
	 * @return Socket world location or ZeroVector when invalid.
	 */
	FVector GetReinforcementLocation(bool& bOutHasValidLocation) const;

	static float SearchForSquadsInterval;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool GetIsDebugEnabled() const;
	bool GetIsValidReinforcementMesh(bool bReportIfMissing = true) const;
	void DrawDebugStatusString(const FString& DebugText, const FVector& DrawLocation) const;
	void StartSquadSearchTimer();
	void StopSquadSearchTimer();
	void RequestSquadOverlapCheck();
	void HandleAsyncSquadOverlapResults(TArray<FHitResult> HitResults);
	void HandleSquadControllerOverlap(class ASquadController* SquadController);
	void HandleSquadControllerExit(class ASquadController* SquadController);
	void ClearTrackedSquadControllers();
	FVector GetSearchOriginLocation() const;
	FCollisionQueryParams BuildOverlapQueryParams(AActor* OwnerActor, TSet<const AActor*>& OutIgnoredActors) const;
	FTraceDelegate BuildOverlapTraceDelegate(TSet<const AActor*> IgnoredActors) const;
	FCollisionObjectQueryParams BuildObjectQueryParams(ETriggerOverlapLogic OverlapLogic) const;

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> M_ReinforcementMeshComponent;

	UPROPERTY()
	FName M_ReinforcementSocketName;

	UPROPERTY()
	int32 M_OwningPlayer;

	UPROPERTY()
	float M_ReinforcementActivationRadius;

	UPROPERTY()
	FTimerHandle M_SearchForSquadsTimerHandle;

	UPROPERTY()
	TSet<TWeakObjectPtr<class ASquadController>> M_TrackedSquadControllers;

	UPROPERTY()
	bool bM_ReinforcementEnabled = false;
};
