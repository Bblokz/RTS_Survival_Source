#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RadixiteGrowthComponent.generated.h"

class ADecalActor;
class UNavigationSystemV1;
class UMaterialInterface;

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERadixiteGrowthDirectionBias : uint8
{
	None = 0 UMETA(DisplayName = "No Preference"),
	WorldPosX = 1 << 0 UMETA(DisplayName = "World Pos X"),
	WorldNegX = 1 << 1 UMETA(DisplayName = "World Neg X"),
	WorldPosY = 1 << 2 UMETA(DisplayName = "World Pos Y"),
	WorldNegY = 1 << 3 UMETA(DisplayName = "World Neg Y"),
	NoPreference = 1 << 4 UMETA(DisplayName = "No Preference (Disable Bias)")
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
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<AActor> SpawnedNodeActor = nullptr;

	UPROPERTY()
	TSet<int32> ConnectedNodeIds;

	UPROPERTY()
	TSet<int32> UsedInitialYawOffsets;
};

/**
 * @brief Attach this component to a root radixite actor and configure growth settings in BP defaults.
 * The component expands decal branches, then grows node actors on validated branch endpoints.
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

	void TickDecalGrowth();
	void TickNodeGrowth();
	void CleanupInvalidReferences();

	bool TrySpawnDecalBranch();
	bool TrySpawnDecalBranchForNode(const int32 StartNodeId);
	bool TryBuildBranchPoints(
		const FVector& StartLocation,
		const int32 BranchLength,
		const float InitialYaw,
		const float DirectionSign,
		const float SegmentDistance,
		TArray<FVector>& OutBranchPoints) const;
	bool TryGetValidGrowthPoint(const FVector& DesiredPoint, FVector& OutValidatedPoint) const;

	bool TrySpawnGrowthNodeFromPendingBranch();
	bool TrySpawnGrowthNodeForBranch(FRadixiteGrowthBranchRecord& BranchToGrow);

	void RegisterSpawnedBranch(
		const int32 StartNodeId,
		const float InitialYaw,
		const FVector& PendingNodeLocation,
		const TArray<TObjectPtr<ADecalActor>>& SpawnedDecals);
	void RegisterSpawnedGrowthNode(
		FRadixiteGrowthBranchRecord& BranchToGrow,
		AActor* SpawnedNodeActor,
		const FVector& SpawnLocation);

	void DestroyBranchDecals(FRadixiteGrowthBranchRecord& BranchRecord) const;
	void RemoveBranchesForNodeDestruction(const int32 DestroyedNodeId, const int32 ConnectionsCount);
	void RemoveBranchByIndex(const int32 BranchIndex);
	void RemoveNodeConnections(const int32 DestroyedNodeId);

	bool GetIsValidOwnerActor() const;
	bool GetIsValidNavigationSystem() const;
	bool GetIsValidWorld() const;

	int32 GetCurrentSpawnedDecalCount() const;
	int32 GetCurrentSpawnedNodeCount() const;
	int32 GetNextNodeId();
	int32 GetNextBranchId();
	int32 GetNodeIdByActor(const AActor* NodeActor) const;

	bool TryGetUnsaturatedNodeIds(TArray<int32>& OutNodeIds) const;
	bool TryChooseInitialYaw(const FRadixiteGrowthNodeRecord& NodeRecord, float& OutYaw, int32& OutYawBucket) const;
	bool GetShouldUseDirectionalBias() const;
	float GetBiasedYaw() const;
	float GetRandomOffsetYaw() const;

	TObjectPtr<ADecalActor> SpawnGrowthDecal(
		const FVector& SegmentStart,
		const FVector& SegmentEnd,
		const float SegmentDistance,
		const float SegmentYaw);
	TSubclassOf<AActor> GetRandomGrowthNodeClass() const;

	UFUNCTION()
	void HandleSpawnedGrowthNodeDestroyed(AActor* DestroyedActor);

	UPROPERTY(EditAnywhere, Category = "Radixite Growth|Bias", meta = (Bitmask, BitmaskEnum = "/Script/RTS_Survival.ERadixiteGrowthDirectionBias"))
	int32 GrowthDirectionBias = static_cast<int32>(ERadixiteGrowthDirectionBias::None);

	UPROPERTY(EditAnywhere, Category = "Radixite Growth|Bias", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrowthDirectionBiasProbability = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Intervals", meta = (ClampMin = "0.01"))
	float IntervalNodeGrowth = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Intervals", meta = (ClampMin = "0.01"))
	float IntervalDecalGrowth = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals")
	TArray<FGrowthDecalOptions> GrowthDecalOptions;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals", meta = (ClampMin = "1.0"))
	float MinDecalExpansionDistance = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals", meta = (ClampMin = "1.0"))
	float MaxDecalExpansionDistance = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals")
	float DecalRotationOffset = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Decals", meta = (ClampMin = "0"))
	int32 MaxDecalGrowth = 100;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Nodes")
	FGrowthNodeOptions GrowthNodeOptions;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Nodes", meta = (ClampMin = "1"))
	int32 MinDecalGrowthsUntilNode = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Nodes", meta = (ClampMin = "1"))
	int32 MaxDecalGrowthsUntilNode = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Spawning", meta = (ClampMin = "1"))
	int32 MaxRetries = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Spawning", meta = (ClampMin = "1.0"))
	float ZLandscapeTraceOffset = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Radixite Growth|Spawning")
	FVector ProjectionToNavMeshExtent = FVector(150.f, 150.f, 250.f);

	UPROPERTY()
	TWeakObjectPtr<AActor> M_OwnerActor = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UNavigationSystemV1> M_NavigationSystem = nullptr;

	UPROPERTY()
	TArray<FRadixiteGrowthNodeRecord> M_GrowthNodes;

	UPROPERTY()
	TArray<FRadixiteGrowthBranchRecord> M_DecalBranches;

	FTimerHandle M_TimerHandleDecalGrowth;
	FTimerHandle M_TimerHandleNodeGrowth;

	int32 M_NextNodeId = 0;
	int32 M_NextBranchId = 0;
};
