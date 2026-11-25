// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DirectionFindingSolver.h"

namespace DirectionFindingSolverConstants
{
	// Wedge sides reach this many map widths from the station before clipping, so they always cross the map.
	constexpr double WedgeReachInMapWidths = 4.0;
	// A circle closer than this to its station counts as inside every wedge of that station.
	constexpr double MinDistanceForBearingCm = 1.0;
	constexpr double MaxHalfAngleDegrees = 89.0;
}

void FDirectionFindingSolver::Init(const TArray<FVector2D>& InStationLocations, const TArray<FVector2D>& InWerkCentres,
                                   const TArray<float>& InWerkRadii, const int32 InCommandPostIndex)
{
	M_StationLocations = InStationLocations;
	M_WerkCentres = InWerkCentres;
	M_WerkRadii = InWerkRadii;
	M_WerkRadii.SetNumZeroed(M_WerkCentres.Num());
	M_WerkEliminated.Init(false, M_WerkCentres.Num());
	M_CommandPostIndex = M_WerkCentres.IsValidIndex(InCommandPostIndex) ? InCommandPostIndex : INDEX_NONE;
	M_Wedges.Reset();
	M_Wedges.SetNum(M_StationLocations.Num());
	if (M_CommandPostIndex == INDEX_NONE)
	{
		return;
	}
	const FVector2D CommandPost = M_WerkCentres[M_CommandPostIndex];
	for (int32 StationIndex = 0; StationIndex < M_StationLocations.Num(); ++StationIndex)
	{
		M_Wedges[StationIndex].TrueBearingDegrees = GetBearingDegrees(M_StationLocations[StationIndex], CommandPost);
	}
}

bool FDirectionFindingSolver::SetStationHeld(const int32 StationIndex, const bool bHeld, const float StartHalfAngle,
                                             const float ErrorFraction)
{
	if (not M_Wedges.IsValidIndex(StationIndex) || M_CommandPostIndex == INDEX_NONE)
	{
		return false;
	}
	FDirectionFindingWedge& Wedge = M_Wedges[StationIndex];
	Wedge.bHeld = bHeld;
	if (not bHeld || Wedge.bOpen)
	{
		return false;
	}
	Wedge.bOpen = true;
	Wedge.HalfAngleDegrees = FMath::Clamp(StartHalfAngle, 0.f,
	                                      static_cast<float>(DirectionFindingSolverConstants::MaxHalfAngleDegrees));
	Wedge.PreviousHalfAngleDegrees = Wedge.HalfAngleDegrees;
	Wedge.ErrorFraction = ErrorFraction;
	return true;
}

int32 FDirectionFindingSolver::ApplyBurst(const float Factor, const float MinHalfAngle, const double NowSeconds)
{
	int32 HeardCount = 0;
	for (FDirectionFindingWedge& Wedge : M_Wedges)
	{
		if (not Wedge.bOpen || not Wedge.bHeld)
		{
			continue;
		}
		HeardCount++;
		Wedge.PreviousHalfAngleDegrees = Wedge.HalfAngleDegrees;
		Wedge.HalfAngleDegrees = FMath::Max(MinHalfAngle, Wedge.HalfAngleDegrees * Factor);
		Wedge.NarrowedAtSeconds = NowSeconds;
	}
	return HeardCount;
}

bool FDirectionFindingSolver::SetWerkEliminated(const int32 WerkIndex)
{
	if (not M_WerkEliminated.IsValidIndex(WerkIndex) || WerkIndex == M_CommandPostIndex)
	{
		return false;
	}
	M_WerkEliminated[WerkIndex] = true;
	return true;
}

TArray<int32> FDirectionFindingSolver::GetCandidateWerke() const
{
	TArray<int32> Candidates;
	if (GetHeldWedgeCount() == 0)
	{
		return Candidates;
	}
	for (int32 WerkIndex = 0; WerkIndex < M_WerkCentres.Num(); ++WerkIndex)
	{
		if (not M_WerkEliminated[WerkIndex] && GetIsWerkInsideEveryHeldWedge(WerkIndex))
		{
			Candidates.Add(WerkIndex);
		}
	}
	return Candidates;
}

int32 FDirectionFindingSolver::GetHeldWedgeCount() const
{
	int32 HeldCount = 0;
	for (const FDirectionFindingWedge& Wedge : M_Wedges)
	{
		HeldCount += Wedge.bOpen && Wedge.bHeld ? 1 : 0;
	}
	return HeldCount;
}

const FDirectionFindingWedge* FDirectionFindingSolver::GetWedge(const int32 StationIndex) const
{
	return M_Wedges.IsValidIndex(StationIndex) ? &M_Wedges[StationIndex] : nullptr;
}

bool FDirectionFindingSolver::GetIsFix(const int32 StationsNeeded) const
{
	if (M_CommandPostIndex == INDEX_NONE || GetHeldWedgeCount() < StationsNeeded)
	{
		return false;
	}
	const TArray<int32> Candidates = GetCandidateWerke();
	return Candidates.Num() == 1 && Candidates[0] == M_CommandPostIndex;
}

bool FDirectionFindingSolver::GetAreAllStationsHeld() const
{
	return not M_Wedges.IsEmpty() && GetHeldWedgeCount() == M_Wedges.Num();
}

float FDirectionFindingSolver::GetShownHalfAngle(const FDirectionFindingWedge& Wedge, const double NowSeconds,
                                                 const float AnimationSeconds)
{
	if (Wedge.NarrowedAtSeconds < 0.0 || AnimationSeconds <= 0.f)
	{
		return Wedge.HalfAngleDegrees;
	}
	const float Alpha = FMath::Clamp(static_cast<float>(NowSeconds - Wedge.NarrowedAtSeconds) / AnimationSeconds,
	                                 0.f, 1.f);
	return FMath::Lerp(Wedge.PreviousHalfAngleDegrees, Wedge.HalfAngleDegrees, FMath::SmoothStep(0.f, 1.f, Alpha));
}

void FDirectionFindingSolver::BuildWedgePolygon(const FVector2D& Apex, const float CentreBearingDegrees,
                                                const float HalfAngleDegrees, const FBox2D& MapBounds,
                                                TArray<FVector2D>& OutPolygon)
{
	using namespace DirectionFindingSolverConstants;
	OutPolygon.Reset();
	if (not MapBounds.bIsValid)
	{
		return;
	}
	const double Reach = MapBounds.GetSize().GetMax() * WedgeReachInMapWidths;
	const double HalfAngle = FMath::Clamp(static_cast<double>(HalfAngleDegrees), 0.0, MaxHalfAngleDegrees);
	const double RightRadians = FMath::DegreesToRadians(CentreBearingDegrees - HalfAngle);
	const double LeftRadians = FMath::DegreesToRadians(CentreBearingDegrees + HalfAngle);
	// Counter-clockwise: apex, the clockwise side, the counter-clockwise side.
	OutPolygon.Add(Apex);
	OutPolygon.Add(Apex + FVector2D(FMath::Cos(RightRadians), FMath::Sin(RightRadians)) * Reach);
	OutPolygon.Add(Apex + FVector2D(FMath::Cos(LeftRadians), FMath::Sin(LeftRadians)) * Reach);
	const TArray<FVector2D> MapSquare = {
		MapBounds.Min, FVector2D(MapBounds.Max.X, MapBounds.Min.Y), MapBounds.Max,
		FVector2D(MapBounds.Min.X, MapBounds.Max.Y)
	};
	ClipConvexPolygon(OutPolygon, MapSquare);
}

void FDirectionFindingSolver::ClipConvexPolygon(TArray<FVector2D>& Subject, const TArray<FVector2D>& Clip)
{
	if (Clip.Num() < 3)
	{
		Subject.Reset();
		return;
	}
	TArray<FVector2D> Input;
	for (int32 EdgeIndex = 0; EdgeIndex < Clip.Num() && Subject.Num() > 0; ++EdgeIndex)
	{
		const FVector2D EdgeStart = Clip[EdgeIndex];
		const FVector2D EdgeVector = Clip[(EdgeIndex + 1) % Clip.Num()] - EdgeStart;
		// Left of a counter-clockwise edge is inside.
		auto SideOf = [&EdgeStart, &EdgeVector](const FVector2D& Point)
		{
			return FVector2D::CrossProduct(EdgeVector, Point - EdgeStart);
		};
		Input = MoveTemp(Subject);
		Subject.Reset();
		for (int32 PointIndex = 0; PointIndex < Input.Num(); ++PointIndex)
		{
			const FVector2D Current = Input[PointIndex];
			const FVector2D Previous = Input[(PointIndex + Input.Num() - 1) % Input.Num()];
			const double CurrentSide = SideOf(Current);
			const double PreviousSide = SideOf(Previous);
			if ((CurrentSide >= 0.0) != (PreviousSide >= 0.0))
			{
				const double Alpha = PreviousSide / (PreviousSide - CurrentSide);
				Subject.Add(Previous + (Current - Previous) * Alpha);
			}
			if (CurrentSide >= 0.0)
			{
				Subject.Add(Current);
			}
		}
	}
	if (Subject.Num() < 3)
	{
		Subject.Reset();
	}
}

float FDirectionFindingSolver::GetBearingDegrees(const FVector2D& From, const FVector2D& To)
{
	const FVector2D Delta = To - From;
	return FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
}

bool FDirectionFindingSolver::GetIsCircleInWedge(const FVector2D& Apex, const float CentreBearingDegrees,
                                                 const float HalfAngleDegrees, const FVector2D& Point,
                                                 const float Radius)
{
	using namespace DirectionFindingSolverConstants;
	const double Distance = FVector2D::Distance(Apex, Point);
	if (Distance <= FMath::Max(static_cast<double>(Radius), MinDistanceForBearingCm))
	{
		return true;
	}
	const double AngularRadius = FMath::RadiansToDegrees(FMath::Asin(Radius / Distance));
	const double Offset = FMath::Abs(FMath::FindDeltaAngleDegrees(CentreBearingDegrees,
	                                                              GetBearingDegrees(Apex, Point)));
	return Offset <= HalfAngleDegrees + AngularRadius;
}

FVector2D FDirectionFindingSolver::GetStationLocation(const int32 StationIndex) const
{
	return M_StationLocations.IsValidIndex(StationIndex) ? M_StationLocations[StationIndex] : FVector2D::ZeroVector;
}

bool FDirectionFindingSolver::GetIsWerkInsideEveryHeldWedge(const int32 WerkIndex) const
{
	for (int32 StationIndex = 0; StationIndex < M_Wedges.Num(); ++StationIndex)
	{
		const FDirectionFindingWedge& Wedge = M_Wedges[StationIndex];
		if (not Wedge.bOpen || not Wedge.bHeld)
		{
			continue;
		}
		if (not GetIsCircleInWedge(M_StationLocations[StationIndex],
		                           Wedge.GetCentreBearingDegrees(Wedge.HalfAngleDegrees), Wedge.HalfAngleDegrees,
		                           M_WerkCentres[WerkIndex], M_WerkRadii[WerkIndex]))
		{
			return false;
		}
	}
	return true;
}
