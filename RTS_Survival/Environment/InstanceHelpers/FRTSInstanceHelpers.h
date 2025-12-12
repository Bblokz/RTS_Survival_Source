#pragma once

#include "CoreMinimal.h"

#include "FRTSInstanceHelpers.generated.h"

USTRUCT(BlueprintType)
struct FRTSRoadInstanceSetup
{
	GENERATED_BODY()
	// The distance over which the parts are calculated both backwards and forwards.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistanceForwardBackward = 3000.f;

	// The distance between sets of instances along the road.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistanceBetweenRoadParts = 300.f;

	// Within one placed set of instances; what is the distance between each instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistanceBetweenInstancesAtPart = 100.f;

	// if set allows for [-value, value] random range for the yaw of the instances.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float NegPosRandomYawRange = 0.f;

	// if set allows for [-value, value] random range for the pitch of the instances.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float NegPosRandomPitchRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float NegPosRandomRollRange = 0.f;

	// Whether to create instances on both sides of the road or not.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsTwoSided = true;

	// If not two sided; only create on the left side?
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsLeftSided = true;

	// Overwrites the random yaw range with a set of values from which one is picked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<float> RandomYawRotationValues = {};

	// Overwrites the random Roll range with a set of values from which one is picked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<float> RandomRollRotationValues = {};

	// Overwrites the random Pitch range with a set of values from which one is picked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<float> RandomPitchRotationValues = {};
};

class RTS_SURVIVAL_API FRTSInstanceHelpers
{
public:
	static void SetupHierarchicalInstanceBox(UInstancedStaticMeshComponent* InstStaticMesh,
	                                         FVector2D BoxBounds,
	                                         const float DistanceBetweenInstances,
	                                         AActor* RequestingActor);

	/**
	 * @brief Place instances along a virtual road forwards and backwards with offsets from an actor.
	 * @param RoadSetup Defines how long to place forwards and backwards and on which side(s).
	 * @param InstStaticMesh The instance component to place the instances on.
	 * @param RequestingActor The actor owning the component and making the request.
	 */
	static void SetupHierarchicalInstanceAlongRoad(const FRTSRoadInstanceSetup RoadSetup,
	                                               UInstancedStaticMeshComponent* InstStaticMesh,
	                                               const AActor* RequestingActor);

	static float VisibilityTraceForZOnLandscape(const FVector& StartLocation, const UWorld* World, const float ZBackupValue);

private:
	static bool EnsureInstanceAlongRoadRequestIsValid(
		const UInstancedStaticMeshComponent* InstStaticMesh,
		const AActor* RequestingActor);

	static void CreateRoadInstancesWithOffset(const FRTSRoadInstanceSetup& RoadSetup,
	                                          UInstancedStaticMeshComponent* InstStaticMesh,
	                                          const FVector& PartLocation, FRotator Rotation,
	                                          const FVector& Scale, const FVector& RightVector, const AActor* RequestingActor);

private:
	static void DetermineRandomRollValue(const FRTSRoadInstanceSetup& RoadSetup, FRotator& OutRotation);
	static void DetermineRandomPitchValue(const FRTSRoadInstanceSetup& RoadSetup, FRotator& OutRotation);
	static void DetermineRandomYawValue(const FRTSRoadInstanceSetup& RoadSetup, FRotator& OutRotation);
};

