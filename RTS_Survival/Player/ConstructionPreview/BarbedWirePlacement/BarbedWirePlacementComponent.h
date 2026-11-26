// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/BarbedWireLine/RTSBarbedWireLine.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "BarbedWirePlacementComponent.generated.h"

class ACPPController;
class URadiusComp;
class UTrainerComponent;

UENUM()
enum class EBarbedWirePlacementPhase : uint8
{
	// No wire is being laid out.
	Idle,
	// One piece follows the mouse and shows where a first click would start the line.
	Hovering,
	// The start is set; the line runs from it to the mouse.
	Drawing
};

/**
 * @brief Lets the player drag out a barbed wire line picked in the HQ's Barriers tab: before the first click one
 * piece follows the mouse, the first click sets where the line starts, the line then follows the mouse, and the second
 * click hands it over to be strung piece by piece (ARTSBarbedWireLine). What is shown is what the confirming click
 * builds; pieces outside the build radii, on too steep ground or on a building cut the line short. Both ends snap to the open ends
 * of the player's other wire (also the gap a destroyed piece left), and the end snaps to the line's own start to
 * close a ring. Nothing is paid here: the line pays for each piece when it is strung.
 * Lives on the player controller, which routes the clicks of its barbed wire preview mode here.
 */
UCLASS(ClassGroup = (Custom))
class RTS_SURVIVAL_API UBarbedWirePlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBarbedWirePlacementComponent();

	/**
	 * @brief Starts laying out a barbed wire line; the next primary click sets its start.
	 * @param Trainer The HQ the wire was picked from; placing stops when it can no longer train.
	 * @param CostPerMetre What the player pays per metre of wire while it is strung.
	 * @param BuildRadii The wire is only laid inside these; none allows it anywhere.
	 */
	void StartPlacement(
		UTrainerComponent* Trainer,
		const TMap<ERTSResourceType, int32>& CostPerMetre,
		const TArray<URadiusComp*>& BuildRadii);

	/**
	 * @brief Sets the start of the line, or confirms where it ends.
	 * @param ClickedLocation The ground under the cursor.
	 * @return True when the line was confirmed and placing is over.
	 */
	bool OnPrimaryClick(const FVector& ClickedLocation);

	/** @brief Stops placing; a line that was not confirmed yet is removed. A confirmed line is kept. */
	void CancelPlacement();

	bool GetIsPlacing() const { return M_Phase != EBarbedWirePlacementPhase::Idle; }
	// The start of the line is set and the line follows the mouse.
	bool GetIsDrawing() const;
	const UTrainerComponent* GetTrainer() const { return M_Trainer.Get(); }

#if RTS_WITH_MAP_TESTS
	/** @brief Makes the line follow this point instead of the mouse; unset to follow the mouse again. */
	void MapTest_SetCursorOverride(const TOptional<FVector>& CursorLocation) { M_MapTestCursorOverride = CursorLocation; }
	const TArray<FBarbedWireSpan>& MapTest_GetPlannedSpans() const { return M_PlannedSpans; }
	ARTSBarbedWireLine* MapTest_GetLastConfirmedLine() const { return M_LastConfirmedLine.Get(); }
	ARTSBarbedWireLine* MapTest_GetPreviewLine() const { return M_PreviewLine.Get(); }
	bool MapTest_GetIsStartSnapped() const { return bM_IsStartSnapped; }
	bool MapTest_GetIsEndSnapped() const { return M_EndSnap.IsSet(); }
#endif

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// The line actor; a Blueprint child may change the wire piece, its ghost material or how fast it is strung.
	UPROPERTY(EditDefaultsOnly, Category = "Barbed Wire")
	TSoftClassPtr<ARTSBarbedWireLine> LineClass;

	// The longest line one drag lays out.
	UPROPERTY(EditDefaultsOnly, Category = "Barbed Wire", meta = (ClampMin = "400.0", Units = "cm"))
	float MaximumLineLength = 6000.0f;

	// The mouse path is recorded in steps of this length; the line follows the recorded path.
	UPROPERTY(EditDefaultsOnly, Category = "Barbed Wire", meta = (ClampMin = "10.0", Units = "cm"))
	float PathPointSpacing = 50.0f;

	// A click or the mouse this close to an open end of the player's wire starts or ends the line on it.
	UPROPERTY(EditDefaultsOnly, Category = "Barbed Wire", meta = (ClampMin = "0.0", Units = "cm"))
	float SnapRadius = 350.0f;

	// An end snapped while drawing lets go once the mouse is this far from it; more than the snap radius, so the
	// line does not flip back and forth at its edge.
	UPROPERTY(EditDefaultsOnly, Category = "Barbed Wire", meta = (ClampMin = "0.0", Units = "cm"))
	float SnapReleaseRadius = 500.0f;

	// The line stops where a piece would have to lean further than this to lie on the ground.
	UPROPERTY(EditDefaultsOnly, Category = "Barbed Wire", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	float MaximumSlope = 20.0f;

	// The end snaps to the line's own start, closing a ring, once the line is at least this many pieces long.
	UPROPERTY(EditDefaultsOnly, Category = "Barbed Wire", meta = (ClampMin = "3"))
	int32 MinimumPiecesToCloseRing = 3;

private:
	TWeakObjectPtr<UTrainerComponent> M_Trainer;
	TArray<TWeakObjectPtr<URadiusComp>> M_BuildRadii;

	UPROPERTY()
	TMap<ERTSResourceType, int32> M_CostPerMetre;

	// Shows the line while it follows the mouse; becomes the real line on the confirming click.
	UPROPERTY()
	TObjectPtr<ARTSBarbedWireLine> M_PreviewLine;

	TWeakObjectPtr<ARTSBarbedWireLine> M_LastConfirmedLine;

	// Ground points the mouse passed since the line was started.
	TArray<FVector2D> M_DragPath;
	// The path and end the shown plan was made for; the plan is only made again when either changed.
	TArray<FVector2D> M_LastPlanPath;
	bool bM_LastPlanEndSnapped = false;
	TArray<FBarbedWireSpan> M_PlannedSpans;
	// The open end of other wire the mouse is near while drawing; the line then ends on it exactly.
	TOptional<FVector> M_EndSnap;
	bool bM_IsStartSnapped = false;
	float M_StartHeight = 0.0f;
	// Which way a line shorter than one piece is laid: screen right before the first click, then the last way the
	// mouse went from the start, so the short line does not flip when the mouse returns to it.
	FVector2D M_ShortLineDirection = FVector2D(1.0f, 0.0f);
	EBarbedWirePlacementPhase M_Phase = EBarbedWirePlacementPhase::Idle;

#if RTS_WITH_MAP_TESTS
	TOptional<FVector> M_MapTestCursorOverride;
#endif

	ACPPController* GetPlayerController() const;
	bool TryGetCursorGround(FVector& OutGround) const;
	bool GetIsWithinBuildRadii(const FVector& Location) const;
	bool GetHasRadiiConstraint() const { return not M_BuildRadii.IsEmpty(); }
	float GetPieceLength() const;

	/**
	 * @brief Spawns the line that shows the plan, if there is none yet.
	 * @return Whether there is one.
	 */
	bool EnsurePreviewLine(const FVector& SpawnLocation);

	/**
	 * @brief The first click: the line starts at the open end it snaps to, or where the player clicked.
	 * @return What OnPrimaryClick returns.
	 */
	bool BeginDrawing(const FVector& ClickedLocation, ACPPController& PlayerController);

	/** @return Where a line started here begins: on the open wire end within the snap radius, or here. */
	FVector GetSnappedStart(const FVector& Location, bool& bOutIsSnapped) const;

	/** @brief Shows the piece a click at the mouse would start. */
	void UpdateHover();

	/** @brief Records the mouse and plans the line along the recorded path up to where the mouse is now. */
	void UpdateDrawing();

	/** @brief Keeps, finds or lets go of the open end the line closes onto, near the end of the path. */
	void UpdateEndSnap(const FVector2D& PathEnd, const float PathLength);

	/**
	 * @brief Plans the line along the path and shows it; red when nothing of it can be built.
	 * @param PlanPath Ground points from the start of the line to the mouse, or to the snapped end.
	 * @param bEndsOnPathEnd The last piece ends exactly at the end of the path.
	 */
	void ShowPlan(TArray<FVector2D> PlanPath, const bool bEndsOnPathEnd);

	/** @brief Replaces a free path shorter than one piece by a straight one piece long. */
	void ExtendToOnePiece(TArray<FVector2D>& InOutPlanPath);

	/** @return Right on the screen, on the ground; the way the line is laid before the mouse showed one. */
	FVector2D GetScreenRightDirection() const;

	/**
	 * @brief Lays the plan onto the ground and cuts it before the first piece the player may not build.
	 * @return The pieces that fit; empty when the first piece does not.
	 */
	TArray<FBarbedWireSpan> KeepBuildablePieces(const TArray<FBarbedWireSpan>& Spans);
	bool GetIsPieceBuildable(const FBarbedWirePieceRuntime& Piece) const;
	void ResetPlacement();
};
