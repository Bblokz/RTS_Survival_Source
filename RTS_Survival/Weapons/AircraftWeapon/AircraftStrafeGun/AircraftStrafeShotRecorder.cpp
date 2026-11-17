// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftStrafeShotRecorder.h"

#if RTS_WITH_MAP_TESTS

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/DeveloperSettings.h"

namespace AircraftStrafeShotRecorderPrivate
{
	// A round counts as off the nose beyond the strafe gun's widest cone (yaw 10, depression 35) plus a margin.
	constexpr float OffNoseYawDeg = 11.f;
	constexpr float OffNoseTotalDeg = 37.f;

	struct FRecorderState
	{
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
}

void FAircraftStrafeShotRecorder::Enable(const FString& Label)
{
	using namespace AircraftStrafeShotRecorderPrivate;
	FRecorderState& State = GetState();
	State = FRecorderState();
	State.bIsEnabled = true;
	State.Label = Label;
	State.CsvLines.Add(TEXT("kind,weapon,time,launch_x,launch_y,launch_z,end_x,end_y,end_z,aim_x,aim_y,aim_z,"
		"nose_x,nose_y,nose_z,hit,nose_angle,impact_lead"));
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
	State.CsvLines.Add(FString::Printf(TEXT("shot,%s,0,%s,%s,%s,%s,0,%.1f,0"),
	                                   Weapon ? *Weapon->GetName() : TEXT("none"),
	                                   *FormatVector(MuzzleLocation),
	                                   *FormatVector(MuzzleLocation + Direction * 1000.f),
	                                   *FormatVector(AimTarget),
	                                   *FString::Printf(TEXT("%.3f,%.3f,%.3f"), Nose.X, Nose.Y, Nose.Z),
	                                   NoseAngleDeg));
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
	State.CsvLines.Add(FString::Printf(TEXT("impact,%s,%.3f,%s,%s,%s,0,0,0,%d,0,%.3f"),
	                                   Weapon ? *Weapon->GetName() : TEXT("none"),
	                                   ImpactTime,
	                                   *FormatVector(LaunchLocation),
	                                   *FormatVector(ImpactLocation),
	                                   *FormatVector(AimTarget),
	                                   bHitSomething ? 1 : 0,
	                                   ImpactLeadSeconds));
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
			"along_track_miss_mean_cm=%.0f along_track_sigma_cm=%.0f impacts_per_s=%.1f ground_probes=%d probe_hits=%d"),
		*State.Label, State.ShotCount, State.ImpactCount, GetGroundHitShare(),
		State.ImpactCount - State.GroundOrUnitHits,
		State.ShotCount > 0 ? State.SumNoseAngleDeg / State.ShotCount : 0.0, State.MaxNoseAngleDeg,
		GetShareOfShotsOutsideNoseCone(), GetMeanAbsImpactLeadSeconds(),
		State.ImpactCount > 0 ? State.SumSlantDistanceCm / State.ImpactCount : 0.0,
		GetMeanCrossTrackMissCm(), MeanAlong, AlongSigma, State.ImpactCount / FiringSeconds, State.GroundProbes,
		State.GroundProbeHits);
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
