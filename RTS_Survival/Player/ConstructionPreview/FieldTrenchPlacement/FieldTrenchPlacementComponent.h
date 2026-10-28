// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/FieldTrench/FieldTrenchLayout.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "FieldTrenchPlacementComponent.generated.h"

class ACPPController;
class ARTSFieldTrench;
class URadiusComp;
class UTrainerComponent;

/**
 * @brief Lets the player lay out a field trench picked in the HQ's training menu: the first click sets where the
 * trench starts, the trench then follows the mouse, and the second click ends it with an end cap and hands it
 * over to be dug. The trench is drawn as ghosts while it follows the mouse and is only laid where the player
 * may build. Nothing is paid here: the trench pays for each piece when its digging starts.
 * Lives on the player controller, which routes the clicks of its field trench preview mode here.
 */
UCLASS(ClassGroup = (Custom))
class RTS_SURVIVAL_API UFieldTrenchPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFieldTrenchPlacementComponent();

	/**
	 * @brief Starts laying out a trench; the next primary click sets its start.
	 * @param Trainer The HQ the trench was picked from; placing stops when it can no longer train.
	 * @param CostPerMetre What the player pays per metre of trench while it is dug.
	 * @param BuildRadii The trench is only laid inside these; none allows it anywhere.
	 */
	void StartPlacement(
		UTrainerComponent* Trainer,
		const TMap<ERTSResourceType, int32>& CostPerMetre,
		const TArray<URadiusComp*>& BuildRadii);

	/**
	 * @brief Sets the start of the trench, or confirms where it ends.
	 * @param ClickedLocation The ground under the cursor.
	 * @return True when the trench was confirmed and placing is over.
	 */
	bool OnPrimaryClick(const FVector& ClickedLocation);

	/** @brief Stops placing; a trench that was not confirmed yet is removed. A confirmed trench is kept. */
	void CancelPlacement();

	bool GetIsPlacing() const { return bM_IsPlacing; }
	// The start of the trench is set and the trench follows the mouse.
	bool GetIsDrawing() const;
	const UTrainerComponent* GetTrainer() const { return M_Trainer.Get(); }

#if RTS_WITH_MAP_TESTS
	/** @brief Makes the trench follow this point instead of the mouse; unset to follow the mouse again. */
	void MapTest_SetCursorOverride(const TOptional<FVector>& CursorLocation) { M_MapTestCursorOverride = CursorLocation; }
	const TArray<FFieldTrenchPiecePlacement>& MapTest_GetPlannedPieces() const { return M_PlannedPieces; }
	ARTSFieldTrench* MapTest_GetLastConfirmedTrench() const { return M_LastConfirmedTrench.Get(); }
#endif

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// The trench actor; a Blueprint child may change the ghost material, dig speed or collision of the pieces.
	UPROPERTY(EditDefaultsOnly, Category = "Field Trench")
	TSoftClassPtr<ARTSFieldTrench> TrenchClass;

	// The longest trench one drag lays out.
	UPROPERTY(EditDefaultsOnly, Category = "Field Trench", meta = (ClampMin = "500.0", Units = "cm"))
	float MaximumTrenchLength = 6000.0f;

	// The mouse path is recorded in steps of this length; the trench follows the recorded path.
	UPROPERTY(EditDefaultsOnly, Category = "Field Trench", meta = (ClampMin = "10.0", Units = "cm"))
	float PathPointSpacing = 50.0f;

	// The trench stops where a piece would have to lean further than this to lie on the ground.
	UPROPERTY(EditDefaultsOnly, Category = "Field Trench", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	float MaximumSlope = 15.0f;

private:
	TWeakObjectPtr<UTrainerComponent> M_Trainer;
	TArray<TWeakObjectPtr<URadiusComp>> M_BuildRadii;

	UPROPERTY()
	TMap<ERTSResourceType, int32> M_CostPerMetre;

	// Shows the trench while it follows the mouse; becomes the real trench on the confirming click.
	UPROPERTY()
	TObjectPtr<ARTSFieldTrench> M_PreviewTrench;

	TWeakObjectPtr<ARTSFieldTrench> M_LastConfirmedTrench;

	// Ground points the mouse passed since the trench was started.
	TArray<FVector2D> M_DragPath;
	// The path the shown plan was made for; the plan is only made again when the path changed.
	TArray<FVector2D> M_LastPlanPath;
	TArray<FFieldTrenchPiecePlacement> M_PlannedPieces;
	float M_StartHeight = 0.0f;
	bool bM_IsPlacing = false;

#if RTS_WITH_MAP_TESTS
	TOptional<FVector> M_MapTestCursorOverride;
#endif

	ACPPController* GetPlayerController() const;
	bool TryGetCursorGround(FVector& OutGround) const;
	bool GetIsWithinBuildRadii(const FVector& Location) const;
	bool GetHasRadiiConstraint() const { return not M_BuildRadii.IsEmpty(); }

	/** @brief Records the mouse and plans the trench along the recorded path up to where the mouse is now. */
	void UpdateDrawing();

	/**
	 * @brief Plans the trench along the path and shortens it until every piece lies where the player may build.
	 * @return The pieces that fit.
	 */
	TArray<FFieldTrenchPiecePlacement> PlanBuildableTrench(const TArray<FVector2D>& DragPath);
	bool GetArePiecesBuildable(const TArray<FFieldTrenchPiecePlacement>& Pieces);
	void ResetPlacement();
};
