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
	// The slots are rebuilt when the cursor moved this far, or the held arrow got this much longer or shorter.
	constexpr float RebuildCursorDistance = 25.0f;
	// The slots are rebuilt when the held arrow turned this far.
	constexpr float RebuildArrowYawDegrees = 1.0f;
	// Upper bound on rebuilds per second while the cursor keeps moving.
	constexpr double MinimumRebuildIntervalSeconds = 0.04;
	// A standing cursor still rebuilds at this pace, because the units walk and the formation turns with them.
	constexpr double IdleRebuildIntervalSeconds = 0.25;

	constexpr float PreviewHeight = 45.0f;
	constexpr float FrontLineThickness = 6.0f;
	constexpr float SlotArrowThickness = 4.0f;
	constexpr float ArrowLength = 110.0f;
	constexpr float ArrowHeadLength = 40.0f;
	constexpr float ArrowHeadHalfWidth = 26.0f;

	const FLinearColor FrontLineColor(1.0f, 1.0f, 1.0f);
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
	M_DragState = FFormationMovePreviewDragState();
	M_DragState.bIsActive = true;
	// The rotation arrow stands where the button went down, so that is where the formation's front is anchored.
	M_DragState.ArrowDrag.AnchorLocation = StartLocation;
}

void UFormationMovePreviewComponent::EndDrag()
{
	if (not GetIsDragActive())
	{
		return;
	}
	M_DragState = FFormationMovePreviewDragState();
	// The slots on screen were built for the drag that is now gone.
	HidePreview();
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

	if (GetIsDragActive())
	{
		UpdateArrowDrag(Input);
	}
	// While dragging, the arrow's anchor is the move target, whatever the cursor happens to be over.
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
	M_RefreshState.ArrowYaw = M_DragState.ArrowDrag.Facing.Yaw;
	M_RefreshState.ArrowLength = M_DragState.ArrowDrag.ArrowLength;
	DrawPreview();
}

void UFormationMovePreviewComponent::UpdateArrowDrag(const FFormationMovePreviewInput& Input)
{
	if (not Input.bArrowFacingChosen)
	{
		return;
	}
	M_DragState.bIsArrowStarted = true;
	FFormationArrowDrag& ArrowDrag = M_DragState.ArrowDrag;
	ArrowDrag.Facing = FRotator(0.0f, Input.ArrowRotation.Yaw, 0.0f);
	if (Input.bCursorHit)
	{
		ArrowDrag.ArrowLength = FVector::Dist2D(ArrowDrag.AnchorLocation, Input.CursorLocation);
	}
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
	// A held drag only changes with its arrow; the cursor itself is not the move target then.
	const bool bCursorMoved = not GetIsDragActive() &&
		FVector::DistSquared2D(Input.CursorLocation, M_RefreshState.CursorLocation) >
		FMath::Square(RebuildCursorDistance);
	return GetHasArrowChangedSinceBuild() || bCursorMoved || SecondsSinceBuild >= IdleRebuildIntervalSeconds;
}

bool UFormationMovePreviewComponent::GetHasArrowChangedSinceBuild() const
{
	using namespace FormationMovePreviewPrivate;
	if (not GetIsArrowFormationReady())
	{
		return false;
	}
	const FFormationArrowDrag& ArrowDrag = M_DragState.ArrowDrag;
	const float YawChange = FMath::Abs(FRotator::NormalizeAxis(ArrowDrag.Facing.Yaw - M_RefreshState.ArrowYaw));
	const float LengthChange = FMath::Abs(ArrowDrag.ArrowLength - M_RefreshState.ArrowLength);
	return YawChange > RebuildArrowYawDegrees || LengthChange > RebuildCursorDistance;
}

void UFormationMovePreviewComponent::RebuildSlots(const FFormationMovePreviewInput& Input)
{
	M_SlotLocations.Reset();
	M_SlotRotations.Reset();
	if (not GetIsValidFormationController())
	{
		return;
	}
	if (GetIsArrowFormationReady())
	{
		M_FormationController->BuildArrowFormationPreview(
			M_DragState.ArrowDrag,
			*Input.SelectedSquads,
			*Input.SelectedPawns,
			*Input.SelectedActorMasters,
			M_SlotLocations,
			M_SlotRotations,
			M_DragState.FrontLine);
		return;
	}
	M_DragState.FrontLine.Reset();
	// Until the rotation arrow appears, a held button previews the quick click on where it went down.
	const FVector MoveLocation = GetIsDragActive() ? M_DragState.ArrowDrag.AnchorLocation : Input.CursorLocation;
	M_FormationController->BuildFormationPreview(
		MoveLocation,
		*Input.SelectedSquads,
		*Input.SelectedPawns,
		*Input.SelectedActorMasters,
		M_SlotLocations,
		M_SlotRotations);
}

uint32 UFormationMovePreviewComponent::BuildSelectionHash(const FFormationMovePreviewInput& Input) const
{
	uint32 SelectionHash = GetTypeHash(GetIsArrowFormationReady());
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
	AddFrontLineLines(PreviewLines);
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

void UFormationMovePreviewComponent::AddFrontLineLines(TArray<FBatchedLine>& OutLines) const
{
	using namespace FormationMovePreviewPrivate;
	const TArray<FVector>& PathPoints = M_DragState.FrontLine.GetPoints();
	const FVector HeightOffset = FVector::UpVector * PreviewHeight;
	for (int32 PointIndex = 1; PointIndex < PathPoints.Num(); ++PointIndex)
	{
		OutLines.Add(MakePreviewLine(
			PathPoints[PointIndex - 1] + HeightOffset,
			PathPoints[PointIndex] + HeightOffset,
			FrontLineColor,
			FrontLineThickness));
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
