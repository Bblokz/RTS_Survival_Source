#include "FormationMovePreviewComponent.h"

#include "Components/LineBatchComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Player/Formation/FormationMovement.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace FormationMovePreviewPrivate
{
	// A drag shorter than this is a plain click that slipped; it stays a regular formation move.
	constexpr float MinimumLineDragLength = 200.0f;

	// The slots are rebuilt when the cursor moved this far.
	constexpr float RebuildCursorDistance = 25.0f;
	// Upper bound on rebuilds per second while the cursor keeps moving.
	constexpr double MinimumRebuildIntervalSeconds = 0.04;
	// A standing cursor still rebuilds at this pace, because the units walk and the formation turns with them.
	constexpr double IdleRebuildIntervalSeconds = 0.25;

	constexpr float PreviewHeight = 45.0f;
	constexpr float DragPathThickness = 6.0f;
	constexpr float SlotArrowThickness = 4.0f;
	constexpr float ArrowLength = 110.0f;
	constexpr float ArrowHeadLength = 40.0f;
	constexpr float ArrowHeadHalfWidth = 26.0f;

	const FLinearColor DragPathColor(1.0f, 1.0f, 1.0f);
	const FLinearColor SlotArrowColor(0.1f, 0.72f, 0.1f);

	FBatchedLine MakePreviewLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		const float Thickness)
	{
		// A life time of zero keeps the line until the batch is flushed.
		constexpr float LifeTimeUntilFlushed = 0.0f;
		return FBatchedLine(Start, End, Color, LifeTimeUntilFlushed, Thickness, SDPG_World);
	}
}

UFormationMovePreviewComponent::UFormationMovePreviewComponent()
{
	// Driven by the player controller's tick.
	PrimaryComponentTick.bCanEverTick = false;
}

void UFormationMovePreviewComponent::InitFormationMovePreview(UFormationController* FormationController)
{
	M_FormationController = FormationController;
}

void UFormationMovePreviewComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// Owned by the world, not by the controller: a component of a hidden actor is never drawn.
	M_PreviewLineBatch = NewObject<ULineBatchComponent>(World);
	if (not IsValid(M_PreviewLineBatch))
	{
		return;
	}
	// The lines are spread over the map and change often; a fixed giant bounds avoids recomputing it.
	M_PreviewLineBatch->bCalculateAccurateBounds = false;
	M_PreviewLineBatch->RegisterComponentWithWorld(World);
}

void UFormationMovePreviewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Nothing else owns the renderer, so it has to leave the world together with this component.
	if (IsValid(M_PreviewLineBatch))
	{
		M_PreviewLineBatch->DestroyComponent();
	}
	M_PreviewLineBatch = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool UFormationMovePreviewComponent::GetCanPreviewRender() const
{
	return IsValid(M_PreviewLineBatch) && M_PreviewLineBatch->IsRegistered() && M_PreviewLineBatch->ShouldRender();
}

int32 UFormationMovePreviewComponent::GetPreviewLineCount() const
{
	return IsValid(M_PreviewLineBatch) ? M_PreviewLineBatch->BatchedLines.Num() : 0;
}

bool UFormationMovePreviewComponent::GetIsValidFormationController() const
{
	if (M_FormationController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_FormationController",
		"GetIsValidFormationController",
		GetOwner());
	return false;
}

bool UFormationMovePreviewComponent::GetIsValidPreviewLineBatch() const
{
	if (IsValid(M_PreviewLineBatch))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PreviewLineBatch",
		"GetIsValidPreviewLineBatch",
		GetOwner());
	return false;
}

void UFormationMovePreviewComponent::BeginDrag(const FVector& StartLocation)
{
	M_DragPath.Start(StartLocation);
}

void UFormationMovePreviewComponent::EndDrag()
{
	if (not GetIsDragActive())
	{
		return;
	}
	M_DragPath.Reset();
	// The slots on screen were built for the line that is now gone.
	HidePreview();
}

bool UFormationMovePreviewComponent::GetIsLineDragReady() const
{
	return GetIsDragActive() && M_DragPath.GetLength() >= FormationMovePreviewPrivate::MinimumLineDragLength;
}

void UFormationMovePreviewComponent::UpdatePreview(const FFormationMovePreviewInput& Input)
{
	const UWorld* World = GetWorld();
	const bool bHasSelection = Input.SelectedSquads != nullptr && Input.SelectedPawns != nullptr &&
		Input.SelectedActorMasters != nullptr;
	if (not Input.bFormationMoveContext || not bHasSelection || not IsValid(World))
	{
		EndDrag();
		HidePreview();
		return;
	}

	if (GetIsDragActive() && Input.bCursorHit && M_DragPath.TryAddPoint(Input.CursorLocation))
	{
		M_RefreshState.bDragPathChanged = true;
	}
	// While dragging, the line itself is the move target, whatever the cursor happens to be over.
	const bool bHasMoveTarget = GetIsDragActive() || Input.bCursorOnMoveGround;
	if (not bHasMoveTarget)
	{
		HidePreview();
		return;
	}

	const uint32 SelectionHash = BuildSelectionHash(Input);
	// Real time, so the preview keeps following the cursor while the game is paused.
	const double WorldSeconds = World->GetRealTimeSeconds();
	if (not GetShouldRebuild(Input, SelectionHash, WorldSeconds))
	{
		return;
	}
	RebuildSlots(Input);
	M_RefreshState.CursorLocation = Input.CursorLocation;
	M_RefreshState.SelectionHash = SelectionHash;
	M_RefreshState.BuiltAtWorldSeconds = WorldSeconds;
	M_RefreshState.bDragPathChanged = false;
	DrawPreview();
}

bool UFormationMovePreviewComponent::GetShouldRebuild(
	const FFormationMovePreviewInput& Input,
	const uint32 SelectionHash,
	const double WorldSeconds) const
{
	using namespace FormationMovePreviewPrivate;
	if (not M_RefreshState.bIsVisible || SelectionHash != M_RefreshState.SelectionHash)
	{
		return true;
	}
	const double SecondsSinceBuild = WorldSeconds - M_RefreshState.BuiltAtWorldSeconds;
	if (SecondsSinceBuild < MinimumRebuildIntervalSeconds)
	{
		return false;
	}
	// A held drag only changes with its path; the cursor wandering near the last point changes nothing.
	const bool bCursorMoved = not GetIsDragActive() &&
		FVector::DistSquared2D(Input.CursorLocation, M_RefreshState.CursorLocation) >
		FMath::Square(RebuildCursorDistance);
	return M_RefreshState.bDragPathChanged || bCursorMoved || SecondsSinceBuild >= IdleRebuildIntervalSeconds;
}

void UFormationMovePreviewComponent::RebuildSlots(const FFormationMovePreviewInput& Input)
{
	M_SlotLocations.Reset();
	M_SlotRotations.Reset();
	if (not GetIsValidFormationController())
	{
		return;
	}
	// A drag that is still too short previews the regular move to where the button went down.
	const FFormationDragPath* LinePath = GetIsLineDragReady() ? &M_DragPath : nullptr;
	const FVector MoveLocation = GetIsDragActive() ? M_DragPath.GetStartLocation() : Input.CursorLocation;
	M_FormationController->BuildFormationPreview(
		MoveLocation,
		LinePath,
		*Input.SelectedSquads,
		*Input.SelectedPawns,
		*Input.SelectedActorMasters,
		M_SlotLocations,
		M_SlotRotations);
}

uint32 UFormationMovePreviewComponent::BuildSelectionHash(const FFormationMovePreviewInput& Input) const
{
	uint32 SelectionHash = GetTypeHash(GetIsLineDragReady());
	for (const ASquadController* SquadController : *Input.SelectedSquads)
	{
		SelectionHash = HashCombine(SelectionHash, GetTypeHash(SquadController));
	}
	for (const ASelectablePawnMaster* PawnMaster : *Input.SelectedPawns)
	{
		SelectionHash = HashCombine(SelectionHash, GetTypeHash(PawnMaster));
	}
	for (const ASelectableActorObjectsMaster* ActorMaster : *Input.SelectedActorMasters)
	{
		SelectionHash = HashCombine(SelectionHash, GetTypeHash(ActorMaster));
	}
	return SelectionHash;
}

void UFormationMovePreviewComponent::HidePreview()
{
	if (not M_RefreshState.bIsVisible)
	{
		return;
	}
	M_RefreshState.bIsVisible = false;
	M_SlotLocations.Reset();
	M_SlotRotations.Reset();
	if (GetIsValidPreviewLineBatch())
	{
		M_PreviewLineBatch->Flush();
	}
}

void UFormationMovePreviewComponent::DrawPreview()
{
	if (not GetIsValidPreviewLineBatch())
	{
		return;
	}
	TArray<FBatchedLine> PreviewLines;
	AddDragPathLines(PreviewLines);
	for (int32 SlotIndex = 0; SlotIndex < M_SlotLocations.Num(); ++SlotIndex)
	{
		const FRotator SlotRotation = M_SlotRotations.IsValidIndex(SlotIndex)
			                              ? M_SlotRotations[SlotIndex]
			                              : FRotator::ZeroRotator;
		AddSlotArrowLines(M_SlotLocations[SlotIndex], SlotRotation, PreviewLines);
	}

	M_PreviewLineBatch->Flush();
	M_PreviewLineBatch->DrawLines(PreviewLines);
	M_RefreshState.bIsVisible = true;
}

void UFormationMovePreviewComponent::AddDragPathLines(TArray<FBatchedLine>& OutLines) const
{
	using namespace FormationMovePreviewPrivate;
	const TArray<FVector>& PathPoints = M_DragPath.GetPoints();
	const FVector HeightOffset = FVector::UpVector * PreviewHeight;
	for (int32 PointIndex = 1; PointIndex < PathPoints.Num(); ++PointIndex)
	{
		OutLines.Add(MakePreviewLine(
			PathPoints[PointIndex - 1] + HeightOffset,
			PathPoints[PointIndex] + HeightOffset,
			DragPathColor,
			DragPathThickness));
	}
}

void UFormationMovePreviewComponent::AddSlotArrowLines(
	const FVector& SlotLocation,
	const FRotator& SlotRotation,
	TArray<FBatchedLine>& OutLines) const
{
	using namespace FormationMovePreviewPrivate;
	const FRotator FlatRotation(0.0f, SlotRotation.Yaw, 0.0f);
	const FVector Forward = FlatRotation.Vector();
	const FVector Right = FlatRotation.RotateVector(FVector::RightVector);

	// The arrow is centred on the slot, so its middle marks where the unit will stand.
	const FVector ArrowCentre = SlotLocation + FVector::UpVector * PreviewHeight;
	const FVector ArrowTail = ArrowCentre - Forward * (ArrowLength * 0.5f);
	const FVector ArrowTip = ArrowCentre + Forward * (ArrowLength * 0.5f);
	const FVector ArrowHeadBase = ArrowTip - Forward * ArrowHeadLength;

	OutLines.Add(MakePreviewLine(ArrowTail, ArrowTip, SlotArrowColor, SlotArrowThickness));
	OutLines.Add(MakePreviewLine(
		ArrowTip, ArrowHeadBase + Right * ArrowHeadHalfWidth, SlotArrowColor, SlotArrowThickness));
	OutLines.Add(MakePreviewLine(
		ArrowTip, ArrowHeadBase - Right * ArrowHeadHalfWidth, SlotArrowColor, SlotArrowThickness));
}
