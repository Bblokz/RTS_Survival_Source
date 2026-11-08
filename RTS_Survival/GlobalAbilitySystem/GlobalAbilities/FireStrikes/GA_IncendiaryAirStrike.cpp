// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_IncendiaryAirStrike.h"

#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"

namespace IncendiaryAirStrikeConstants
{
	// Same seed for every strip: the preview and the drop agree on the spots without passing them around.
	constexpr int32 ScatterSeed = 19430;
	// Fraction of one canister's slot along the strip that it may shift.
	constexpr float AlongJitterFraction = 0.15f;
	// Smallest share of the side scatter, so alternating canisters never line up exactly.
	constexpr float MinSideShare = 0.4f;
}

void UGA_IncendiaryAirStrike::GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
                                                 TArray<FVector>& OutDropPoints) const
{
	using namespace IncendiaryAirStrikeConstants;
	const FVector Line(LineEnd.X - LineStart.X, LineEnd.Y - LineStart.Y, 0.0f);
	const float StripLength = Line.Size();
	const int32 CanisterCount = FMath::Max(M_IncendiarySettings.CanisterCount, 1);
	if (StripLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Direction = Line / StripLength;
	const FVector Sideways(-Direction.Y, Direction.X, 0.0f);
	const float SlotLength = StripLength / CanisterCount;
	const float MaxSide = M_AimSettings.AreaAim.AreaRadius * M_IncendiarySettings.SideScatterFraction;

	OutDropPoints.Reserve(CanisterCount);
	for (int32 CanisterIndex = 0; CanisterIndex < CanisterCount; ++CanisterIndex)
	{
		FRandomStream Scatter(ScatterSeed + CanisterIndex);
		const float Along = SlotLength * (CanisterIndex + 0.5f + Scatter.FRandRange(-AlongJitterFraction,
		                                                                            AlongJitterFraction));
		const float Side = (CanisterIndex % 2 == 0 ? 1.0f : -1.0f) * Scatter.FRandRange(MinSideShare, 1.0f) * MaxSide;
		FVector GroundLocation;
		// Off the landscape the location is kept as given; the canister still bursts there.
		GlobalAbilityHelpers::TryFindGroundLocation(this, LineStart + Direction * Along + Sideways * Side,
		                                            GroundLocation);
		OutDropPoints.Add(GroundLocation);
	}
}

void UGA_IncendiaryAirStrike::OnStripFlightStarted(const FStripFlightRun& Run)
{
	TArray<FVector> DropPoints;
	GetStripDropPoints(Run.LineStart, Run.LineEnd, DropPoints);
	TArray<FPendingCanister> PendingCanisters;
	PendingCanisters.Reserve(DropPoints.Num());
	for (const FVector& DropPoint : DropPoints)
	{
		FPendingCanister PendingCanister;
		PendingCanister.GroundLocation = DropPoint;
		PendingCanister.DistanceOnStrip = FVector::DotProduct(DropPoint - Run.LineStart, Run.Direction);
		PendingCanisters.Add(PendingCanister);
	}
	M_PendingCanistersPerRun.Add(Run.RunId, MoveTemp(PendingCanisters));
}

void UGA_IncendiaryAirStrike::OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
                                                    const float NewDistance)
{
	TArray<FPendingCanister>* PendingCanisters = M_PendingCanistersPerRun.Find(Run.RunId);
	if (not PendingCanisters)
	{
		return;
	}
	const float ReleaseDistance = NewDistance + M_IncendiarySettings.ReleaseLeadDistance;
	// Reverse: removing keeps the indices of the canisters not yet visited.
	for (int32 CanisterIndex = PendingCanisters->Num() - 1; CanisterIndex >= 0; --CanisterIndex)
	{
		const FPendingCanister PendingCanister = (*PendingCanisters)[CanisterIndex];
		if (PendingCanister.DistanceOnStrip <= ReleaseDistance)
		{
			PendingCanisters->RemoveAt(CanisterIndex);
			DropCanister(Run, PendingCanister);
		}
	}
}

void UGA_IncendiaryAirStrike::OnStripFlightEnded(const FStripFlightRun& Run)
{
	M_PendingCanistersPerRun.Remove(Run.RunId);
}

void UGA_IncendiaryAirStrike::DropCanister(const FStripFlightRun& Run, const FPendingCanister& Canister) const
{
	UWorld* World = GetWorld();
	const AAircraftMaster* Aircraft = Run.Aircraft.Get();
	if (not IsValid(World) || not IsValid(Aircraft))
	{
		return;
	}
	const FVector ReleaseLocation = Aircraft->GetActorLocation();
	AIncendiaryCanister* DroppedCanister = World->SpawnActor<AIncendiaryCanister>(
		AIncendiaryCanister::StaticClass(), ReleaseLocation, Run.Direction.Rotation());
	if (not IsValid(DroppedCanister))
	{
		return;
	}
	DroppedCanister->StartFall(ReleaseLocation, Canister.GroundLocation, GetOwningPlayer(),
	                           M_IncendiarySettings.CanisterSettings);
}
