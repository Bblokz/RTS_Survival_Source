// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "GMRJobTypes.h"

class UWorld;

/** @brief Camera state for one frame. */
struct FGMRCameraPose
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector LookAt = FVector::ZeroVector;
	float FocalLength = 35.f;
	float FocusDistance = 1000.f;
};

/**
 * @brief Resolves a key's pivot string to a world location and, for tracked units, their mean heading.
 * Returns false when the pivot has no live target (the rig keeps its last value).
 */
using FGMRPivotResolver = TFunction<bool(const FString& Pivot, FVector& OutLocation, FVector& OutHeading)>;

/**
 * @brief Turns one FGMRShot into a smooth per-frame camera pose: keys are splined over an S-curve shot clock,
 * tracked pivots are spring-smoothed, the camera keeps clear of the ground and is lifted at the shot start when
 * the subject would be hidden behind static geometry. The director owns one rig and calls BeginShot at each cut.
 */
class FGMRCameraRig
{
public:
	/**
	 * @brief Prepares a shot: adapts the keys to the take's aspect, snaps the pivot springs and measures occlusion.
	 * @param Shot Shot to film.
	 * @param Job The job (anchor, aspect, vertical defaults).
	 * @param World World used for ground and occlusion traces.
	 * @param Resolver Pivot lookup supplied by the director.
	 * @param IgnoredActors Units that never count as ground or occluders.
	 * @return Degrees the whole shot was tilted down to keep the subject in view.
	 */
	float BeginShot(const FGMRShot& Shot, const FGMRJob& Job, UWorld& World, const FGMRPivotResolver& Resolver,
	                const TArray<const AActor*>& IgnoredActors);

	/**
	 * @brief Advances the rig one frame.
	 * @param ShotSeconds Seconds since the shot started.
	 * @param DeltaSeconds Frame time (fixed by MRQ).
	 * @param World World used for the ground trace.
	 * @param Resolver Pivot lookup supplied by the director.
	 * @param IgnoredActors Units that never count as ground.
	 * @return The pose to apply to the cine camera.
	 */
	FGMRCameraPose Evaluate(float ShotSeconds, float DeltaSeconds, UWorld& World, const FGMRPivotResolver& Resolver,
	                        const TArray<const AActor*>& IgnoredActors);

	static float EaseShotAlpha(float Alpha, const FString& Ease);

private:
	struct FKeyState
	{
		FGMRCamKey Key;
		FVector SmoothedPivot = FVector::ZeroVector;
		FVector PivotVelocity = FVector::ZeroVector;
		FVector Heading = FVector::ForwardVector;
		bool bHasPivot = false;
	};

	void AdaptKeysToAspect(const FGMRJob& Job);
	void ApplyCameraOverride(UWorld& World, const FGMRPivotResolver& Resolver, const TArray<const AActor*>& IgnoredActors);
	void UpdatePivots(float DeltaSeconds, const FGMRPivotResolver& Resolver, bool bSnap);
	void UpdateFollowHeading(float DeltaSeconds, bool bSnap);
	FGMRCameraPose ComputeRawPose(float ShotSeconds) const;
	float ComputeOcclusionBoost(UWorld& World, const TArray<const AActor*>& IgnoredActors);
	bool GetIsSightBlocked(UWorld& World, const FVector& From, const FVector& To,
	                       const TArray<const AActor*>& IgnoredActors) const;
	float GetGroundHeight(UWorld& World, const FVector& Location, const TArray<const AActor*>& IgnoredActors,
	                      bool& bOutFound) const;
	void ApplyShake(float ShotSeconds, FRotator& InOutRotation) const;
	static void RaiseAroundLook(FGMRCameraPose& InOutPose, float Degrees);
	bool GetIsSubjectHidden(UWorld& World, const FGMRCameraPose& Pose, const TArray<const AActor*>& IgnoredActors) const;
	bool GetIsCameraEmbedded(UWorld& World, const FVector& Location, const TArray<const AActor*>& IgnoredActors) const;
	void UpdateDynamicBoost(UWorld& World, const FGMRCameraPose& RawPose, const TArray<const AActor*>& IgnoredActors,
	                        float DeltaSeconds);
	float GetSplineTime(float ShotSeconds) const;
	FVector ToWorld(const FVector& AnchorFrameVector) const;

	FGMRShot M_Shot;
	TArray<FKeyState> M_Keys;
	FRotator M_AnchorRotation = FRotator::ZeroRotator;
	bool bM_IsVertical = false;
	float M_PitchBoost = 0.f;
	// Degrees the whole shot is swung around its subject to see past an occluder (instead of a steep lift).
	float M_YawSwing = 0.f;
	bool bM_IsFixedRig = false;
	// Follow rig: smoothed heading yaw of the first key's units.
	FVector2D M_FollowDirection = FVector2D(1.f, 0.f);
	FVector2D M_FollowDirectionVelocity = FVector2D::ZeroVector;
	// Fixed rig: camera location frozen at the shot start.
	FVector M_FixedLocation = FVector::ZeroVector;
	// Ground clearance lift, smoothed so terrain bumps under a moving camera do not shake it.
	float M_ClearanceLift = 0.f;
	float M_ClearanceLiftVelocity = 0.f;
	// Elevation added while the subject is hidden behind something mid-shot.
	float M_DynamicBoost = 0.f;
	float M_DynamicBoostTarget = 0.f;
	float M_DynamicBoostVelocity = 0.f;
	float M_ClearSeconds = 0.f;
	float M_BlockedSeconds = 0.f;
	bool bM_FirstEvaluate = true;
};
