#pragma once

#include "CoreMinimal.h"
#include "GeneratedRotationBounds/GeneratedRotationBounds.h"
#include "RTS_Survival/Environment/EnvironmentActor/EnvironmentActor.h"
#include "DestroyedBuildingGenerator.generated.h"


struct FGeneratedRotationBounds;
class UHierarchicalInstancedStaticMeshComponent;

/**
 * ADestroyedBuildingGenerator
 * 
 * Generates a "destroyed" square room using hierarchical instanced static mesh components.
 * Walls are chosen randomly from WallMeshes, and a roof (if enabled) is placed from RoofMeshes.
 */
UCLASS()
class RTS_SURVIVAL_API ADestroyedBuildingGenerator : public AEnvironmentActor
{
	GENERATED_BODY()

public:
	/** Sets default values for this actor's properties */
	ADestroyedBuildingGenerator(const FObjectInitializer& ObjectInitializer);

protected:
	/** Called when the game starts or when spawned */
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void EnsureAllComponentsAreCleared();

	/**
	 * \brief Generates a square "room" (4 walls + optional roof) around the given local center point.
	 * 
	 * Walls:
	 *  - Each wall is aligned so the X-axis is its "length."
	 *  - The walls form a square with side length = RoomTotalWallLength, centered at LocalRoomCenter.
	 *  - Each segment of a wall has size = IndividualWallXLength. Randomly picks from WallMeshes.
	 * 
	 * Roof (only if bHasRoof == true):
	 *  - Uses the FIRST mesh in RoofMeshes as the roof tile.
	 *  - Tiled in X and Y directions, each tile is roughly (IndividualRoofXLength x IndividualRoofYLength).
	 *  - Placed at LocalRoomCenter.Z + RoofHeight.
	 *  - Applies random rotation variation from RoofRotationBounds.
	 * 
	 * \param LocalRoomCenter       Center point for the square room (in local space).
	 * \param IndividualWallXLength Length (in X) of a single wall mesh piece.
	 * \param RoomTotalWallLength   Total length of each wall side.
	 * \param bHasRoof              Whether to generate a roof or not.
	 * \param RoofRotationBounds    Random rotation bounds to apply on roof tiles.
	 * \param IndividualRoofXLength Single roof tile length in X.
	 * \param IndividualRoofYLength Single roof tile length in Y.
	 * \param RoofHeight            Z-offset above LocalRoomCenter for the roof.
	 */
	UFUNCTION(BlueprintCallable, Category="Generate")
	void GenerateSquareRoom(
		const FVector LocalRoomCenter = FVector(0,0,0),
		const float IndividualWallXLength = 150.0f,
		const float RoomTotalWallLength  = 500.0f,
		const bool bHasRoof = true,
		FGeneratedRotationBounds RoofRotationBounds= FGeneratedRotationBounds(),
		const float IndividualRoofXLength = 300.0f,
		const float IndividualRoofYLength = 100.0f,
		const float RoofHeight = 300.0f
	);

	/** Wall mesh variations to pick from randomly. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Meshes")
	TArray<TObjectPtr<UStaticMesh>> WallMeshes;

	/** Roof mesh variations. We'll use only the FIRST mesh for the roof. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Meshes")
	TArray<TObjectPtr<UStaticMesh>> RoofMeshes;

	/** Holds references to the instanced mesh components created at runtime. */
	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Hism_Components;

private:

	/**
	 * \brief Gets or creates a HierarchicalInstancedStaticMeshComponent for the specified mesh.
	 * 
	 * \param InMesh The static mesh to find or create a HISM for.
	 * \return A valid UHierarchicalInstancedStaticMeshComponent pointer.
	 */
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateHISM(UStaticMesh* InMesh);

	/**
	 * \brief Helper to generate all four walls around the center point.
	 */
	void GenerateWalls(
		const FVector& LocalCenter,
		float IndividualWallXLength,
		float RoomTotalWallLength
	);

	/**
	 * \brief Generates one wall side (a series of segments) along the X-axis.
	 * 
	 * \param BaseLocation The center location of the wall side (in actor local space).
	 * \param BaseRotation The orientation for the wall side.
	 * \param IndividualWallXLength Size of one wall segment in X.
	 * \param WallTotalLength The total length we want to span.
	 */
	void GenerateWallSegments(
		const FVector& BaseLocation,
		const FRotator& BaseRotation,
		float IndividualWallXLength,
		float WallTotalLength
	);

	/**
	 * \brief Helper to generate the roof if bHasRoof == true.
	 * 
	 * \param LocalCenter The same LocalRoomCenter for positioning.
	 * \param RoomTotalWallLength The total dimension (X and Y) the roof should cover or exceed.
	 * \param IndividualRoofXLength Single roof tile length in X.
	 * \param IndividualRoofYLength Single roof tile length in Y.
	 * \param RoofHeight Z offset from the center.
	 * \param RotationBounds Bounds for random pitch/roll/yaw.
	 */
	void GenerateRoof(
		const FVector& LocalCenter,
		float RoomTotalWallLength,
		float IndividualRoofXLength,
		float IndividualRoofYLength,
		float RoofHeight,
		const FGeneratedRotationBounds& RotationBounds
	);

	/**
	 * \brief Generates a random rotation based on the specified rotation bounds.
	 * 
	 * \param Bounds The rotation bounds for pitch/roll (XY) and yaw (Z).
	 * \return A random FRotator within the specified bounds.
	 */
	FRotator GenerateRandomRotation(const FGeneratedRotationBounds& Bounds) const;
};
