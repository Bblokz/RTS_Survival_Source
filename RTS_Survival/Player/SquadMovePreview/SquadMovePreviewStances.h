#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnitPlannedPosition.h"
#include "SquadMovePreviewStances.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
struct FSquadMovePlan;

/** The stance meshes a planned soldier position can be shown with. */
enum class ESquadPreviewStance : uint8
{
	NoCover,
	CrouchCover,
	// Shown for standing cover on either side.
	HighCover,
	ProneCover,
	TrenchCover,
	Count
};

/**
 * @brief Shows the squad move preview as stance meshes, one instanced mesh component per stance.
 * Owned by the squad move preview component: Setup once, ShowPlan whenever the plan changed, Hide when the
 * preview goes away. Instances are pooled and only touched when the plan changes, never per frame.
 */
USTRUCT()
struct FSquadMovePreviewStances
{
	GENERATED_BODY()

	/**
	 * @brief Spawns the actor that carries the instanced mesh components and fills each component with its
	 * preloaded, parked instances. The components cannot live on the player controller: a controller is a
	 * hidden actor, and nothing owned by a hidden actor is drawn.
	 * @param World World the preview is shown in.
	 */
	void Setup(UWorld& World);
	void Destroy();

	// Places one stance mesh on every position of the plan, turned to the position's facing.
	void ShowPlan(const FSquadMovePlan& Plan);
	void Hide();

	static ESquadPreviewStance GetStanceForPosition(const FSquadUnitPlannedPosition& Position);

	int32 GetShownInstanceCount(ESquadPreviewStance Stance) const;
	int32 GetShownInstanceCount() const;

	/**
	 * @brief Reads back where a shown stance mesh stands, for the preview's map test.
	 * @param Stance Stance whose instances to read.
	 * @param ShownIndex Index below GetShownInstanceCount(Stance).
	 * @param OutTransform World transform of that instance.
	 * @return False when there is no such shown instance.
	 */
	bool TryGetShownInstanceTransform(ESquadPreviewStance Stance, int32 ShownIndex, FTransform& OutTransform) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> M_StanceActor = nullptr;

	// One component per ESquadPreviewStance, in enum order; an entry is null when its mesh is not set.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> M_StanceComponents;

	// How many instances of each component currently show a position; the rest of its pool is parked.
	TArray<int32> M_ShownInstanceCounts;

	float M_StanceYawOffsetDegrees = 0.0f;
	bool bM_IsVisible = false;

	UInstancedStaticMeshComponent* CreateStanceComponent(AActor& Owner, UStaticMesh* StanceMesh, int32 PreloadCount) const;

	/**
	 * @brief Moves a stance's pooled instances onto the given transforms and parks the ones no longer needed.
	 * @param StanceIndex Index of the stance in enum order.
	 * @param Transforms World transforms wanted for this stance; the pool grows when it is too small.
	 */
	void ApplyStanceTransforms(int32 StanceIndex, const TArray<FTransform>& Transforms);
	void SetComponentsVisible(bool bVisible);
};
