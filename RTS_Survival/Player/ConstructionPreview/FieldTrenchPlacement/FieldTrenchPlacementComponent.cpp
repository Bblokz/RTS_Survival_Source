// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldTrenchPlacementComponent.h"

#include "Algo/Reverse.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "RTS_Survival/Buildings/FieldTrench/RTSFieldTrench.h"
#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorSubsystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerError/PlayerError.h"
#include "RTS_Survival/RTSComponents/RadiusComp/RadiusComp.h"

namespace FieldTrenchPlacementPrivate
{
	// The local player; trenches are only laid out by the player.
	constexpr int32 PlayerOwnedTeam = 1;
	// Shortening a plan to the buildable part of it rarely takes more than one try.
	constexpr int32 MaximumShortenAttempts = 6;
	// Lengths are summed from floats; the shortened plan must still have room for the pieces it keeps.
	constexpr float LengthTolerance = 1.0f;
	// A short trench turns to the mouse once the mouse is this far from the start; closer, a shaking hand would
	// spin it round.
	constexpr float MinimumShortTrenchAimCm = 25.0f;
	// From the camera's forward to screen right.
	constexpr float ScreenRightYawOffsetDegrees = 90.0f;

	FVector2D GetYawDirection(const float YawDegrees)
	{
		const float YawRadians = FMath::DegreesToRadians(YawDegrees);
		return FVector2D(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
	}
}

UFieldTrenchPlacementComponent::UFieldTrenchPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// The C++ class has the trench collision and the construction preview ghost; a Blueprint may be set instead.
	TrenchClass = ARTSFieldTrench::StaticClass();
}

void UFieldTrenchPlacementComponent::StartPlacement(
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
	M_Phase = EFieldTrenchPlacementPhase::Hovering;
	SetComponentTickEnabled(true);
	// Shown at once, not only from the next tick.
	UpdateHover();
}

bool UFieldTrenchPlacementComponent::OnPrimaryClick(const FVector& ClickedLocation)
{
	using namespace FieldTrenchPlacementPrivate;
	ACPPController* PlayerController = GetPlayerController();
	if (not GetIsPlacing() || not IsValid(PlayerController))
	{
		return true;
	}
	if (M_Phase == EFieldTrenchPlacementPhase::Hovering)
	{
		return BeginDrawing(ClickedLocation, *PlayerController);
	}
	if (not IsValid(M_PreviewTrench))
	{
		return true;
	}
	UpdateDrawing();
	if (M_PlannedPieces.IsEmpty())
	{
		// Nothing of it lies where the player may build.
		PlayerController->DisplayErrorMessage(EPlayerError::Error_CannotBuildHere);
		return false;
	}
	// The trench claims the anchor its plan starts at.
	M_PreviewTrench->StartConstruction({}, PlayerOwnedTeam, M_CostPerMetre);
	M_LastConfirmedTrench = M_PreviewTrench;
	// Handed over: the trench digs itself from here on and must outlive the placement.
	M_PreviewTrench = nullptr;
	ResetPlacement();
	return true;
}

bool UFieldTrenchPlacementComponent::GetIsDrawing() const
{
	return M_Phase == EFieldTrenchPlacementPhase::Drawing && IsValid(M_PreviewTrench);
}

void UFieldTrenchPlacementComponent::CancelPlacement()
{
	if (IsValid(M_PreviewTrench))
	{
		M_PreviewTrench->Destroy();
	}
	M_PreviewTrench = nullptr;
	ResetPlacement();
}

void UFieldTrenchPlacementComponent::TickComponent(
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
		// The HQ died or packed up while the player was laying the trench out.
		if (ACPPController* PlayerController = GetPlayerController())
		{
			PlayerController->StopFieldTrenchPlacement();
		}
		else
		{
			CancelPlacement();
		}
		return;
	}
	if (M_Phase == EFieldTrenchPlacementPhase::Hovering)
	{
		UpdateHover();
		return;
	}
	if (GetIsDrawing())
	{
		UpdateDrawing();
	}
}

ACPPController* UFieldTrenchPlacementComponent::GetPlayerController() const
{
	return Cast<ACPPController>(GetOwner());
}

bool UFieldTrenchPlacementComponent::TryGetCursorGround(FVector& OutGround) const
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

bool UFieldTrenchPlacementComponent::GetIsWithinBuildRadii(const FVector& Location) const
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

bool UFieldTrenchPlacementComponent::EnsurePreviewTrench(const FVector& SpawnLocation)
{
	if (IsValid(M_PreviewTrench))
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	UClass* SpawnClass = TrenchClass.LoadSynchronous();
	if (not IsValid(SpawnClass))
	{
		SpawnClass = ARTSFieldTrench::StaticClass();
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_PreviewTrench = World->SpawnActor<ARTSFieldTrench>(SpawnClass, FTransform(SpawnLocation), SpawnParameters);
	return IsValid(M_PreviewTrench);
}

bool UFieldTrenchPlacementComponent::BeginDrawing(const FVector& ClickedLocation, ACPPController& PlayerController)
{
	const FFieldTrenchAnchorHandle StartAnchor = FindAnchorNear(FVector2D(ClickedLocation), FFieldTrenchAnchorHandle());
	const FVector StartLocation = GetAnchorOrFallbackLocation(StartAnchor, ClickedLocation);
	if (GetHasRadiiConstraint() && not GetIsWithinBuildRadii(StartLocation))
	{
		PlayerController.DisplayErrorMessage(EPlayerError::Error_CannotBuildHere);
		return false;
	}
	if (not EnsurePreviewTrench(StartLocation))
	{
		return true;
	}
	// The pieces are laid in world space; the plan is laid again below, after the actor moved to the start.
	M_PreviewTrench->SetActorLocation(StartLocation);
	M_DragPath = {FVector2D(StartLocation)};
	M_LastPlanPath.Reset();
	M_StartAnchor = StartAnchor;
	M_EndAnchor.Reset();
	M_LastPlanAnchor.Reset();
	M_StartHeight = StartLocation.Z;
	M_PlannedPieces.Reset();
	M_Phase = EFieldTrenchPlacementPhase::Drawing;
	// The short trench the hover showed stays up instead of vanishing for a frame.
	UpdateDrawing();
	return false;
}

void UFieldTrenchPlacementComponent::UpdateHover()
{
	FVector CursorGround;
	if (not TryGetCursorGround(CursorGround))
	{
		return;
	}
	// Exactly what a click here would start: on the anchor the click snaps to, or at the mouse.
	M_StartAnchor = FindAnchorNear(FVector2D(CursorGround), FFieldTrenchAnchorHandle());
	const FVector StartLocation = GetAnchorOrFallbackLocation(M_StartAnchor, CursorGround);
	if (not EnsurePreviewTrench(StartLocation))
	{
		return;
	}
	M_StartHeight = StartLocation.Z;
	M_ShortTrenchDirection = GetScreenRightDirection();
	ShowPlan({FVector2D(StartLocation)}, M_StartAnchor, false);
}

void UFieldTrenchPlacementComponent::UpdateDrawing()
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
			// Between recorded steps the trench still reaches to the mouse. A doubled point would stall the planner.
			PlanPath.Add(Cursor);
		}
	}
	if (M_StartAnchor.IsSet() && not GetIsAnchorFree(M_StartAnchor))
	{
		// Its owner went while the trench was laid out; the trench starts with a cap where it was.
		M_StartAnchor.Reset();
	}
	M_EndAnchor = M_StartAnchor.IsSet() ? FFieldTrenchAnchorHandle() : FindAnchorNear(PlanPath.Last(), M_EndAnchor);
	// A trench is planned from its anchor out, so it fits the anchor exactly; joined at the end, it is planned
	// from there back to the first click.
	const bool bIsJoinedAtEnd = M_EndAnchor.IsSet();
	ShowPlan(MoveTemp(PlanPath), bIsJoinedAtEnd ? M_EndAnchor : M_StartAnchor, bIsJoinedAtEnd);
}

void UFieldTrenchPlacementComponent::ShowPlan(
	TArray<FVector2D> PlanPath,
	const FFieldTrenchAnchorHandle& PlanAnchor,
	const bool bIsJoinedAtEnd)
{
	if (PlanPath.IsEmpty() || not IsValid(M_PreviewTrench))
	{
		return;
	}
	FFieldTrenchPlanOptions Options;
	StartPathOnAnchor(PlanPath, Options, PlanAnchor, bIsJoinedAtEnd);
	ExtendToMinimumShownLength(PlanPath, Options);
	if (PlanPath == M_LastPlanPath && PlanAnchor == M_LastPlanAnchor)
	{
		return;
	}
	M_LastPlanPath = PlanPath;
	M_LastPlanAnchor = PlanAnchor;
	M_PreviewTrench->SetStartAnchor(PlanAnchor);
	M_PlannedPieces = PlanBuildableTrench(PlanPath, Options);
	if (not M_PlannedPieces.IsEmpty())
	{
		M_PreviewTrench->SetPlannedPieces(M_PlannedPieces);
		M_PreviewTrench->SetPlanIsValid(true);
		return;
	}
	// Nothing of it can be built: the whole plan is shown in red, so the player sees why.
	TArray<FFieldTrenchPiecePlacement> UnbuildablePlan = FieldTrenchLayout::PlanAlongPath(
		PlanPath, MaximumTrenchLength, Options);
	for (FFieldTrenchPiecePlacement& Piece : UnbuildablePlan)
	{
		Piece.Location.Z = M_StartHeight;
	}
	M_PreviewTrench->SetPlannedPieces(UnbuildablePlan);
	M_PreviewTrench->SetPlanIsValid(false);
}

void UFieldTrenchPlacementComponent::StartPathOnAnchor(
	TArray<FVector2D>& InOutPlanPath,
	FFieldTrenchPlanOptions& InOutOptions,
	const FFieldTrenchAnchorHandle& PlanAnchor,
	const bool bIsJoinedAtEnd) const
{
	const UFieldTrenchAnchorComponent* AnchorComponent = PlanAnchor.Get();
	if (not IsValid(AnchorComponent))
	{
		return;
	}
	InOutOptions.StartPort = AnchorComponent->GetAnchorPort(PlanAnchor.AnchorIndex);
	if (bIsJoinedAtEnd)
	{
		Algo::Reverse(InOutPlanPath);
	}
	const FVector2D AnchorLocation = InOutOptions.StartPort->Location;
	InOutPlanPath[0] = AnchorLocation;
	// The mouse over the anchor's owner, behind the anchor, would turn the trench back into it.
	const FVector2D AnchorOut = FieldTrenchPlacementPrivate::GetYawDirection(InOutOptions.StartPort->Yaw);
	for (int32 PathIndex = InOutPlanPath.Num() - 1; PathIndex >= 1; --PathIndex)
	{
		const FVector2D FromAnchor = InOutPlanPath[PathIndex] - AnchorLocation;
		if (FromAnchor.Size() <= AnchorReleaseRadius && FVector2D::DotProduct(FromAnchor, AnchorOut) < 0.0f)
		{
			InOutPlanPath.RemoveAt(PathIndex);
		}
	}
}

void UFieldTrenchPlacementComponent::ExtendToMinimumShownLength(
	TArray<FVector2D>& InOutPlanPath,
	const FFieldTrenchPlanOptions& Options)
{
	using namespace FieldTrenchPlacementPrivate;
	if (InOutPlanPath.IsEmpty())
	{
		return;
	}
	const FVector2D Start = InOutPlanPath[0];
	float FarthestDistance = 0.0f;
	for (const FVector2D& Point : InOutPlanPath)
	{
		FarthestDistance = FMath::Max(FarthestDistance, FVector2D::Distance(Start, Point));
	}
	if (FarthestDistance >= MinimumShownLength)
	{
		return;
	}
	const FVector2D TowardsMouse = InOutPlanPath.Last() - Start;
	FVector2D Direction = M_ShortTrenchDirection;
	if (TowardsMouse.Size() >= MinimumShortTrenchAimCm)
	{
		Direction = TowardsMouse.GetSafeNormal();
		M_ShortTrenchDirection = Direction;
	}
	else if (Options.StartPort.IsSet())
	{
		// On an anchor the trench leaves the way the anchor points.
		Direction = GetYawDirection(Options.StartPort->Yaw);
	}
	InOutPlanPath = {Start, Start + Direction * MinimumShownLength};
}

FVector2D UFieldTrenchPlacementComponent::GetScreenRightDirection() const
{
	const ACPPController* PlayerController = GetPlayerController();
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		return M_ShortTrenchDirection;
	}
	const float CameraYaw = PlayerController->PlayerCameraManager->GetCameraRotation().Yaw;
	return FieldTrenchPlacementPrivate::GetYawDirection(
		CameraYaw + FieldTrenchPlacementPrivate::ScreenRightYawOffsetDegrees);
}

FVector UFieldTrenchPlacementComponent::GetAnchorOrFallbackLocation(
	const FFieldTrenchAnchorHandle& Anchor,
	const FVector& Fallback)
{
	const UFieldTrenchAnchorComponent* AnchorComponent = Anchor.Get();
	return IsValid(AnchorComponent) ? AnchorComponent->GetAnchorLocation(Anchor.AnchorIndex) : Fallback;
}

FFieldTrenchAnchorHandle UFieldTrenchPlacementComponent::FindAnchorNear(
	const FVector2D& Location,
	const FFieldTrenchAnchorHandle& CurrentAnchor) const
{
	using namespace FieldTrenchPlacementPrivate;
	if (const UFieldTrenchAnchorComponent* CurrentComponent = CurrentAnchor.Get(); GetIsAnchorFree(CurrentAnchor) &&
		FVector2D::Distance(Location, CurrentComponent->GetAnchorPort(CurrentAnchor.AnchorIndex).Location) <=
		AnchorReleaseRadius)
	{
		return CurrentAnchor;
	}
	const UWorld* World = GetWorld();
	const UFieldTrenchAnchorSubsystem* AnchorSubsystem = IsValid(World)
		? World->GetSubsystem<UFieldTrenchAnchorSubsystem>()
		: nullptr;
	if (not IsValid(AnchorSubsystem))
	{
		return FFieldTrenchAnchorHandle();
	}
	return AnchorSubsystem->FindFreeAnchorNear(Location, AnchorSnapRadius, PlayerOwnedTeam);
}

bool UFieldTrenchPlacementComponent::GetIsAnchorFree(const FFieldTrenchAnchorHandle& Anchor)
{
	const UFieldTrenchAnchorComponent* AnchorComponent = Anchor.Get();
	return IsValid(AnchorComponent) && AnchorComponent->GetIsAnchorFree(Anchor.AnchorIndex);
}

TArray<FFieldTrenchPiecePlacement> UFieldTrenchPlacementComponent::PlanBuildableTrench(
	const TArray<FVector2D>& DragPath,
	const FFieldTrenchPlanOptions& Options)
{
	using namespace FieldTrenchPlacementPrivate;
	float MaximumLength = MaximumTrenchLength;
	// Without a start cap the first piece is already trench worth keeping.
	const int32 PiecesBeforeTrench = Options.GetHasStartCap() ? 1 : 0;
	TArray<FFieldTrenchPiecePlacement> Pieces;
	for (int32 Attempt = 0; Attempt < MaximumShortenAttempts; ++Attempt)
	{
		Pieces = M_PreviewTrench->PlanOnGround(DragPath, MaximumLength, M_StartHeight, Options);
		if (Pieces.IsEmpty() || GetArePiecesBuildable(Pieces))
		{
			return Pieces;
		}
		// The trench is laid in the same order again, but ends before the first piece that does not fit.
		const int32 FirstBadPiece = M_PreviewTrench->GetPieces().IndexOfByPredicate(
			[this](const FFieldTrenchPieceRuntime& Piece)
			{
				return Piece.SlopeDegrees > MaximumSlope ||
					(GetHasRadiiConstraint() && not GetIsWithinBuildRadii(Piece.GroundTransform.GetLocation()));
			});
		if (FirstBadPiece <= PiecesBeforeTrench)
		{
			Pieces.Reset();
			return Pieces;
		}
		float KeptLength = 0.0f;
		for (int32 PieceIndex = 0; PieceIndex < FirstBadPiece; ++PieceIndex)
		{
			KeptLength += Pieces[PieceIndex].LengthCm;
		}
		MaximumLength = KeptLength + FieldTrenchLayout::GetPieceDefinition(EFieldTrenchPiece::End).LengthCm +
			LengthTolerance;
	}
	Pieces.Reset();
	return Pieces;
}

bool UFieldTrenchPlacementComponent::GetArePiecesBuildable(const TArray<FFieldTrenchPiecePlacement>& Pieces)
{
	// The preview lays the pieces onto the ground, which tells how far each has to lean.
	M_PreviewTrench->SetPlannedPieces(Pieces);
	for (const FFieldTrenchPieceRuntime& Piece : M_PreviewTrench->GetPieces())
	{
		const bool bIsOutsideRadii = GetHasRadiiConstraint() &&
			not GetIsWithinBuildRadii(Piece.GroundTransform.GetLocation());
		if (Piece.SlopeDegrees > MaximumSlope || bIsOutsideRadii)
		{
			return false;
		}
	}
	return true;
}

void UFieldTrenchPlacementComponent::ResetPlacement()
{
	M_Trainer.Reset();
	M_BuildRadii.Reset();
	M_CostPerMetre.Reset();
	M_DragPath.Reset();
	M_LastPlanPath.Reset();
	M_PlannedPieces.Reset();
	M_StartAnchor.Reset();
	M_EndAnchor.Reset();
	M_LastPlanAnchor.Reset();
	M_Phase = EFieldTrenchPlacementPhase::Idle;
	SetComponentTickEnabled(false);
}
