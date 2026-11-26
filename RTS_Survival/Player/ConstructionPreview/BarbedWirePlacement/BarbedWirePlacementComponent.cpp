// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BarbedWirePlacementComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerError/PlayerError.h"
#include "RTS_Survival/RTSComponents/RadiusComp/RadiusComp.h"

namespace BarbedWirePlacementPrivate
{
	// The local player; barbed wire lines are only laid out by the player.
	constexpr int32 PlayerOwnedTeam = 1;
	// A short line turns to the mouse once the mouse is this far from the start; closer, a shaking hand would spin it.
	constexpr float MinimumShortLineAimCm = 25.0f;
	// An open end this close to the start of the line is the end the line started on; the end never snaps back to it.
	constexpr float StartEndTolerance = 75.0f;
	// From the camera's forward to screen right.
	constexpr float ScreenRightYawOffsetDegrees = 90.0f;

	float GetPathLength(const TArray<FVector2D>& Path)
	{
		float Length = 0.0f;
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			Length += FVector2D::Distance(Path[PointIndex - 1], Path[PointIndex]);
		}
		return Length;
	}
}

UBarbedWirePlacementComponent::UBarbedWirePlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	LineClass = ARTSBarbedWireLine::StaticClass();
}

void UBarbedWirePlacementComponent::StartPlacement(
	UTrainerComponent* Trainer,
	const TMap<ERTSResourceType, int32>& CostPerMetre,
	const TArray<URadiusComp*>& BuildRadii)
{
	CancelPlacement();
	M_Trainer = Trainer;
	M_CostPerMetre = CostPerMetre;
	M_BuildRadii.Reset();
	for (URadiusComp* BuildRadius : BuildRadii)
	{
		M_BuildRadii.Add(BuildRadius);
	}
	M_Phase = EBarbedWirePlacementPhase::Hovering;
	SetComponentTickEnabled(true);
	// Shown at once, not only from the next tick.
	UpdateHover();
}

bool UBarbedWirePlacementComponent::OnPrimaryClick(const FVector& ClickedLocation)
{
	ACPPController* PlayerController = GetPlayerController();
	if (not GetIsPlacing() || not IsValid(PlayerController))
	{
		return true;
	}
	if (M_Phase == EBarbedWirePlacementPhase::Hovering)
	{
		return BeginDrawing(ClickedLocation, *PlayerController);
	}
	if (not IsValid(M_PreviewLine))
	{
		return true;
	}
	UpdateDrawing();
	if (M_PlannedSpans.IsEmpty())
	{
		// Nothing of it lies where the player may build.
		PlayerController->DisplayErrorMessage(EPlayerError::Error_CannotBuildHere);
		return false;
	}
	M_PreviewLine->StartConstruction(BarbedWirePlacementPrivate::PlayerOwnedTeam, M_CostPerMetre);
	M_LastConfirmedLine = M_PreviewLine;
	// Handed over: the line strings itself from here on and must outlive the placement.
	M_PreviewLine = nullptr;
	ResetPlacement();
	return true;
}

bool UBarbedWirePlacementComponent::GetIsDrawing() const
{
	return M_Phase == EBarbedWirePlacementPhase::Drawing && IsValid(M_PreviewLine);
}

void UBarbedWirePlacementComponent::CancelPlacement()
{
	if (IsValid(M_PreviewLine))
	{
		M_PreviewLine->Destroy();
	}
	M_PreviewLine = nullptr;
	ResetPlacement();
}

void UBarbedWirePlacementComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (not GetIsPlacing())
	{
		SetComponentTickEnabled(false);
		return;
	}
	const UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Trainer) || not Trainer->GetIsAbleToTrain())
	{
		// The HQ died or packed up while the player was laying the wire out.
		if (ACPPController* PlayerController = GetPlayerController())
		{
			PlayerController->StopBarbedWirePlacement();
		}
		else
		{
			CancelPlacement();
		}
		return;
	}
	if (M_Phase == EBarbedWirePlacementPhase::Hovering)
	{
		UpdateHover();
	}
	else if (GetIsDrawing())
	{
		UpdateDrawing();
	}
}

ACPPController* UBarbedWirePlacementComponent::GetPlayerController() const
{
	return Cast<ACPPController>(GetOwner());
}

bool UBarbedWirePlacementComponent::TryGetCursorGround(FVector& OutGround) const
{
#if RTS_WITH_MAP_TESTS
	if (M_MapTestCursorOverride.IsSet())
	{
		OutGround = M_MapTestCursorOverride.GetValue();
		return true;
	}
#endif
	const ACPPController* PlayerController = GetPlayerController();
	if (not IsValid(PlayerController))
	{
		return false;
	}
	bool bIsValidCursor = false;
	OutGround = PlayerController->GetCursorWorldPosition(DeveloperSettings::UIUX::SightDistanceMouse, bIsValidCursor);
	return bIsValidCursor;
}

bool UBarbedWirePlacementComponent::GetIsWithinBuildRadii(const FVector& Location) const
{
	for (const TWeakObjectPtr<URadiusComp>& BuildRadius : M_BuildRadii)
	{
		const URadiusComp* RadiusComp = BuildRadius.Get();
		if (IsValid(RadiusComp) && IsValid(RadiusComp->GetOwner()) &&
			FVector::Dist(Location, RadiusComp->GetOwner()->GetActorLocation()) <= RadiusComp->GetRadius())
		{
			return true;
		}
	}
	return false;
}

float UBarbedWirePlacementComponent::GetPieceLength() const
{
	return IsValid(M_PreviewLine)
		       ? M_PreviewLine->GetPieceLength()
		       : GetDefault<ARTSBarbedWireLine>()->GetPieceLength();
}

bool UBarbedWirePlacementComponent::EnsurePreviewLine(const FVector& SpawnLocation)
{
	if (IsValid(M_PreviewLine))
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	UClass* SpawnClass = LineClass.LoadSynchronous();
	if (not IsValid(SpawnClass))
	{
		SpawnClass = ARTSBarbedWireLine::StaticClass();
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_PreviewLine = World->SpawnActor<ARTSBarbedWireLine>(SpawnClass, FTransform(SpawnLocation), SpawnParameters);
	return IsValid(M_PreviewLine);
}

bool UBarbedWirePlacementComponent::BeginDrawing(const FVector& ClickedLocation, ACPPController& PlayerController)
{
	bool bIsSnapped = false;
	const FVector StartLocation = GetSnappedStart(ClickedLocation, bIsSnapped);
	if (GetHasRadiiConstraint() && not GetIsWithinBuildRadii(StartLocation))
	{
		PlayerController.DisplayErrorMessage(EPlayerError::Error_CannotBuildHere);
		return false;
	}
	if (not EnsurePreviewLine(StartLocation))
	{
		return true;
	}
	M_PreviewLine->SetActorLocation(StartLocation);
	M_DragPath = {FVector2D(StartLocation)};
	M_LastPlanPath.Reset();
	M_PlannedSpans.Reset();
	M_EndSnap.Reset();
	bM_IsStartSnapped = bIsSnapped;
	M_StartHeight = StartLocation.Z;
	M_Phase = EBarbedWirePlacementPhase::Drawing;
	// The piece the hover showed stays up instead of vanishing for a frame.
	UpdateDrawing();
	return false;
}

FVector UBarbedWirePlacementComponent::GetSnappedStart(const FVector& Location, bool& bOutIsSnapped) const
{
	FVector OpenEnd;
	bOutIsSnapped = ARTSBarbedWireLine::FindOpenEndNear(
		GetWorld(), FVector2D(Location), SnapRadius, BarbedWirePlacementPrivate::PlayerOwnedTeam, M_PreviewLine,
		OpenEnd);
	return bOutIsSnapped ? OpenEnd : Location;
}

void UBarbedWirePlacementComponent::UpdateHover()
{
	FVector CursorGround;
	if (not TryGetCursorGround(CursorGround))
	{
		return;
	}
	// Exactly what a click here would start: on the open end the click snaps to, or at the mouse.
	const FVector StartLocation = GetSnappedStart(CursorGround, bM_IsStartSnapped);
	if (not EnsurePreviewLine(StartLocation))
	{
		return;
	}
	M_StartHeight = StartLocation.Z;
	M_ShortLineDirection = GetScreenRightDirection();
	ShowPlan({FVector2D(StartLocation)}, false);
}

void UBarbedWirePlacementComponent::UpdateDrawing()
{
	FVector CursorGround;
	TArray<FVector2D> PlanPath = M_DragPath;
	if (TryGetCursorGround(CursorGround))
	{
		const FVector2D Cursor(CursorGround);
		if (FVector2D::Distance(M_DragPath.Last(), Cursor) >= PathPointSpacing)
		{
			M_DragPath.Add(Cursor);
			PlanPath = M_DragPath;
		}
		else if (not Cursor.Equals(PlanPath.Last()))
		{
			// Between recorded steps the line still reaches to the mouse.
			PlanPath.Add(Cursor);
		}
	}
	UpdateEndSnap(PlanPath.Last(), BarbedWirePlacementPrivate::GetPathLength(PlanPath));
	if (M_EndSnap.IsSet())
	{
		// The line closes onto the open end instead of the mouse beside it.
		const FVector2D SnappedEnd(M_EndSnap.GetValue());
		if (PlanPath.Num() > 1)
		{
			PlanPath.Last() = SnappedEnd;
		}
		else
		{
			PlanPath.Add(SnappedEnd);
		}
	}
	ShowPlan(MoveTemp(PlanPath), M_EndSnap.IsSet());
}

void UBarbedWirePlacementComponent::UpdateEndSnap(const FVector2D& PathEnd, const float PathLength)
{
	using namespace BarbedWirePlacementPrivate;
	const FVector2D LineStart = M_DragPath.IsEmpty() ? PathEnd : M_DragPath[0];
	const float Radius = M_EndSnap.IsSet() ? SnapReleaseRadius : SnapRadius;
	// A long line closes a ring on its own start.
	const bool bIsLongEnoughForRing = PathLength >= MinimumPiecesToCloseRing * GetPieceLength();
	if (bIsLongEnoughForRing && FVector2D::Distance(PathEnd, LineStart) <= Radius)
	{
		M_EndSnap = FVector(LineStart, M_StartHeight);
		return;
	}
	FVector OpenEnd;
	const bool bFoundOpenEnd = ARTSBarbedWireLine::FindOpenEndNear(
		GetWorld(), PathEnd, Radius, PlayerOwnedTeam, M_PreviewLine, OpenEnd);
	// The end the line started on is open while the line is only laid out; the line never closes back onto it.
	const bool bIsStartEnd = bFoundOpenEnd && FVector2D::Distance(FVector2D(OpenEnd), LineStart) <= StartEndTolerance;
	if (bFoundOpenEnd && not bIsStartEnd)
	{
		M_EndSnap = OpenEnd;
		return;
	}
	M_EndSnap.Reset();
}

void UBarbedWirePlacementComponent::ShowPlan(TArray<FVector2D> PlanPath, const bool bEndsOnPathEnd)
{
	if (PlanPath.IsEmpty() || not IsValid(M_PreviewLine))
	{
		return;
	}
	if (not bEndsOnPathEnd)
	{
		ExtendToOnePiece(PlanPath);
	}
	if (PlanPath == M_LastPlanPath && bEndsOnPathEnd == bM_LastPlanEndSnapped)
	{
		return;
	}
	M_LastPlanPath = PlanPath;
	bM_LastPlanEndSnapped = bEndsOnPathEnd;
	const TArray<FBarbedWireSpan> Spans = ARTSBarbedWireLine::PlanAlongPath(
		PlanPath, GetPieceLength(), MaximumLineLength, bEndsOnPathEnd);
	M_PlannedSpans = KeepBuildablePieces(Spans);
	if (not M_PlannedSpans.IsEmpty())
	{
		M_PreviewLine->SetPlannedPieces(M_PlannedSpans, M_StartHeight);
		M_PreviewLine->SetPlanIsValid(true);
		return;
	}
	// Nothing of it can be built: the whole plan is shown in red, so the player sees why.
	M_PreviewLine->SetPlannedPieces(Spans, M_StartHeight);
	M_PreviewLine->SetPlanIsValid(false);
}

void UBarbedWirePlacementComponent::ExtendToOnePiece(TArray<FVector2D>& InOutPlanPath)
{
	using namespace BarbedWirePlacementPrivate;
	if (InOutPlanPath.IsEmpty())
	{
		return;
	}
	const float PieceLength = GetPieceLength();
	if (GetPathLength(InOutPlanPath) >= PieceLength)
	{
		return;
	}
	const FVector2D Start = InOutPlanPath[0];
	const FVector2D TowardsMouse = InOutPlanPath.Last() - Start;
	if (TowardsMouse.Size() >= MinimumShortLineAimCm)
	{
		M_ShortLineDirection = TowardsMouse.GetSafeNormal();
	}
	InOutPlanPath = {Start, Start + M_ShortLineDirection * PieceLength};
}

FVector2D UBarbedWirePlacementComponent::GetScreenRightDirection() const
{
	const ACPPController* PlayerController = GetPlayerController();
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		return M_ShortLineDirection;
	}
	const float CameraYaw = PlayerController->PlayerCameraManager->GetCameraRotation().Yaw;
	const float YawRadians = FMath::DegreesToRadians(
		CameraYaw + BarbedWirePlacementPrivate::ScreenRightYawOffsetDegrees);
	return FVector2D(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
}

TArray<FBarbedWireSpan> UBarbedWirePlacementComponent::KeepBuildablePieces(const TArray<FBarbedWireSpan>& Spans)
{
	// The preview lays the pieces onto the ground, which tells how far each leans and whether it has ground.
	M_PreviewLine->SetPlannedPieces(Spans, M_StartHeight);
	const int32 FirstUnbuildablePiece = M_PreviewLine->GetPieces().IndexOfByPredicate(
		[this](const FBarbedWirePieceRuntime& Piece)
		{
			return not GetIsPieceBuildable(Piece);
		});
	if (FirstUnbuildablePiece == INDEX_NONE)
	{
		return Spans;
	}
	TArray<FBarbedWireSpan> BuildableSpans;
	for (int32 SpanIndex = 0; SpanIndex < FirstUnbuildablePiece && Spans.IsValidIndex(SpanIndex); ++SpanIndex)
	{
		BuildableSpans.Add(Spans[SpanIndex]);
	}
	return BuildableSpans;
}

bool UBarbedWirePlacementComponent::GetIsPieceBuildable(const FBarbedWirePieceRuntime& Piece) const
{
	if (not Piece.bHasGround || Piece.SlopeDegrees > MaximumSlope)
	{
		return false;
	}
	if (GetHasRadiiConstraint() && not GetIsWithinBuildRadii(Piece.GroundTransform.GetLocation()))
	{
		return false;
	}
	// Last, as the only query: wire is not strung through buildings, pits or walls.
	return not IsValid(M_PreviewLine) || not M_PreviewLine->GetIsPieceOnObstacle(Piece);
}

void UBarbedWirePlacementComponent::ResetPlacement()
{
	M_Trainer.Reset();
	M_BuildRadii.Reset();
	M_CostPerMetre.Reset();
	M_DragPath.Reset();
	M_LastPlanPath.Reset();
	bM_LastPlanEndSnapped = false;
	M_PlannedSpans.Reset();
	M_EndSnap.Reset();
	bM_IsStartSnapped = false;
	M_Phase = EBarbedWirePlacementPhase::Idle;
	SetComponentTickEnabled(false);
}
