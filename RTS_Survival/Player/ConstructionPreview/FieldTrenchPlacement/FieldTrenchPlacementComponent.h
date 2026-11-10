// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/FieldTrench/FieldTrenchLayout.h"
#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "FieldTrenchPlacementComponent.generated.h"

class ACPPController;
class ARTSFieldTrench;
class URadiusComp;
class UTrainerComponent;

UENUM()
enum class EFieldTrenchPlacementPhase : uint8
{
	// No trench is being laid out.
	Idle,
	// A short trench follows the mouse and shows where a first click would start it.
	Hovering,
	// The start is set; the trench runs from it to the mouse.
	Drawing
};

/**
 * @brief Lets the player lay out a field trench picked in the HQ's training menu: before the first click a short
 * trench follows the mouse, the first click sets where the trench starts, the trench then follows the mouse, and
 * the second click ends it with an end cap and hands it over to be dug. The trench shown is never shorter than
 * MinimumShownLength, and what is shown is what the confirming click builds. The trench is drawn as ghosts while it follows the mouse and is only laid where the player
 * may build. Nothing is paid here: the trench pays for each piece when its digging starts.
 * A trench joins the player's trench anchors (UFieldTrenchAnchorComponent), such as the sockets at the back of
 * a pit: a first click near a free anchor starts the trench on it, and while the start is free, moving the
 * mouse near one ends the trench there. A trench joins one anchor; it is always planned from the anchor out,
 * so the pieces fit the anchor exactly and its free end gets the cap.
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

	bool GetIsPlacing() const { return M_Phase != EFieldTrenchPlacementPhase::Idle; }
	// The start of the trench is set and the trench follows the mouse.
	bool GetIsDrawing() const;
	const UTrainerComponent* GetTrainer() const { return M_Trainer.Get(); }

#if RTS_WITH_MAP_TESTS
	/** @brief Makes the trench follow this point instead of the mouse; unset to follow the mouse again. */
	void MapTest_SetCursorOverride(const TOptional<FVector>& CursorLocation) { M_MapTestCursorOverride = CursorLocation; }
	const TArray<FFieldTrenchPiecePlacement>& MapTest_GetPlannedPieces() const { return M_PlannedPieces; }
	ARTSFieldTrench* MapTest_GetLastConfirmedTrench() const { return M_LastConfirmedTrench.Get(); }
	ARTSFieldTrench* MapTest_GetPreviewTrench() const { return M_PreviewTrench.Get(); }
	const FFieldTrenchAnchorHandle& MapTest_GetPlanAnchor() const { return M_LastPlanAnchor; }
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

	// A click or the mouse this close to a free trench anchor joins the trench to it.
	UPROPERTY(EditDefaultsOnly, Category = "Field Trench", meta = (ClampMin = "0.0", Units = "cm"))
	float AnchorSnapRadius = 350.0f;

	// A trench joined to an anchor at its end lets go of it once the mouse is this far from it; more than the
	// snap radius, so the plan does not flip back and forth at its edge.
	UPROPERTY(EditDefaultsOnly, Category = "Field Trench", meta = (ClampMin = "0.0", Units = "cm"))
	float AnchorReleaseRadius = 500.0f;

	// The trench shown before the first click, and while the mouse is still close to the start, is this long, so
	// the player always sees what a click builds. More than the shortest straight (125 cm), or nothing is planned.
	UPROPERTY(EditDefaultsOnly, Category = "Field Trench", meta = (ClampMin = "130.0", Units = "cm"))
	float MinimumShownLength = 150.0f;

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
	// The anchor the first click was on.
	FFieldTrenchAnchorHandle M_StartAnchor;
	// The anchor the mouse is near while the start is free; the trench is then planned from it.
	FFieldTrenchAnchorHandle M_EndAnchor;
	// The anchor the shown plan starts at.
	FFieldTrenchAnchorHandle M_LastPlanAnchor;
	float M_StartHeight = 0.0f;
	// Which way a trench shorter than MinimumShownLength is laid: screen right before the first click, then the
	// last way the mouse went from the start, so the short trench does not flip when the mouse returns to it.
	FVector2D M_ShortTrenchDirection = FVector2D(1.0f, 0.0f);
	EFieldTrenchPlacementPhase M_Phase = EFieldTrenchPlacementPhase::Idle;

#if RTS_WITH_MAP_TESTS
	TOptional<FVector> M_MapTestCursorOverride;
#endif

	ACPPController* GetPlayerController() const;
	bool TryGetCursorGround(FVector& OutGround) const;
	bool GetIsWithinBuildRadii(const FVector& Location) const;
	bool GetHasRadiiConstraint() const { return not M_BuildRadii.IsEmpty(); }

	/**
	 * @brief Spawns the trench that shows the plan, if there is none yet.
	 * @return Whether there is one.
	 */
	bool EnsurePreviewTrench(const FVector& SpawnLocation);

	/**
	 * @brief The first click: the trench starts at the anchor it snaps to, or where the player clicked.
	 * @return What OnPrimaryClick returns.
	 */
	bool BeginDrawing(const FVector& ClickedLocation, ACPPController& PlayerController);

	/** @brief Shows the short trench a click at the mouse would start: on the anchor it snaps to, or at the mouse. */
	void UpdateHover();

	/** @brief Records the mouse and plans the trench along the recorded path up to where the mouse is now. */
	void UpdateDrawing();

	/**
	 * @brief Plans the trench along the path and shows it; red when nothing of it can be built.
	 * @param PlanPath Ground points from the start of the trench to the mouse.
	 * @param PlanAnchor The anchor the trench is planned from; unset starts it with a cap.
	 * @param bIsJoinedAtEnd The anchor is at the end of the path: the trench is planned from there back to the start.
	 */
	void ShowPlan(TArray<FVector2D> PlanPath, const FFieldTrenchAnchorHandle& PlanAnchor, const bool bIsJoinedAtEnd);

	/**
	 * @brief Starts the path on the anchor's port and drops points behind the anchor, where the mouse over its
	 * owner would turn the trench back into it. Without an anchor the path is left as it is.
	 */
	void StartPathOnAnchor(TArray<FVector2D>& InOutPlanPath, FFieldTrenchPlanOptions& InOutOptions,
	                       const FFieldTrenchAnchorHandle& PlanAnchor, const bool bIsJoinedAtEnd) const;

	/** @brief Replaces a path that stays within MinimumShownLength of its start by a straight that long. */
	void ExtendToMinimumShownLength(TArray<FVector2D>& InOutPlanPath, const FFieldTrenchPlanOptions& Options);

	/** @return Right on the screen, on the ground; the way a trench is laid before the mouse showed one. */
	FVector2D GetScreenRightDirection() const;

	/** @return The location of the anchor, or the fallback when it is unset. */
	static FVector GetAnchorOrFallbackLocation(const FFieldTrenchAnchorHandle& Anchor, const FVector& Fallback);

	/**
	 * @param Location Where the click or the mouse is.
	 * @param CurrentAnchor The anchor joined so far; kept while the location is within the release radius.
	 * @return The free anchor of the player the trench joins there; unset for none.
	 */
	FFieldTrenchAnchorHandle FindAnchorNear(const FVector2D& Location,
	                                        const FFieldTrenchAnchorHandle& CurrentAnchor) const;
	static bool GetIsAnchorFree(const FFieldTrenchAnchorHandle& Anchor);

	/**
	 * @brief Plans the trench along the path and shortens it until every piece lies where the player may build.
	 * @return The pieces that fit.
	 */
	TArray<FFieldTrenchPiecePlacement> PlanBuildableTrench(const TArray<FVector2D>& DragPath,
	                                                       const FFieldTrenchPlanOptions& Options);
	bool GetArePiecesBuildable(const TArray<FFieldTrenchPiecePlacement>& Pieces);
	void ResetPlacement();
};
