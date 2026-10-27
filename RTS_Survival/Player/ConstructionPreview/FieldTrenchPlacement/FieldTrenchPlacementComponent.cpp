// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldTrenchPlacementComponent.h"

#include "Engine/World.h"
#include "RTS_Survival/Buildings/FieldTrench/RTSFieldTrench.h"
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
	bM_IsPlacing = true;
	SetComponentTickEnabled(true);
}

bool UFieldTrenchPlacementComponent::OnPrimaryClick(const FVector& ClickedLocation)
{
	using namespace FieldTrenchPlacementPrivate;
	ACPPController* PlayerController = GetPlayerController();
	if (not bM_IsPlacing || not IsValid(PlayerController))
	{
		return true;
	}
	if (not GetIsDrawing())
	{
		if (GetHasRadiiConstraint() && not GetIsWithinBuildRadii(ClickedLocation))
		{
			PlayerController->DisplayErrorMessage(EPlayerError::Error_CannotBuildHere);
			return false;
		}
		UClass* SpawnClass = TrenchClass.LoadSynchronous();
		if (not IsValid(SpawnClass))
		{
			SpawnClass = ARTSFieldTrench::StaticClass();
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		M_PreviewTrench = GetWorld()->SpawnActor<ARTSFieldTrench>(SpawnClass, FTransform(ClickedLocation), SpawnParameters);
		if (not IsValid(M_PreviewTrench))
		{
			return true;
		}
		M_DragPath = {FVector2D(ClickedLocation)};
		M_LastPlanPath.Reset();
		M_StartHeight = ClickedLocation.Z;
		M_PlannedPieces.Reset();
		return false;
	}
	UpdateDrawing();
	if (M_PlannedPieces.IsEmpty())
	{
		// Too short, or nothing of it lies where the player may build.
		PlayerController->DisplayErrorMessage(EPlayerError::Error_CannotBuildHere);
		return false;
	}
	M_PreviewTrench->StartConstruction({}, PlayerOwnedTeam, M_CostPerMetre);
	M_LastConfirmedTrench = M_PreviewTrench;
	// Handed over: the trench digs itself from here on and must outlive the placement.
	M_PreviewTrench = nullptr;
	ResetPlacement();
	return true;
}

bool UFieldTrenchPlacementComponent::GetIsDrawing() const
{
	return bM_IsPlacing && IsValid(M_PreviewTrench);
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
	if (not bM_IsPlacing)
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
		else
		{
			// Between recorded steps the trench still reaches to the mouse.
			PlanPath.Add(Cursor);
		}
	}
	if (PlanPath == M_LastPlanPath)
	{
		return;
	}
	M_LastPlanPath = PlanPath;
	M_PlannedPieces = PlanBuildableTrench(PlanPath);
	if (not M_PlannedPieces.IsEmpty())
	{
		M_PreviewTrench->SetPlannedPieces(M_PlannedPieces);
		M_PreviewTrench->SetPlanIsValid(true);
		return;
	}
	// Nothing of it can be built: the whole plan is shown in red, so the player sees why.
	TArray<FFieldTrenchPiecePlacement> UnbuildablePlan = FieldTrenchLayout::PlanAlongPath(PlanPath, MaximumTrenchLength);
	for (FFieldTrenchPiecePlacement& Piece : UnbuildablePlan)
	{
		Piece.Location.Z = M_StartHeight;
	}
	M_PreviewTrench->SetPlannedPieces(UnbuildablePlan);
	M_PreviewTrench->SetPlanIsValid(false);
}

TArray<FFieldTrenchPiecePlacement> UFieldTrenchPlacementComponent::PlanBuildableTrench(const TArray<FVector2D>& DragPath)
{
	using namespace FieldTrenchPlacementPrivate;
	float MaximumLength = MaximumTrenchLength;
	TArray<FFieldTrenchPiecePlacement> Pieces;
	for (int32 Attempt = 0; Attempt < MaximumShortenAttempts; ++Attempt)
	{
		Pieces = M_PreviewTrench->PlanOnGround(DragPath, MaximumLength, M_StartHeight);
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
		if (FirstBadPiece <= 1)
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
	bM_IsPlacing = false;
	SetComponentTickEnabled(false);
}
