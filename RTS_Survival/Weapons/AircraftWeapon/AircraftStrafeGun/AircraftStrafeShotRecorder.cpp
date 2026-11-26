// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftStrafeShotRecorder.h"

#if RTS_WITH_MAP_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/DeveloperSettings.h"

namespace AircraftStrafeShotRecorderPrivate
{
	// A round counts as off the nose beyond the strafe gun's widest cone (yaw 10, depression 35) plus a margin.
	constexpr float OffNoseYawDeg = 11.f;
	constexpr float OffNoseTotalDeg = 37.f;

	// A round lands on target this close to the attacked unit or to the strafe line (2D, cm).
	constexpr float OnTargetRadiusCm = 400.f;
	constexpr float NearTargetRadiusCm = 800.f;
	// A dive counts as on target with at least this share of its ground impacts within OnTargetRadiusCm.
	constexpr float MinOnTargetShareForDive = 0.2f;

	struct FDiveRecord
	{
		TWeakObjectPtr<const AActor> Aircraft;
		FString AircraftName;
		int32 DiveIndexOfAircraft = 0;
		FString Description;
		TWeakObjectPtr<const AActor> TargetActor;
		FVector RunStart = FVector::ZeroVector;
		FVector RunEnd = FVector::ZeroVector;
		int32 Impacts = 0;
		int32 GroundHits = 0;
		int32 OnTarget = 0;
		int32 NearTarget = 0;
		double SumDistanceCm = 0.0;
		float MinDistanceCm = TNumericLimits<float>::Max();
		// Mean miss along / across the flight at the closest impacts, to tell overshoot from a lateral miss.
		double SumAlongCm = 0.0;
		double SumCrossCm = 0.0;

		bool GetIsOnTarget() const
		{
			return GroundHits > 0 && static_cast<float>(OnTarget) / GroundHits >= MinOnTargetShareForDive;
		}
	};

	struct FRecorderState
	{
		TArray<FDiveRecord> Dives;
		bool bIsEnabled = false;
		FString Label;
		int32 ShotCount = 0;
		int32 ShotsOffNose = 0;
		double SumNoseAngleDeg = 0.0;
		float MaxNoseAngleDeg = 0.f;
		int32 ImpactCount = 0;
		int32 GroundOrUnitHits = 0;
		double SumAbsImpactLeadSeconds = 0.0;
		double SumCrossTrackMissCm = 0.0;
		double SumAlongTrackMissCm = 0.0;
		double SumSquaredAlongTrackMissCm = 0.0;
		double SumSlantDistanceCm = 0.0;
		float FirstImpactTime = -1.f;
		float LastImpactTime = -1.f;
		int32 GroundProbes = 0;
		int32 GroundProbeHits = 0;
		int32 NearMissHits = 0;
		int32 NearMissKills = 0;
		int32 CannonSplashes = 0;
		TArray<FString> CsvLines;
	};

	FRecorderState& GetState()
	{
		static FRecorderState State;
		return State;
	}

	float GetYawBetweenDeg(const FVector& A, const FVector& B)
	{
		const FVector A2D = FVector(A.X, A.Y, 0.f).GetSafeNormal();
		const FVector B2D = FVector(B.X, B.Y, 0.f).GetSafeNormal();
		if (A2D.IsNearlyZero() || B2D.IsNearlyZero())
		{
			return 0.f;
		}
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A2D, B2D), -1.f, 1.f)));
	}

	FString FormatVector(const FVector& Vector)
	{
		return FString::Printf(TEXT("%.0f,%.0f,%.0f"), Vector.X, Vector.Y, Vector.Z);
	}

	/** @return Index of the weapon's aircraft's latest dive in State.Dives; INDEX_NONE without one. */
	int32 FindLatestDiveOfWeapon(const FRecorderState& State, const UObject* Weapon)
	{
		const AActor* const Aircraft = Weapon != nullptr ? Weapon->GetTypedOuter<AActor>() : nullptr;
		if (Aircraft == nullptr)
		{
			return INDEX_NONE;
		}
		for (int32 DiveIndex = State.Dives.Num() - 1; DiveIndex >= 0; --DiveIndex)
		{
			if (State.Dives[DiveIndex].Aircraft.Get() == Aircraft)
			{
				return DiveIndex;
			}
		}
		return INDEX_NONE;
	}

	void AddImpactToDive(FDiveRecord& Dive, const bool bHitSomething, const FVector& ImpactLocation,
	                     const FVector& FlightForward)
	{
		++Dive.Impacts;
		if (not bHitSomething)
		{
			return;
		}
		++Dive.GroundHits;
		// A unit is measured where it is now; a ground strafe against its whole line.
		const AActor* const TargetActor = Dive.TargetActor.Get();
		const FVector RunStart = IsValid(TargetActor) ? TargetActor->GetActorLocation() : Dive.RunStart;
		const FVector RunEnd = IsValid(TargetActor) ? RunStart : Dive.RunEnd;
		const FVector Impact2D(ImpactLocation.X, ImpactLocation.Y, 0.f);
		const FVector Closest = FMath::ClosestPointOnSegment(Impact2D, FVector(RunStart.X, RunStart.Y, 0.f),
		                                                     FVector(RunEnd.X, RunEnd.Y, 0.f));
		const FVector Miss = Impact2D - Closest;
		const float Distance = Miss.Size();
		Dive.SumDistanceCm += Distance;
		Dive.MinDistanceCm = FMath::Min(Dive.MinDistanceCm, Distance);
		Dive.OnTarget += Distance <= OnTargetRadiusCm ? 1 : 0;
		Dive.NearTarget += Distance <= NearTargetRadiusCm ? 1 : 0;
		const FVector Forward2D = FVector(FlightForward.X, FlightForward.Y, 0.f).GetSafeNormal();
		Dive.SumAlongCm += FVector::DotProduct(Miss, Forward2D);
		Dive.SumCrossCm += FVector::DotProduct(Miss, FVector::CrossProduct(FVector::UpVector, Forward2D));
	}

	FString GetDiveKey(const FRecorderState& State, const UObject* Weapon)
	{
		const int32 DiveIndex = FindLatestDiveOfWeapon(State, Weapon);
		if (not State.Dives.IsValidIndex(DiveIndex))
		{
			return TEXT("none");
		}
		const FDiveRecord& Dive = State.Dives[DiveIndex];
		const AActor* const TargetActor = Dive.TargetActor.Get();
		const FVector Target = IsValid(TargetActor) ? TargetActor->GetActorLocation() : Dive.RunStart;
		return FString::Printf(TEXT("%s#%d@%.0f;%.0f;%.0f"), *Dive.AircraftName, Dive.DiveIndexOfAircraft, Target.X,
		                       Target.Y, Target.Z);
	}

	bool GetIsLaterDive(const FDiveRecord& Dive)
	{
		return Dive.DiveIndexOfAircraft > 0;
	}
}

void FAircraftStrafeShotRecorder::Enable(const FString& Label)
{
	using namespace AircraftStrafeShotRecorderPrivate;
	FRecorderState& State = GetState();
	State = FRecorderState();
	State.bIsEnabled = true;
	State.Label = Label;
	State.CsvLines.Add(TEXT("kind,weapon,time,launch_x,launch_y,launch_z,end_x,end_y,end_z,aim_x,aim_y,aim_z,"
		"nose_x,nose_y,nose_z,hit,nose_angle,impact_lead,dive"));
}

void FAircraftStrafeShotRecorder::Disable()
{
	AircraftStrafeShotRecorderPrivate::GetState().bIsEnabled = false;
}

bool FAircraftStrafeShotRecorder::GetIsEnabled()
{
	return AircraftStrafeShotRecorderPrivate::GetState().bIsEnabled;
}

void FAircraftStrafeShotRecorder::RecordShot(
	const UObject* Weapon,
	const FVector& MuzzleLocation,
	const FVector& FireDirection,
	const FVector& NoseForward,
	const FVector& AimTarget)
{
	using namespace AircraftStrafeShotRecorderPrivate;
	FRecorderState& State = GetState();
	if (not State.bIsEnabled)
	{
		return;
	}
	const FVector Direction = FireDirection.GetSafeNormal();
	const FVector Nose = NoseForward.GetSafeNormal();
	const float NoseAngleDeg = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, Nose), -1.f, 1.f)));
	const float YawDeg = GetYawBetweenDeg(Direction, Nose);
	++State.ShotCount;
	State.SumNoseAngleDeg += NoseAngleDeg;
	State.MaxNoseAngleDeg = FMath::Max(State.MaxNoseAngleDeg, NoseAngleDeg);
	if (YawDeg > OffNoseYawDeg || NoseAngleDeg > OffNoseTotalDeg)
	{
		++State.ShotsOffNose;
	}
	const UWorld* const ShotWorld = Weapon != nullptr ? Weapon->GetWorld() : nullptr;
	State.CsvLines.Add(FString::Printf(TEXT("shot,%s,%.3f,%s,%s,%s,%s,0,%.1f,0,%s"),
	                                   Weapon ? *Weapon->GetName() : TEXT("none"),
	                                   ShotWorld != nullptr ? ShotWorld->GetTimeSeconds() : 0.f,
	                                   *FormatVector(MuzzleLocation),
	                                   *FormatVector(MuzzleLocation + Direction * 1000.f),
	                                   *FormatVector(AimTarget),
	                                   *FString::Printf(TEXT("%.3f,%.3f,%.3f"), Nose.X, Nose.Y, Nose.Z),
	                                   NoseAngleDeg, *GetDiveKey(State, Weapon)));
}

void FAircraftStrafeShotRecorder::RecordImpact(
	const UObject* Weapon,
	const bool bHitSomething,
	const FVector& LaunchLocation,
	const FVector& ImpactLocation,
	const FVector& AimTarget,
	const FVector& FlightForward,
	const float LaunchTime,
	const float ImpactTime)
{
	using namespace AircraftStrafeShotRecorderPrivate;
	FRecorderState& State = GetState();
	if (not State.bIsEnabled)
	{
		return;
	}
	const float SlantDistance = FVector::Dist(LaunchLocation, ImpactLocation);
	const float TracerFlightSeconds = SlantDistance /
		static_cast<float>(DeveloperSettings::GamePlay::Projectile::SmallArmsProjectileSpeed);
	// Positive: the impact shows before the tracer gets there.
	const float ImpactLeadSeconds = TracerFlightSeconds - (ImpactTime - LaunchTime);
	++State.ImpactCount;
	State.SumAbsImpactLeadSeconds += FMath::Abs(ImpactLeadSeconds);
	State.SumSlantDistanceCm += SlantDistance;
	State.FirstImpactTime = State.FirstImpactTime < 0.f ? ImpactTime : State.FirstImpactTime;
	State.LastImpactTime = ImpactTime;
	if (bHitSomething)
	{
		++State.GroundOrUnitHits;
		const FVector Forward2D = FVector(FlightForward.X, FlightForward.Y, 0.f).GetSafeNormal();
		const FVector Right2D = FVector::CrossProduct(FVector::UpVector, Forward2D);
		const FVector Miss = ImpactLocation - AimTarget;
		const float AlongTrack = FVector::DotProduct(Miss, Forward2D);
		State.SumCrossTrackMissCm += FMath::Abs(FVector::DotProduct(Miss, Right2D));
		State.SumAlongTrackMissCm += AlongTrack;
		State.SumSquaredAlongTrackMissCm += AlongTrack * AlongTrack;
	}
	const int32 DiveIndex = FindLatestDiveOfWeapon(State, Weapon);
	if (State.Dives.IsValidIndex(DiveIndex))
	{
		AddImpactToDive(State.Dives[DiveIndex], bHitSomething, ImpactLocation, FlightForward);
	}
	State.CsvLines.Add(FString::Printf(TEXT("impact,%s,%.3f,%s,%s,%s,0,0,0,%d,0,%.3f,%s"),
	                                   Weapon ? *Weapon->GetName() : TEXT("none"),
	                                   ImpactTime,
	                                   *FormatVector(LaunchLocation),
	                                   *FormatVector(ImpactLocation),
	                                   *FormatVector(AimTarget),
	                                   bHitSomething ? 1 : 0,
	                                   ImpactLeadSeconds, *GetDiveKey(State, Weapon)));
}

void FAircraftStrafeShotRecorder::RecordGroundProbe(const bool bFoundGround)
{
	using namespace AircraftStrafeShotRecorderPrivate;
	FRecorderState& State = GetState();
	if (not State.bIsEnabled)
	{
		return;
	}
	++State.GroundProbes;
	State.GroundProbeHits += bFoundGround ? 1 : 0;
}

void FAircraftStrafeShotRecorder::RecordNearMissHit(const bool bKilled)
{
	using namespace AircraftStrafeShotRecorderPrivate;
	FRecorderState& State = GetState();
	if (not State.bIsEnabled)
	{
		return;
	}
	++State.NearMissHits;
	State.NearMissKills += bKilled ? 1 : 0;
}

void FAircraftStrafeShotRecorder::RecordCannonSplash()
{
	using namespace AircraftStrafeShotRecorderPrivate;
	FRecorderState& State = GetState();
	if (not State.bIsEnabled)
	{
		return;
	}
	++State.CannonSplashes;
}

void FAircraftStrafeShotRecorder::BeginDive(const AActor* Aircraft, const FString& Description,
                                            const AActor* TargetActor, const FVector& RunStart, const FVector& RunEnd)
{
	using namespace AircraftStrafeShotRecorderPrivate;
	FRecorderState& State = GetState();
	if (not State.bIsEnabled || not IsValid(Aircraft))
	{
		return;
	}
	int32 EarlierDives = 0;
	for (const FDiveRecord& Dive : State.Dives)
	{
		EarlierDives += Dive.Aircraft.Get() == Aircraft ? 1 : 0;
	}
	FDiveRecord NewDive;
	NewDive.Aircraft = Aircraft;
	NewDive.AircraftName = Aircraft->GetName();
	NewDive.DiveIndexOfAircraft = EarlierDives;
	NewDive.Description = Description;
	NewDive.TargetActor = TargetActor;
	NewDive.RunStart = RunStart;
	NewDive.RunEnd = RunEnd;
	UE_LOG(LogTemp, Display, TEXT("RTS_STRAFE_DIVE_START aircraft=%s dive=%d %s"), *NewDive.AircraftName,
	       NewDive.DiveIndexOfAircraft, *Description);
	State.Dives.Add(MoveTemp(NewDive));
}

void FAircraftStrafeShotRecorder::LogDives()
{
	using namespace AircraftStrafeShotRecorderPrivate;
	const FRecorderState& State = GetState();
	for (const FDiveRecord& Dive : State.Dives)
	{
		const int32 Hits = FMath::Max(1, Dive.GroundHits);
		UE_LOG(LogTemp, Display,
		       TEXT("RTS_STRAFE_DIVE label=%s aircraft=%s dive=%d impacts=%d ground=%d on_target_4m=%d near_8m=%d "
			       "mean_dist_cm=%.0f min_dist_cm=%.0f mean_along_cm=%.0f mean_cross_cm=%.0f verdict=%s | %s"),
		       *State.Label, *Dive.AircraftName, Dive.DiveIndexOfAircraft, Dive.Impacts, Dive.GroundHits, Dive.OnTarget,
		       Dive.NearTarget, Dive.SumDistanceCm / Hits, Dive.GroundHits > 0 ? Dive.MinDistanceCm : -1.f,
		       Dive.SumAlongCm / Hits, Dive.SumCrossCm / Hits,
		       Dive.Impacts == 0 ? TEXT("no_fire") : Dive.GetIsOnTarget() ? TEXT("on_target") : TEXT("MISS"),
		       *Dive.Description);
	}
	UE_LOG(LogTemp, Display, TEXT("RTS_STRAFE_DIVE_SUMMARY label=%s dives=%d later_dives_fired=%d later_on_target=%d"),
	       *State.Label, State.Dives.Num(), GetLaterDivesWithImpactsCount(), GetLaterDivesOnTargetCount());
}

int32 FAircraftStrafeShotRecorder::GetLaterDivesOnTargetCount()
{
	using namespace AircraftStrafeShotRecorderPrivate;
	int32 Count = 0;
	for (const FDiveRecord& Dive : GetState().Dives)
	{
		Count += GetIsLaterDive(Dive) && Dive.GetIsOnTarget() ? 1 : 0;
	}
	return Count;
}

int32 FAircraftStrafeShotRecorder::GetLaterDivesWithImpactsCount()
{
	using namespace AircraftStrafeShotRecorderPrivate;
	int32 Count = 0;
	for (const FDiveRecord& Dive : GetState().Dives)
	{
		Count += GetIsLaterDive(Dive) && Dive.Impacts > 0 ? 1 : 0;
	}
	return Count;
}

int32 FAircraftStrafeShotRecorder::GetNearMissHitCount()
{
	return AircraftStrafeShotRecorderPrivate::GetState().NearMissHits;
}

int32 FAircraftStrafeShotRecorder::GetNearMissKillCount()
{
	return AircraftStrafeShotRecorderPrivate::GetState().NearMissKills;
}

int32 FAircraftStrafeShotRecorder::GetCannonSplashCount()
{
	return AircraftStrafeShotRecorderPrivate::GetState().CannonSplashes;
}

int32 FAircraftStrafeShotRecorder::GetShotCount()
{
	return AircraftStrafeShotRecorderPrivate::GetState().ShotCount;
}

int32 FAircraftStrafeShotRecorder::GetImpactCount()
{
	return AircraftStrafeShotRecorderPrivate::GetState().ImpactCount;
}

float FAircraftStrafeShotRecorder::GetGroundHitShare()
{
	const auto& State = AircraftStrafeShotRecorderPrivate::GetState();
	return State.ImpactCount > 0 ? static_cast<float>(State.GroundOrUnitHits) / State.ImpactCount : 0.f;
}

float FAircraftStrafeShotRecorder::GetShareOfShotsOutsideNoseCone()
{
	const auto& State = AircraftStrafeShotRecorderPrivate::GetState();
	return State.ShotCount > 0 ? static_cast<float>(State.ShotsOffNose) / State.ShotCount : 0.f;
}

float FAircraftStrafeShotRecorder::GetMeanAbsImpactLeadSeconds()
{
	const auto& State = AircraftStrafeShotRecorderPrivate::GetState();
	return State.ImpactCount > 0 ? static_cast<float>(State.SumAbsImpactLeadSeconds / State.ImpactCount) : 0.f;
}

float FAircraftStrafeShotRecorder::GetMeanCrossTrackMissCm()
{
	const auto& State = AircraftStrafeShotRecorderPrivate::GetState();
	return State.GroundOrUnitHits > 0 ? static_cast<float>(State.SumCrossTrackMissCm / State.GroundOrUnitHits) : 0.f;
}

FString FAircraftStrafeShotRecorder::DescribeMetrics()
{
	const auto& State = AircraftStrafeShotRecorderPrivate::GetState();
	const int32 Hits = FMath::Max(1, State.GroundOrUnitHits);
	const double MeanAlong = State.SumAlongTrackMissCm / Hits;
	const double AlongSigma = FMath::Sqrt(FMath::Max(0.0, State.SumSquaredAlongTrackMissCm / Hits - MeanAlong * MeanAlong));
	const float FiringSeconds = FMath::Max(0.001f, State.LastImpactTime - State.FirstImpactTime);
	return FString::Printf(
		TEXT("label=%s shots=%d impacts=%d ground_hit_share=%.2f air_impacts=%d nose_angle_mean=%.1f nose_angle_max=%.1f "
			"off_nose_share=%.2f impact_lead_abs_mean_s=%.3f slant_mean_cm=%.0f cross_track_miss_mean_cm=%.0f "
			"along_track_miss_mean_cm=%.0f along_track_sigma_cm=%.0f impacts_per_s=%.1f ground_probes=%d probe_hits=%d "
			"near_miss_hits=%d near_miss_kills=%d cannon_splashes=%d"),
		*State.Label, State.ShotCount, State.ImpactCount, GetGroundHitShare(),
		State.ImpactCount - State.GroundOrUnitHits,
		State.ShotCount > 0 ? State.SumNoseAngleDeg / State.ShotCount : 0.0, State.MaxNoseAngleDeg,
		GetShareOfShotsOutsideNoseCone(), GetMeanAbsImpactLeadSeconds(),
		State.ImpactCount > 0 ? State.SumSlantDistanceCm / State.ImpactCount : 0.0,
		GetMeanCrossTrackMissCm(), MeanAlong, AlongSigma, State.ImpactCount / FiringSeconds, State.GroundProbes,
		State.GroundProbeHits, State.NearMissHits, State.NearMissKills, State.CannonSplashes);
}

void FAircraftStrafeShotRecorder::WriteCsv()
{
	const auto& State = AircraftStrafeShotRecorderPrivate::GetState();
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("AircraftStrafeQA");
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
	const FString FilePath = Directory / (State.Label + TEXT(".csv"));
	FFileHelper::SaveStringArrayToFile(State.CsvLines, *FilePath);
	UE_LOG(LogTemp, Display, TEXT("RTS_AIRCRAFT_TEST STRAFE_QA_CSV %s"), *FilePath);
}

#endif
