// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_AirMinefield.h"

#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionActor/FieldMine.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

namespace AirMinefieldConstants
{
	// Same seed for every strip: the preview and the drop agree on the scatter without passing it around.
	constexpr int32 ScatterSeed = 19420;
	// Fraction of the slot of one mine along the strip that it may shift.
	constexpr float AlongJitterFraction = 0.3f;
	// Fraction of the half width mines stay within, so none lands on the drawn edge.
	constexpr float WidthFraction = 0.85f;
	const FVector NavProjectionExtent(150.0f, 150.0f, 400.0f);
}

void UGA_AirMinefield::GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
                                          TArray<FVector>& OutDropPoints) const
{
	using namespace AirMinefieldConstants;
	const FVector Line(LineEnd.X - LineStart.X, LineEnd.Y - LineStart.Y, 0.0f);
	const float StripLength = Line.Size();
	if (StripLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Direction = Line / StripLength;
	const FVector Sideways(-Direction.Y, Direction.X, 0.0f);
	const float SlotLength = StripLength / M_MinefieldSettings.MineCount;
	const float HalfWidth = M_AimSettings.AreaAim.AreaRadius * WidthFraction;

	OutDropPoints.Reserve(M_MinefieldSettings.MineCount);
	for (int32 MineIndex = 0; MineIndex < M_MinefieldSettings.MineCount; ++MineIndex)
	{
		FRandomStream Scatter(ScatterSeed + MineIndex);
		const float Along = SlotLength * (MineIndex + 0.5f + Scatter.FRandRange(-AlongJitterFraction,
		                                                                        AlongJitterFraction));
		// Alternating sides keep the field wide even when the random part is small.
		const float Side = (MineIndex % 2 == 0 ? 1.0f : -1.0f) * Scatter.FRandRange(0.2f, 1.0f) * HalfWidth;
		const FVector OnStrip = LineStart + Direction * Along + Sideways * Side;

		FVector GroundLocation;
		if (not GlobalAbilityHelpers::TryFindGroundLocation(this, OnStrip, GroundLocation))
		{
			continue;
		}
		bool bIsOnNavigation = false;
		URTSBlueprintFunctionLibrary::RTSProjectLocationToNavigation(this, GroundLocation, NavProjectionExtent,
		                                                             bIsOnNavigation);
		if (bIsOnNavigation)
		{
			OutDropPoints.Add(GroundLocation);
		}
	}
}

void UGA_AirMinefield::OnStripFlightStarted(const FStripFlightRun& Run)
{
	TArray<FVector> DropPoints;
	GetStripDropPoints(Run.LineStart, Run.LineEnd, DropPoints);
	TArray<FPendingMine> PendingMines;
	PendingMines.Reserve(DropPoints.Num());
	for (int32 PointIndex = 0; PointIndex < DropPoints.Num(); ++PointIndex)
	{
		FPendingMine PendingMine;
		PendingMine.GroundLocation = DropPoints[PointIndex];
		PendingMine.DistanceOnStrip = FVector::DotProduct(DropPoints[PointIndex] - Run.LineStart, Run.Direction);
		PendingMine.MineIndex = PointIndex;
		PendingMines.Add(PendingMine);
	}
	M_PendingMinesPerRun.Add(Run.RunId, MoveTemp(PendingMines));
}

void UGA_AirMinefield::OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
                                             const float NewDistance)
{
	TArray<FPendingMine>* PendingMines = M_PendingMinesPerRun.Find(Run.RunId);
	if (not PendingMines)
	{
		return;
	}
	for (int32 MineIndex = PendingMines->Num() - 1; MineIndex >= 0; --MineIndex)
	{
		const FPendingMine PendingMine = (*PendingMines)[MineIndex];
		if (PendingMine.DistanceOnStrip <= NewDistance)
		{
			PendingMines->RemoveAt(MineIndex);
			DropMine(Run, PendingMine);
		}
	}
}

void UGA_AirMinefield::OnStripFlightEnded(const FStripFlightRun& Run)
{
	M_PendingMinesPerRun.Remove(Run.RunId);
}

void UGA_AirMinefield::GetAssetsToLoadBeforeFlight(TArray<FSoftObjectPath>& OutAssets) const
{
	for (const TSoftClassPtr<AFieldMine>& MineClass : M_MinefieldSettings.MineClasses)
	{
		OutAssets.Add(MineClass.ToSoftObjectPath());
	}
}

FVector UGA_AirMinefield::GetDirectionForPointExecution(const FVector& TargetLocation) const
{
	const FVector ApproachDirection = Super::GetDirectionForPointExecution(TargetLocation);
	return FVector(-ApproachDirection.Y, ApproachDirection.X, 0.0f);
}

void UGA_AirMinefield::DropMine(const FStripFlightRun& Run, const FPendingMine& Mine)
{
	UWorld* World = GetWorld();
	const AAircraftMaster* Aircraft = Run.Aircraft.Get();
	if (not IsValid(World) || not IsValid(Aircraft) || M_MinefieldSettings.MineClasses.IsEmpty())
	{
		return;
	}
	const int32 ClassIndex = Mine.MineIndex % M_MinefieldSettings.MineClasses.Num();
	UClass* MineClass = M_MinefieldSettings.MineClasses[ClassIndex].Get();
	if (not IsValid(MineClass))
	{
		return;
	}
	AAirDroppedMine* DroppedMine = World->SpawnActor<AAirDroppedMine>(
		AAirDroppedMine::StaticClass(), Aircraft->GetActorLocation(), Run.Direction.Rotation());
	if (not IsValid(DroppedMine))
	{
		return;
	}
	DroppedMine->StartFall(Aircraft->GetActorLocation(), Mine.GroundLocation, MineClass,
	                       static_cast<uint8>(GetOwningPlayer()), M_MinefieldSettings.DropSettings);
}
