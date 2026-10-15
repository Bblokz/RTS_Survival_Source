// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GunPitAnimInstance.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace GunPitFireKick
{
	// After this many damping time constants the kick is too small to see.
	constexpr float SettleTimeConstants = 5.f;
	// The muzzle jumps a moment after the carriage starts back.
	constexpr float MuzzleJumpLagSeconds = 0.03f;
	// An autocannon's kicks overlap; older shots beyond this many are dropped.
	constexpr int32 MaxOverlappingShots = 8;
}

void UGunPitAnimInstance::PlayFireKick()
{
	if (not M_FireKickState.bHasGunFrame && not CacheGunFrame())
	{
		return;
	}
	if (M_FireKickState.ShotAges.Num() >= GunPitFireKick::MaxOverlappingShots)
	{
		M_FireKickState.ShotAges.RemoveAt(0);
	}
	M_FireKickState.ShotAges.Add(0.f);
}

void UGunPitAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The curve is from the last evaluated pose; one frame of delay is invisible on a slow loading blend.
	const float LoadingElevationWeight = LoadingElevationCurveName.IsNone()
		                                     ? 0.f
		                                     : FMath::Clamp(GetCurveValue(LoadingElevationCurveName), 0.f, 1.f);
	AimPitchAlpha = 1.f - LoadingElevationWeight;
	TurntableYawRotation = FRotator(0.f, Yaw, 0.f);

	const float Response = UpdateFireKickResponse(DeltaSeconds);
	UpdateFireKickPose(Response);
	const float MuzzleResponse = GetSummedKickResponse(GunPitFireKick::MuzzleJumpLagSeconds);
	CradlePitchRotation = FRotator(Pitch + FireKick.MuzzleJumpDegrees * MuzzleResponse, 0.f, 0.f);
}

float UGunPitAnimInstance::UpdateFireKickResponse(const float DeltaSeconds)
{
	if (M_FireKickState.ShotAges.IsEmpty())
	{
		return 0.f;
	}
	const float AngularFrequency = UE_TWO_PI * FireKick.FrequencyHz;
	const float SettleSeconds = GunPitFireKick::SettleTimeConstants / (FireKick.DampingRatio * AngularFrequency);
	// Shots are added in order, so the settled ones are at the front.
	for (int32 ShotIndex = M_FireKickState.ShotAges.Num() - 1; ShotIndex >= 0; --ShotIndex)
	{
		M_FireKickState.ShotAges[ShotIndex] += DeltaSeconds;
		if (M_FireKickState.ShotAges[ShotIndex] > SettleSeconds)
		{
			M_FireKickState.ShotAges.RemoveAt(ShotIndex);
		}
	}
	return GetSummedKickResponse(0.f);
}

float UGunPitAnimInstance::GetSummedKickResponse(const float LagSeconds) const
{
	float Response = 0.f;
	for (const float ShotAge : M_FireKickState.ShotAges)
	{
		Response += GetKickResponseAt(ShotAge - LagSeconds);
	}
	return Response;
}

float UGunPitAnimInstance::GetKickResponseAt(const float Seconds) const
{
	if (Seconds <= 0.f)
	{
		return 0.f;
	}
	// Impulse response of a damped spring, scaled so its first swing peaks at 1.
	const float Damping = FMath::Clamp(FireKick.DampingRatio, 0.05f, 0.95f);
	const float AngularFrequency = UE_TWO_PI * FireKick.FrequencyHz;
	const float DampedFrequency = AngularFrequency * FMath::Sqrt(1.f - Damping * Damping);
	const float PeakSeconds = FMath::Atan2(FMath::Sqrt(1.f - Damping * Damping), Damping) / DampedFrequency;
	const float Peak = FMath::Exp(-Damping * AngularFrequency * PeakSeconds) * FMath::Sin(DampedFrequency * PeakSeconds);
	return FMath::Exp(-Damping * AngularFrequency * Seconds) * FMath::Sin(DampedFrequency * Seconds) / Peak;
}

void UGunPitAnimInstance::UpdateFireKickPose(const float Response)
{
	if (FMath::IsNearlyZero(Response))
	{
		FireKickRotation = FRotator::ZeroRotator;
		FireKickTranslation = FVector::ZeroVector;
		return;
	}
	const FQuat Rock(M_FireKickState.RockAxis, FMath::DegreesToRadians(FireKick.RockDegrees * Response));
	// The Modify Bone node turns the kick bone about itself; this moves it as if it turned about the rock pivot.
	const FVector PivotOffset = Rock.RotateVector(M_FireKickState.KickBoneFromPivot) - M_FireKickState.KickBoneFromPivot;
	FireKickRotation = Rock.Rotator();
	FireKickTranslation = M_FireKickState.BackDirection * FireKick.JoltCentimetres * Response + PivotOffset;
}

bool UGunPitAnimInstance::CacheGunFrame()
{
	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkelMeshComponent();
	const USkeletalMesh* SkeletalMesh = IsValid(SkeletalMeshComponent) ? SkeletalMeshComponent->GetSkeletalMeshAsset() : nullptr;
	if (not IsValid(SkeletalMesh))
	{
		return false;
	}
	const USkeletalMeshSocket* GunFrameSocket = SkeletalMesh->FindSocket(FireKick.GunFrameSocket);
	if (not IsValid(GunFrameSocket) || FireKick.KickBone.IsNone())
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Gun pit anim %s on %s has no gun frame socket %s or no kick bone; the gun will not buck on a shot."),
			*GetClass()->GetName(), *SkeletalMesh->GetName(), *FireKick.GunFrameSocket.ToString()));
		FireKick.JoltCentimetres = 0.f;
		FireKick.RockDegrees = 0.f;
		M_FireKickState.bHasGunFrame = true;
		return true;
	}
	// Reference pose, component space: the kick node runs before the turntable yaw, in the unturned gun frame.
	const FTransform SocketBoneReference(SkeletalMesh->GetComposedRefPoseMatrix(GunFrameSocket->BoneName));
	const FTransform GunFrame = GunFrameSocket->GetSocketLocalTransform() * SocketBoneReference;
	FVector Forward = GunFrame.GetUnitAxis(EAxis::X);
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	M_FireKickState.BackDirection = -Forward;
	// A positive turn about this axis lifts the muzzle.
	M_FireKickState.RockAxis = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
	const FVector Pivot = GunFrame.GetLocation() - Forward * FireKick.RockPivotBackCentimetres;
	const FVector KickBoneLocation = SkeletalMesh->GetComposedRefPoseMatrix(FireKick.KickBone).GetOrigin();
	M_FireKickState.KickBoneFromPivot = KickBoneLocation - Pivot;
	M_FireKickState.bHasGunFrame = true;
	return true;
}
