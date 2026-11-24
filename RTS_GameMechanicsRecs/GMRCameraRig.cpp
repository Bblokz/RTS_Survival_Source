// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "GMRCameraRig.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace GMRCameraRigPrivate
{
	// Fraction of a shot spent accelerating (and again decelerating) on the S-curve clock.
	constexpr float EaseRampFraction = 0.35f;
	constexpr float OcclusionBoostStepDegrees = 5.f;
	constexpr float MaxOcclusionBoostDegrees = 35.f;
	constexpr int32 OcclusionSamples = 7;
	// Hits this close to the look point (fraction of the camera distance) belong to the subject itself.
	constexpr float SubjectBulkFraction = 0.12f;
	constexpr float GroundTraceUp = 20000.f;
	constexpr float GroundTraceDown = 60000.f;
	constexpr float ClearanceSmoothSeconds = 0.35f;
	constexpr float MaxPitchDegrees = 85.f;
	constexpr float MinDistance = 100.f;
	constexpr float MinFocal = 8.f;
	constexpr float ShakePitchFrequency = 0.31f;
	constexpr float ShakeYawFrequency = 0.23f;
	constexpr float ShakeRollFrequency = 0.17f;
	constexpr float ShakeRollShare = 0.3f;
	constexpr float FollowHeadingLagScale = 2.f;
	// Continuous occlusion avoidance (degrees of extra elevation around the look point).
	constexpr float MaxDynamicBoostDegrees = 40.f;
	constexpr float DynamicRaiseRate = 45.f;
	constexpr float DynamicLowerRate = 6.f;
	constexpr float DynamicHoldSeconds = 0.8f;
	constexpr float DynamicBoostSmoothSeconds = 0.35f;
	constexpr float DynamicBlockPersistSeconds = 0.25f;
	constexpr float SubjectProbeHeight = 200.f;
	constexpr float SubjectProbeWidth = 250.f;
	constexpr float CameraBodyRadius = 60.f;
	constexpr float MaxShotPitchDegrees = 50.f;
	constexpr float MaxDynamicElevationDegrees = 55.f;
	// Manifest "boost" = pitch boost + 0.001 x yaw swing, so a swing shows up without a new field.
	constexpr float YawSwingReportScale = 0.001f;

	/** @brief Critically damped spring (Game Programming Gems 4) used for every smoothed channel. */
	template <typename T>
	T SmoothDamp(const T& Current, const T& Target, T& InOutVelocity, const float SmoothTime, const float DeltaSeconds)
	{
		const float SafeSmoothTime = FMath::Max(0.0001f, SmoothTime);
		const float Omega = 2.f / SafeSmoothTime;
		const float X = Omega * DeltaSeconds;
		const float Decay = 1.f / (1.f + X + 0.48f * X * X + 0.235f * X * X * X);
		const T Change = Current - Target;
		const T Temp = (InOutVelocity + Change * Omega) * DeltaSeconds;
		InOutVelocity = (InOutVelocity - Temp * Omega) * Decay;
		return Target + (Change + Temp) * Decay;
	}

	/** @brief Position on a velocity profile that ramps up and down with smoothstep shoulders. */
	float RampedPosition(const float Alpha, const float RampIn, const float RampOut)
	{
		const float PeakVelocity = 1.f / (1.f - 0.5f * RampIn - 0.5f * RampOut);
		auto RampArea = [](const float X) { return X * X * X - 0.5f * X * X * X * X; };
		if (RampIn > 0.f && Alpha < RampIn)
		{
			return PeakVelocity * RampIn * RampArea(Alpha / RampIn);
		}
		if (RampOut > 0.f && Alpha > 1.f - RampOut)
		{
			return 1.f - PeakVelocity * RampOut * RampArea((1.f - Alpha) / RampOut);
		}
		return PeakVelocity * (0.5f * RampIn + (Alpha - RampIn));
	}

	/** @brief Non-uniform Catmull-Rom (cubic Hermite) through keyed values. */
	template <typename T>
	T EvaluateKeys(const TArray<float>& Times, const TArray<T>& Values, const float Time)
	{
		const int32 Num = Values.Num();
		if (Num == 1 || Time <= Times[0])
		{
			return Values[0];
		}
		if (Time >= Times[Num - 1])
		{
			return Values[Num - 1];
		}
		int32 Segment = 0;
		while (Segment < Num - 2 && Time > Times[Segment + 1])
		{
			++Segment;
		}
		auto Tangent = [&Times, &Values, Num](const int32 Index) -> T
		{
			const int32 Before = FMath::Max(0, Index - 1);
			const int32 After = FMath::Min(Num - 1, Index + 1);
			return (Values[After] - Values[Before]) / FMath::Max(Times[After] - Times[Before], KINDA_SMALL_NUMBER);
		};
		const float SegmentSeconds = FMath::Max(Times[Segment + 1] - Times[Segment], KINDA_SMALL_NUMBER);
		const float S = (Time - Times[Segment]) / SegmentSeconds;
		const float S2 = S * S;
		const float S3 = S2 * S;
		const float H00 = 2.f * S3 - 3.f * S2 + 1.f;
		const float H10 = S3 - 2.f * S2 + S;
		const float H01 = -2.f * S3 + 3.f * S2;
		const float H11 = S3 - S2;
		return Values[Segment] * H00 + Tangent(Segment) * (H10 * SegmentSeconds)
			+ Values[Segment + 1] * H01 + Tangent(Segment + 1) * (H11 * SegmentSeconds);
	}

	FVector SphereOffset(const float YawDegrees, const float PitchDegrees, const float Distance)
	{
		const float Yaw = FMath::DegreesToRadians(YawDegrees);
		const float Pitch = FMath::DegreesToRadians(FMath::Clamp(PitchDegrees, -MaxPitchDegrees, MaxPitchDegrees));
		return FVector(FMath::Cos(Pitch) * FMath::Cos(Yaw), FMath::Cos(Pitch) * FMath::Sin(Yaw), FMath::Sin(Pitch))
			* FMath::Max(MinDistance, Distance);
	}

	bool GetIsIgnoredHit(const FHitResult& Hit, const TArray<const AActor*>& IgnoredActors)
	{
		const AActor* HitActor = Hit.GetActor();
		return HitActor == nullptr || IgnoredActors.Contains(HitActor) || HitActor->IsA<APawn>()
			|| HitActor->IsHidden();
	}
}

float FGMRCameraRig::EaseShotAlpha(const float Alpha, const FString& Ease)
{
	using namespace GMRCameraRigPrivate;
	const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
	if (Ease.Equals(TEXT("Linear"), ESearchCase::IgnoreCase))
	{
		return Clamped;
	}
	if (Ease.Equals(TEXT("In"), ESearchCase::IgnoreCase))
	{
		return RampedPosition(Clamped, EaseRampFraction, 0.f);
	}
	if (Ease.Equals(TEXT("Out"), ESearchCase::IgnoreCase))
	{
		return RampedPosition(Clamped, 0.f, EaseRampFraction);
	}
	return RampedPosition(Clamped, EaseRampFraction, EaseRampFraction);
}

float FGMRCameraRig::BeginShot(const FGMRShot& Shot, const FGMRJob& Job, UWorld& World,
                               const FGMRPivotResolver& Resolver, const TArray<const AActor*>& IgnoredActors)
{
	M_Shot = Shot;
	if (M_Shot.Keys.Num() == 0)
	{
		M_Shot.Keys.Add(FGMRCamKey());
	}
	M_Shot.Keys.Sort([](const FGMRCamKey& A, const FGMRCamKey& B) { return A.Time < B.Time; });
	M_AnchorRotation = FRotator(0.f, Job.AnchorYaw, 0.f);
	bM_IsVertical = Job.ResolutionY > Job.ResolutionX;
	M_Keys.Reset();
	for (const FGMRCamKey& Key : M_Shot.Keys)
	{
		FKeyState& State = M_Keys.AddDefaulted_GetRef();
		State.Key = Key;
	}
	AdaptKeysToAspect(Job);
	M_PitchBoost = 0.f;
	M_YawSwing = 0.f;
	bM_IsFixedRig = M_Shot.Rig.Equals(TEXT("Fixed"), ESearchCase::IgnoreCase);
	UpdatePivots(0.f, Resolver, true);
	UpdateFollowHeading(0.f, true);
	M_PitchBoost = M_Shot.bAvoidOcclusion ? ComputeOcclusionBoost(World, IgnoredActors) : 0.f;
	M_FixedLocation = ComputeRawPose(M_Keys[0].Key.Time).Location;
	ApplyCameraOverride(World, Resolver, IgnoredActors);
	M_ClearanceLift = 0.f;
	M_ClearanceLiftVelocity = 0.f;
	bM_FirstEvaluate = true;
	return M_PitchBoost;
}

void FGMRCameraRig::ApplyCameraOverride(UWorld& World, const FGMRPivotResolver& Resolver,
                                        const TArray<const AActor*>& IgnoredActors)
{
	const FVector Offset = bM_IsVertical && not M_Shot.VCameraOffset.IsNearlyZero() ? M_Shot.VCameraOffset : M_Shot.CameraOffset;
	if (M_Shot.CameraPivot.IsEmpty() && Offset.IsNearlyZero())
	{
		return;
	}
	FVector Base;
	FVector Heading;
	if (not Resolver(M_Shot.CameraPivot, Base, Heading))
	{
		return;
	}
	FVector Location = Base + ToWorld(FVector(Offset.X, Offset.Y, 0.f));
	bool bFoundGround = false;
	const float GroundZ = GetGroundHeight(World, Location, IgnoredActors, bFoundGround);
	Location.Z = (bFoundGround ? GroundZ : Base.Z) + Offset.Z;
	M_FixedLocation = Location;
}

void FGMRCameraRig::AdaptKeysToAspect(const FGMRJob& Job)
{
	if (not bM_IsVertical)
	{
		return;
	}
	for (FKeyState& State : M_Keys)
	{
		FGMRCamKey& Key = State.Key;
		Key.Yaw = Key.VYaw != 0.f ? Key.VYaw : Key.Yaw;
		Key.Pitch = Key.VPitch != 0.f ? Key.VPitch : Key.Pitch + Job.VerticalPitchAdd;
		Key.Distance = Key.VDistance > 0.f ? Key.VDistance : Key.Distance * Job.VerticalDistanceScale;
		Key.Focal = Key.VFocal > 0.f ? Key.VFocal : Key.Focal * Job.VerticalFocalScale;
		Key.LookOffset = Key.VLookOffset.IsNearlyZero() ? Key.LookOffset : Key.VLookOffset;
	}
}

void FGMRCameraRig::UpdatePivots(const float DeltaSeconds, const FGMRPivotResolver& Resolver, const bool bSnap)
{
	using namespace GMRCameraRigPrivate;
	for (FKeyState& State : M_Keys)
	{
		FVector Raw;
		FVector Heading;
		if (not Resolver(State.Key.Pivot, Raw, Heading))
		{
			continue;
		}
		if (bSnap || not State.bHasPivot)
		{
			State.SmoothedPivot = Raw;
			State.PivotVelocity = FVector::ZeroVector;
		}
		else
		{
			State.SmoothedPivot = SmoothDamp(State.SmoothedPivot, Raw, State.PivotVelocity, M_Shot.PivotLag, DeltaSeconds);
		}
		if (not Heading.IsNearlyZero())
		{
			State.Heading = Heading.GetSafeNormal2D();
		}
		State.bHasPivot = true;
	}
}

void FGMRCameraRig::UpdateFollowHeading(const float DeltaSeconds, const bool bSnap)
{
	using namespace GMRCameraRigPrivate;
	if (M_Keys.Num() == 0)
	{
		return;
	}
	const FVector2D Target(M_Keys[0].Heading.X, M_Keys[0].Heading.Y);
	if (bSnap)
	{
		M_FollowDirection = Target.GetSafeNormal();
		M_FollowDirectionVelocity = FVector2D::ZeroVector;
		return;
	}
	M_FollowDirection = SmoothDamp(M_FollowDirection, Target, M_FollowDirectionVelocity,
	                               M_Shot.PivotLag * FollowHeadingLagScale, DeltaSeconds);
}

float FGMRCameraRig::GetSplineTime(const float ShotSeconds) const
{
	const float FirstTime = M_Keys[0].Key.Time;
	const float LastTime = M_Keys.Last().Key.Time;
	if (LastTime - FirstTime <= KINDA_SMALL_NUMBER)
	{
		return FirstTime;
	}
	const float Alpha = (ShotSeconds - FirstTime) / (LastTime - FirstTime);
	return FirstTime + EaseShotAlpha(Alpha, M_Shot.Ease) * (LastTime - FirstTime);
}

FVector FGMRCameraRig::ToWorld(const FVector& AnchorFrameVector) const
{
	return M_AnchorRotation.RotateVector(AnchorFrameVector);
}

FGMRCameraPose FGMRCameraRig::ComputeRawPose(const float ShotSeconds) const
{
	using namespace GMRCameraRigPrivate;
	const int32 Num = M_Keys.Num();
	TArray<float> Times;
	TArray<FVector> Centers;
	TArray<FVector> LookOffsets;
	TArray<float> Yaws;
	TArray<float> Pitches;
	TArray<float> Distances;
	TArray<float> Focals;
	TArray<float> Rolls;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FKeyState& State = M_Keys[Index];
		Times.Add(State.Key.Time);
		Centers.Add(State.SmoothedPivot + ToWorld(State.Key.Offset));
		LookOffsets.Add(ToWorld(State.Key.LookOffset));
		Yaws.Add(State.Key.Yaw + M_YawSwing);
		// Never steeper than MaxShotPitch from occlusion lifts: near top-down shots read as a map, not a film.
		Pitches.Add(FMath::Min(State.Key.Pitch + M_PitchBoost, FMath::Max(State.Key.Pitch, MaxShotPitchDegrees)));
		Distances.Add(State.Key.Distance);
		Focals.Add(State.Key.Focal);
		Rolls.Add(State.Key.Roll);
	}
	const float SplineTime = GetSplineTime(ShotSeconds);
	const FVector Center = EvaluateKeys(Times, Centers, SplineTime);
	const float BaseYaw = M_Shot.Rig.Equals(TEXT("Follow"), ESearchCase::IgnoreCase)
		                      ? FMath::RadiansToDegrees(FMath::Atan2(M_FollowDirection.Y, M_FollowDirection.X))
		                      : M_AnchorRotation.Yaw;
	FGMRCameraPose Pose;
	Pose.LookAt = Center + EvaluateKeys(Times, LookOffsets, SplineTime);
	if (M_Shot.Rig.Equals(TEXT("Fixed"), ESearchCase::IgnoreCase) && not bM_FirstEvaluate)
	{
		Pose.Location = M_FixedLocation;
	}
	else
	{
		Pose.Location = Center + SphereOffset(BaseYaw + EvaluateKeys(Times, Yaws, SplineTime),
		                                      EvaluateKeys(Times, Pitches, SplineTime),
		                                      EvaluateKeys(Times, Distances, SplineTime));
	}
	Pose.Rotation = (Pose.LookAt - Pose.Location).Rotation();
	Pose.Rotation.Roll = EvaluateKeys(Times, Rolls, SplineTime);
	Pose.FocalLength = FMath::Max(MinFocal, EvaluateKeys(Times, Focals, SplineTime));
	Pose.FocusDistance = FVector::Dist(Pose.Location, Pose.LookAt);
	return Pose;
}

float FGMRCameraRig::ComputeOcclusionBoost(UWorld& World, const TArray<const AActor*>& IgnoredActors)
{
	using namespace GMRCameraRigPrivate;
	const float FirstTime = M_Keys[0].Key.Time;
	const float LastTime = FMath::Max(M_Keys.Last().Key.Time, M_Shot.End - M_Shot.Start);
	// Candidates in order of preference: as designed, small pitch lifts, then the same shot swung around the
	// subject, and only then steep lifts (near top-down shots over smoke read worst).
	struct FCandidate
	{
		float Boost;
		float Swing;
	};
	TArray<FCandidate> Candidates = {{0.f, 0.f}, {5.f, 0.f}, {10.f, 0.f}};
	for (const float Swing : {30.f, -30.f, 60.f, -60.f})
	{
		Candidates.Add({0.f, Swing});
		Candidates.Add({5.f, Swing});
	}
	for (float Boost = 15.f; Boost <= MaxOcclusionBoostDegrees; Boost += OcclusionBoostStepDegrees)
	{
		Candidates.Add({Boost, 0.f});
	}
	FCandidate Best = Candidates[0];
	int32 FewestBlocked = TNumericLimits<int32>::Max();
	for (const FCandidate& Candidate : Candidates)
	{
		M_PitchBoost = Candidate.Boost;
		M_YawSwing = bM_IsFixedRig ? 0.f : Candidate.Swing;
		bM_FirstEvaluate = true;
		M_FixedLocation = ComputeRawPose(FirstTime).Location;
		bM_FirstEvaluate = false;
		int32 Blocked = 0;
		for (int32 Sample = 0; Sample < OcclusionSamples; ++Sample)
		{
			const float SampleSeconds = FMath::Lerp(FirstTime, LastTime, Sample / static_cast<float>(OcclusionSamples - 1));
			const FGMRCameraPose Pose = ComputeRawPose(SampleSeconds);
			Blocked += GetIsSubjectHidden(World, Pose, IgnoredActors) ? 1 : 0;
		}
		if (Blocked < FewestBlocked)
		{
			FewestBlocked = Blocked;
			Best = Candidate;
		}
		if (Blocked == 0)
		{
			break;
		}
	}
	M_PitchBoost = Best.Boost;
	M_YawSwing = bM_IsFixedRig ? 0.f : Best.Swing;
	bM_FirstEvaluate = true;
	return M_PitchBoost + FMath::Abs(M_YawSwing) * YawSwingReportScale;
}

bool FGMRCameraRig::GetIsSightBlocked(UWorld& World, const FVector& From, const FVector& To,
                                      const TArray<const AActor*>& IgnoredActors) const
{
	using namespace GMRCameraRigPrivate;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GMRCameraOcclusion), false);
	TArray<FHitResult> Hits;
	World.LineTraceMultiByObjectType(Hits, To, From, ObjectParams, QueryParams);
	const float SubjectBulk = FVector::Dist(From, To) * SubjectBulkFraction;
	for (const FHitResult& Hit : Hits)
	{
		if (GetIsIgnoredHit(Hit, IgnoredActors) || FVector::Dist(Hit.ImpactPoint, To) < SubjectBulk)
		{
			continue;
		}
		return true;
	}
	return false;
}

float FGMRCameraRig::GetGroundHeight(UWorld& World, const FVector& Location, const TArray<const AActor*>& IgnoredActors,
                                     bool& bOutFound) const
{
	using namespace GMRCameraRigPrivate;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GMRCameraGround), false);
	TArray<FHitResult> Hits;
	World.LineTraceMultiByObjectType(Hits, Location + FVector(0.f, 0.f, GroundTraceUp),
	                                 Location - FVector(0.f, 0.f, GroundTraceDown), ObjectParams, QueryParams);
	for (const FHitResult& Hit : Hits)
	{
		if (GetIsIgnoredHit(Hit, IgnoredActors))
		{
			continue;
		}
		bOutFound = true;
		return Hit.ImpactPoint.Z;
	}
	bOutFound = false;
	return Location.Z;
}

void FGMRCameraRig::RaiseAroundLook(FGMRCameraPose& InOutPose, const float Degrees)
{
	using namespace GMRCameraRigPrivate;
	if (Degrees <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Offset = InOutPose.Location - InOutPose.LookAt;
	const float Distance = Offset.Size();
	const float Elevation = FMath::RadiansToDegrees(FMath::Atan2(Offset.Z, Offset.Size2D()));
	const float Raised = FMath::DegreesToRadians(FMath::Min(FMath::Max(Elevation, MaxDynamicElevationDegrees), Elevation + Degrees));
	const FVector Flat = Offset.GetSafeNormal2D();
	InOutPose.Location = InOutPose.LookAt + (Flat * FMath::Cos(Raised) + FVector::UpVector * FMath::Sin(Raised)) * Distance;
}

void FGMRCameraRig::UpdateDynamicBoost(UWorld& World, const FGMRCameraPose& RawPose,
                                       const TArray<const AActor*>& IgnoredActors, const float DeltaSeconds)
{
	using namespace GMRCameraRigPrivate;
	// Something moved between the camera and the subject (a ruin the dolly passes, a dune crest): crane up
	// over it, and only come back down once the lower angle has been clear for a moment.
	FGMRCameraPose Test = RawPose;
	RaiseAroundLook(Test, M_DynamicBoostTarget);
	if (GetIsSubjectHidden(World, Test, IgnoredActors))
	{
		// A pole or pylon sweeping past is gone again in a few frames: only a lasting block lifts the camera.
		M_BlockedSeconds += DeltaSeconds;
		M_ClearSeconds = 0.f;
		if (M_BlockedSeconds > DynamicBlockPersistSeconds)
		{
			M_DynamicBoostTarget = FMath::Min(MaxDynamicBoostDegrees, M_DynamicBoostTarget + DynamicRaiseRate * DeltaSeconds);
		}
	}
	else
	{
		M_BlockedSeconds = 0.f;
		M_ClearSeconds += DeltaSeconds;
		if (M_ClearSeconds > DynamicHoldSeconds && M_DynamicBoostTarget > 0.f)
		{
			const float Lower = FMath::Max(0.f, M_DynamicBoostTarget - DynamicLowerRate * DeltaSeconds);
			FGMRCameraPose LowerTest = RawPose;
			RaiseAroundLook(LowerTest, Lower);
			if (not GetIsSubjectHidden(World, LowerTest, IgnoredActors))
			{
				M_DynamicBoostTarget = Lower;
			}
		}
	}
	M_DynamicBoost = SmoothDamp(M_DynamicBoost, M_DynamicBoostTarget, M_DynamicBoostVelocity, DynamicBoostSmoothSeconds,
	                            DeltaSeconds);
}

bool FGMRCameraRig::GetIsCameraEmbedded(UWorld& World, const FVector& Location,
                                        const TArray<const AActor*>& IgnoredActors) const
{
	using namespace GMRCameraRigPrivate;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FOverlapResult> Overlaps;
	World.OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, ObjectParams,
	                               FCollisionShape::MakeSphere(CameraBodyRadius),
	                               FCollisionQueryParams(SCENE_QUERY_STAT(GMRCameraEmbedded), false));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* Actor = Overlap.GetActor();
		if (Actor != nullptr && not IgnoredActors.Contains(Actor) && not Actor->IsA<APawn>() && not Actor->IsHidden())
		{
			return true;
		}
	}
	return false;
}

bool FGMRCameraRig::GetIsSubjectHidden(UWorld& World, const FGMRCameraPose& Pose,
                                       const TArray<const AActor*>& IgnoredActors) const
{
	using namespace GMRCameraRigPrivate;
	// A camera inside a wall or a house sees black: that always counts as hidden.
	if (GetIsCameraEmbedded(World, Pose.Location, IgnoredActors))
	{
		return true;
	}
	// Three points on the subject; it is hidden only when most of them are, so a thin occluder never counts.
	const FVector Forward = (Pose.LookAt - Pose.Location).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector Points[] = {
		Pose.LookAt, Pose.LookAt + FVector(0.f, 0.f, SubjectProbeHeight), Pose.LookAt + Right * SubjectProbeWidth
	};
	int32 Blocked = 0;
	for (const FVector& Point : Points)
	{
		Blocked += GetIsSightBlocked(World, Pose.Location, Point, IgnoredActors) ? 1 : 0;
	}
	return Blocked >= 2;
}

void FGMRCameraRig::ApplyShake(const float ShotSeconds, FRotator& InOutRotation) const
{
	using namespace GMRCameraRigPrivate;
	if (M_Shot.Shake <= 0.f)
	{
		return;
	}
	InOutRotation.Pitch += M_Shot.Shake * FMath::PerlinNoise1D(ShotSeconds * ShakePitchFrequency + 11.1f);
	InOutRotation.Yaw += M_Shot.Shake * FMath::PerlinNoise1D(ShotSeconds * ShakeYawFrequency + 37.7f);
	InOutRotation.Roll += M_Shot.Shake * ShakeRollShare * FMath::PerlinNoise1D(ShotSeconds * ShakeRollFrequency + 71.3f);
}

FGMRCameraPose FGMRCameraRig::Evaluate(const float ShotSeconds, const float DeltaSeconds, UWorld& World,
                                       const FGMRPivotResolver& Resolver, const TArray<const AActor*>& IgnoredActors)
{
	using namespace GMRCameraRigPrivate;
	UpdatePivots(DeltaSeconds, Resolver, bM_FirstEvaluate);
	UpdateFollowHeading(DeltaSeconds, bM_FirstEvaluate);
	const bool bWasFirst = bM_FirstEvaluate;
	bM_FirstEvaluate = false;
	const FGMRCameraPose RawPose = ComputeRawPose(ShotSeconds);
	if (bWasFirst)
	{
		M_DynamicBoost = 0.f;
		M_DynamicBoostTarget = 0.f;
		M_DynamicBoostVelocity = 0.f;
		M_ClearSeconds = 0.f;
		M_BlockedSeconds = 0.f;
	}
	else if (M_Shot.bAvoidOcclusion)
	{
		UpdateDynamicBoost(World, RawPose, IgnoredActors, DeltaSeconds);
	}
	FGMRCameraPose Pose = RawPose;
	RaiseAroundLook(Pose, M_DynamicBoost);
	bool bFoundGround = false;
	const float GroundZ = GetGroundHeight(World, Pose.Location, IgnoredActors, bFoundGround);
	const float WantedLift = bFoundGround ? FMath::Max(0.f, GroundZ + M_Shot.MinGroundClearance - Pose.Location.Z) : 0.f;
	if (bWasFirst)
	{
		M_ClearanceLift = WantedLift;
		M_ClearanceLiftVelocity = 0.f;
	}
	else
	{
		M_ClearanceLift = SmoothDamp(M_ClearanceLift, WantedLift, M_ClearanceLiftVelocity, ClearanceSmoothSeconds,
		                             DeltaSeconds);
		// Never dip below the wanted lift: a smoothed lift that lags behind rising ground would clip into it.
		M_ClearanceLift = FMath::Max(M_ClearanceLift, WantedLift * 0.9f);
	}
	Pose.Location.Z += M_ClearanceLift;
	const float Roll = Pose.Rotation.Roll;
	Pose.Rotation = (Pose.LookAt - Pose.Location).Rotation();
	Pose.Rotation.Roll = Roll;
	ApplyShake(ShotSeconds, Pose.Rotation);
	Pose.FocusDistance = FVector::Dist(Pose.Location, Pose.LookAt);
	return Pose;
}
