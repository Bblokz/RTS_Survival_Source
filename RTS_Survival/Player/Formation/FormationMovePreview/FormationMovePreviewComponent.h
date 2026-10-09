#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/Formation/FormationDragPath.h"
#include "FormationMovePreviewComponent.generated.h"

class ASquadController;
class ASelectablePawnMaster;
class ASelectableActorObjectsMaster;
class UFormationController;
class ULineBatchComponent;
struct FBatchedLine;

/** What the player controller knows this frame that decides whether and where a formation move is previewed. */
struct FFormationMovePreviewInput
{
	// The current selection; only read during the call.
	const TArray<ASquadController*>* SelectedSquads = nullptr;
	const TArray<ASelectablePawnMaster*>* SelectedPawns = nullptr;
	const TArray<ASelectableActorObjectsMaster*>* SelectedActorMasters = nullptr;

	// True when the selection moves through the formation controller and no other cursor mode is active.
	bool bFormationMoveContext = false;

	bool bCursorHit = false;

	// False when the cursor is over an actor that a click would target instead of move to.
	bool bCursorOnMoveGround = false;
	FVector CursorLocation = FVector::ZeroVector;
};

/** Remembers what the slots on screen were built from, so they are only rebuilt when something relevant changed. */
struct FFormationMovePreviewRefreshState
{
	FVector CursorLocation = FVector::ZeroVector;
	double BuiltAtWorldSeconds = 0.0;
	uint32 SelectionHash = 0;
	bool bIsVisible = false;

	// Set when the dragged line grew since the slots were last built.
	bool bDragPathChanged = false;
};

/**
 * @brief Shows where a formation move would put every selected unit for the current cursor position, and
 * records the line the player drags with the secondary button so the units can be spread along it.
 * The player controller calls UpdatePreview every tick and BeginDrag / EndDrag around the secondary click.
 * @note InitFormationMovePreview: call from the owning controller before the first UpdatePreview.
 */
UCLASS()
class RTS_SURVIVAL_API UFormationMovePreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFormationMovePreviewComponent();

	void InitFormationMovePreview(UFormationController* FormationController);

	/**
	 * @brief Extends an active drag, rebuilds the slots when needed and redraws them.
	 * @param Input This frame's selection and cursor state from the player controller.
	 */
	void UpdatePreview(const FFormationMovePreviewInput& Input);

	/** Starts recording the dragged line at the ground location the secondary button went down on. */
	void BeginDrag(const FVector& StartLocation);

	/** Forgets the dragged line; the next order is a regular formation move again. */
	void EndDrag();

	bool GetIsDragActive() const { return M_DragPath.GetIsStarted(); }

	/** @return True once the drag is long enough that releasing it spreads the units along the line. */
	bool GetIsLineDragReady() const;

	const FFormationDragPath& GetDragPath() const { return M_DragPath; }

	/** @return True when the renderer exists and the engine will actually draw it in the game view. */
	bool GetCanPreviewRender() const;

	/** @return Number of line segments of the preview that is on screen right now. */
	int32 GetPreviewLineCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<UFormationController> M_FormationController;

	bool GetIsValidFormationController() const;

	// Line renderer; its lines stay until flushed, so the preview is only redrawn when it changed.
	// Registered with the world instead of the owning controller: controllers are hidden actors and the
	// engine draws nothing that belongs to a hidden actor.
	UPROPERTY()
	TObjectPtr<ULineBatchComponent> M_PreviewLineBatch;

	bool GetIsValidPreviewLineBatch() const;

	FFormationDragPath M_DragPath;
	FFormationMovePreviewRefreshState M_RefreshState;

	// Slots of the preview on screen, parallel arrays.
	TArray<FVector> M_SlotLocations;
	TArray<FRotator> M_SlotRotations;

	bool GetShouldRebuild(
		const FFormationMovePreviewInput& Input,
		uint32 SelectionHash,
		double WorldSeconds) const;
	void RebuildSlots(const FFormationMovePreviewInput& Input);
	uint32 BuildSelectionHash(const FFormationMovePreviewInput& Input) const;

	void HidePreview();
	void DrawPreview();
	void AddDragPathLines(TArray<FBatchedLine>& OutLines) const;
	void AddSlotArrowLines(const FVector& SlotLocation, const FRotator& SlotRotation,
	                       TArray<FBatchedLine>& OutLines) const;
};
