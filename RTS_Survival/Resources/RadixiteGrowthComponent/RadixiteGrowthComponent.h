#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RadixiteGrowthComponent.generated.h"

class ADecalActor;
class UNavigationSystemV1;
class UMaterialInterface;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
struct FPathFindingQuery;

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERadixiteGrowthDirectionBias : uint8
{
	None = 0 UMETA(Hidden),
	NoPreference = 1 << 0 UMETA(DisplayName = "No Preference"),
	WorldPosX = 1 << 1 UMETA(DisplayName = "World Pos X"),
	WorldNegX = 1 << 2 UMETA(DisplayName = "World Neg X"),
	WorldPosY = 1 << 3 UMETA(DisplayName = "World Pos Y"),
	WorldNegY = 1 << 4 UMETA(DisplayName = "World Neg Y")
};
ENUM_CLASS_FLAGS(ERadixiteGrowthDirectionBias);

USTRUCT(BlueprintType)
struct FGrowthDecalOptions
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Decal")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Decal")
	float AimAtRotationYaw = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Decal", meta = (ClampMin = "1.0"))
	float DistanceCoveredAtScale1 = 150.f;
};

USTRUCT(BlueprintType)
struct FGrowthNodeOptions
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Node")
	TArray<TSubclassOf<AActor>> GrowthNodeClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Node")
	float MinRotation = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Node")
	float MaxRotation = 360.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Node", meta = (ClampMin = "0"))
	int32 MaxSpawnedNodes = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Node", meta = (ClampMin = "0"))
	int32 MaxDecalGrowthPerNode = 0;
};

USTRUCT(BlueprintType)
struct FGrowthFxSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Fx")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Fx")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Fx")
	TObjectPtr<USoundAttenuation> SoundAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth Fx")
	TObjectPtr<USoundConcurrency> SoundConcurrency = nullptr;
};

USTRUCT()
struct FRadixiteGrowthBranchRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 BranchId = INDEX_NONE;

	UPROPERTY()
	int32 StartNodeId = INDEX_NONE;

	UPROPERTY()
	int32 EndNodeId = INDEX_NONE;

	UPROPERTY()
	bool bM_HasPendingGrowthNode = false;

	UPROPERTY()
	float InitialYaw = 0.f;

	UPROPERTY()
	int32 InitialYawBucket = INDEX_NONE;

	UPROPERTY()
	FVector PendingNodeLocation = FVector::ZeroVector;

	UPROPERTY()
	TArray<TObjectPtr<ADecalActor>> SpawnedDecals;
};

USTRUCT()
struct FRadixiteGrowthNodeRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 NodeId = INDEX_NONE;

	UPROPERTY()
	bool bM_IsRootNode = false;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<AActor> SpawnedNodeActor = nullptr;

	UPROPERTY()
	TSet<int32> ConnectedNodeIds;

	UPROPERTY()
	TSet<int32> UsedInitialYawOffsets;
};

USTRUCT()
struct FRadixiteGrowthFxCacheRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> NiagaraComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent = nullptr;
};

/**
 * @brief Attach this component to a radixite owner actor and drive growth from BP defaults.
 * It expands decal branches first, then spawns node actors at validated branch endpoints.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URadixiteGrowthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URadixiteGrowthComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BeginPlay_InitCachedSystems();
	void BeginPlay_InitRootGrowthNode();
	void BeginPlay_InitGrowthTimers();
	void BeginPlay_InitOwnerDestructionCallback();

	void TickDecalGrowth();
	void TickNodeGrowth();
	void TickOwnerDestructionSequence();
	void CleanupInvalidReferences();
	void CleanupInvalidBranchDecals(FRadixiteGrowthBranchRecord& BranchRecord) const;
	bool GetHasAtLeastOneValidDecal(const FRadixiteGrowthBranchRecord& BranchRecord) const;

	bool TrySpawnDecalBranch();
	bool TrySpawnDecalBranchForNode(const int32 StartNodeId);

	/**
	 * @brief Builds and validates the full branch path before spawning decals to avoid partial placement.
	 * @param StartLocation Source location for the first branch segment.
	 * @param BranchLength Number of decal segments to build in this branch.
	 * @param InitialYaw Starting yaw of the first segment.
	 * @param DirectionSign +1 or -1 for offset rotation stepping.
	 * @param SegmentDistance Segment length used for each decal step.
	 * @param OutBranchPoints Ordered points from source to last segment endpoint.
	 * @return True when every segment endpoint is valid and can be used for spawning.
	 */
	bool TryBuildBranchPoints(
		const FVector& StartLocation,
		const int32 BranchLength,
		const float InitialYaw,
		const float DirectionSign,
		const float SegmentDistance,
		TArray<FVector>& OutBranchPoints) const;

	/**
	 * @brief Validates a growth point with landscape tracing, nav projection and nav-area-cost filtering.
	 * @param DesiredPoint Candidate location before terrain projection.
	 * @param OutValidatedPoint Final location on valid low-cost nav terrain.
	 * @return True if the point is usable for decals and future node spawning.
	 */
	bool TryGetValidGrowthPoint(const FVector& DesiredPoint, FVector& OutValidatedPoint) const;
	bool TryBuildHighCostQuery(FPathFindingQuery& OutPathFindingQuery, const FVector& QueryLocation) const;

	bool TrySpawnGrowthNodeFromPendingBranch();
	bool TrySpawnGrowthNodeForBranch(FRadixiteGrowthBranchRecord& BranchToGrow);

	void RegisterSpawnedBranch(
		const int32 StartNodeId,
		const float InitialYaw,
		const int32 InitialYawBucket,
		const FVector& PendingNodeLocation,
		const TArray<TObjectPtr<ADecalActor>>& SpawnedDecals);
	void RegisterSpawnedGrowthNode(
		FRadixiteGrowthBranchRecord& BranchToGrow,
		AActor* SpawnedNodeActor,
		const FVector& SpawnLocation);

	void DestroyBranchDecals(FRadixiteGrowthBranchRecord& BranchRecord) const;
	void DestroyAllGrowthContentImmediately();
	void StartOwnerDestructionSequence();
	void StopOwnerDestructionSequence();
	bool TryPopNextDestructionBranch(FRadixiteGrowthBranchRecord& OutBranchRecord);
	void QueueDestructionBranchesInDistanceOrder();
	void DestroyRemainingGrowthNodesForOwnerDestruction();
	void StartDecalShrinkAndDestroy(ADecalActor* DecalToDestroy);
	void HandleNodeDestructionOverTime(AActor* NodeActorToDestroy);
	void StartNodeSpawnVerticalAnimation(AActor* SpawnedNodeActor, const FVector& FinalSpawnLocation);
	void TryDestroyRuntimeFxCache(FRadixiteGrowthFxCacheRuntime& FxCacheRuntime);
	void RemoveBranchesForNodeDestruction(const int32 DestroyedNodeId, const int32 ConnectionsCount);
	void RemoveBranchesThatNoLongerHaveDecals();
	void RemoveBranchByIndex(const int32 BranchIndex);
	void RemoveNodeConnections(const int32 DestroyedNodeId);
	void ReleaseInitialYawBucket(const FRadixiteGrowthBranchRecord& BranchRecord);

	bool GetIsValidOwnerActor() const;
	bool GetIsValidNavigationSystem() const;
	bool GetIsValidWorld() const;

	int32 GetCurrentSpawnedDecalCount() const;
	int32 GetCurrentSpawnedNodeCount() const;
	int32 GetNextNodeId();
	int32 GetNextBranchId();
	int32 GetNodeIdByActor(const AActor* NodeActor) const;
	int32 GetYawBucketFromYaw(const float YawToBucket) const;

	bool TryGetUnsaturatedNodeIds(TArray<int32>& OutNodeIds) const;
	bool TryChooseInitialYaw(const FRadixiteGrowthNodeRecord& NodeRecord, float& OutYaw, int32& OutYawBucket) const;
	bool GetShouldUseDirectionalBias() const;
	bool GetShouldIgnoreDirectionalBias() const;
	float GetBiasedYaw() const;
	float GetRandomOffsetYaw() const;

	TObjectPtr<ADecalActor> SpawnGrowthDecal(
		const FVector& SegmentStart,
		const FVector& SegmentEnd,
		const float SegmentDistance,
		const float SegmentYaw);
	void PlayCreationFxAtLocation(const FVector& EffectLocation, const bool bIsNodeEffect);
	void PlayDestructionFxAtLocation(const FVector& EffectLocation, const bool bIsNodeEffect);
	void PlayFxAtLocation(
		const FVector& EffectLocation,
		const FGrowthFxSettings& GrowthFxSettings,
		FRadixiteGrowthFxCacheRuntime& FxCacheRuntime);
	void UpdateCachedNiagaraFx(
		const FVector& EffectLocation,
		const FGrowthFxSettings& GrowthFxSettings,
		FRadixiteGrowthFxCacheRuntime& FxCacheRuntime);
	void UpdateCachedAudioFx(
		const FVector& EffectLocation,
		const FGrowthFxSettings& GrowthFxSettings,
		FRadixiteGrowthFxCacheRuntime& FxCacheRuntime);
	TSubclassOf<AActor> GetRandomGrowthNodeClass() const;

	UFUNCTION()
	void HandleSpawnedGrowthNodeDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleOwnerDestroyed(AActor* DestroyedActor);

	UPROPERTY(EditAnywhere, Category = "Radixite Growth|Bias", meta = (Bitmask, BitmaskEnum = "/Script/RTS_Survival.ERadixiteGrowthDirectionBias"))
	int32 M_GrowthDirectionBias = static_cast<int32>(ERadixiteGrowthDirectionBias::NoPreference);

	UPROPERTY(EditAnywhere, Category = "Radixite Growth|Bias", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float M_GrowthDirectionBiasProbability = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Intervals", meta = (ClampMin = "0.01"))
	float M_IntervalNodeGrowth = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Intervals", meta = (ClampMin = "0.01"))
	float M_IntervalDecalGrowth = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals")
	TArray<FGrowthDecalOptions> M_GrowthDecalOptions;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals", meta = (ClampMin = "1.0"))
	float M_MinDecalExpansionDistance = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals", meta = (ClampMin = "1.0"))
	float M_MaxDecalExpansionDistance = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals", meta = (ClampMin = "0.1"))
	float M_DecalRotationOffset = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals", meta = (ClampMin = "0"))
	int32 M_MaxDecalGrowth = 100;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Nodes")
	FGrowthNodeOptions M_GrowthNodeOptions;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Nodes", meta = (ClampMin = "1"))
	int32 M_MinDecalGrowthsUntilNode = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Nodes", meta = (ClampMin = "1"))
	int32 M_MaxDecalGrowthsUntilNode = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Nodes", meta = (ClampMin = "0.0"))
	float M_NodeSpawnStartZOffset = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Nodes", meta = (ClampMin = "0.01"))
	float M_NodeSpawnVerticalAnimationTime = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Spawning", meta = (ClampMin = "1"))
	int32 M_MaxRetries = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Spawning", meta = (ClampMin = "1.0"))
	float M_ZLandscapeTraceOffset = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Spawning")
	FVector M_ProjectionToNavMeshExtent = FVector(150.f, 150.f, 250.f);

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Destruction", meta = (ClampMin = "0.01"))
	float M_IntervalDestruction = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Destruction", meta = (ClampMin = "0.01"))
	float M_DecalShrinkTime = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Fx")
	FGrowthFxSettings M_DecalCreationFxSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Fx")
	FGrowthFxSettings M_DecalDestructionFxSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Fx")
	FGrowthFxSettings M_NodeCreationFxSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Fx")
	FGrowthFxSettings M_NodeDestructionFxSettings;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_OwnerActor = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UNavigationSystemV1> M_NavigationSystem = nullptr;

	// Stores active growth-node graph connectivity and per-node used yaw buckets.
	UPROPERTY()
	TArray<FRadixiteGrowthNodeRecord> M_GrowthNodes;

	// Tracks each spawned decal branch so node spawning and destruction cleanup stay deterministic.
	UPROPERTY()
	TArray<FRadixiteGrowthBranchRecord> M_DecalBranches;

	FTimerHandle M_TimerHandleDecalGrowth;
	FTimerHandle M_TimerHandleNodeGrowth;
	FTimerHandle M_TimerHandleOwnerDestruction;

	UPROPERTY()
	FRadixiteGrowthFxCacheRuntime M_DecalCreationFxCacheRuntime;

	UPROPERTY()
	FRadixiteGrowthFxCacheRuntime M_DecalDestructionFxCacheRuntime;

	UPROPERTY()
	FRadixiteGrowthFxCacheRuntime M_NodeCreationFxCacheRuntime;

	UPROPERTY()
	FRadixiteGrowthFxCacheRuntime M_NodeDestructionFxCacheRuntime;

	TMap<TObjectPtr<ADecalActor>, FTimerHandle> M_ActiveDecalShrinkTimers;
	TMap<TObjectPtr<AActor>, FTimerHandle> M_ActiveNodeSpawnAnimationTimers;
	TArray<int32> M_OwnerDestructionBranchQueue;

	bool bM_IsOwnerDestructionSequenceActive = false;
	FVector M_LastKnownOwnerLocation = FVector::ZeroVector;

	int32 M_NextNodeId = 0;
	int32 M_NextBranchId = 0;
};
