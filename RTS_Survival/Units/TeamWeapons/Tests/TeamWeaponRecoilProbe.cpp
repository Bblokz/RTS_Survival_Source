// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TeamWeaponRecoilProbe.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "RTS_Survival/RTSComponents/VehicleFireFeedbackComponent/AnimMeshFeedbackComponent.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTeamWeaponRecoilProbe, Log, All);

namespace TeamWeaponRecoilProbePrivate
{
	// The feedback component keeps its mesh and settings protected; the probe reads them by property name.
	const FName AnimatedMeshPropertyName(TEXT("M_AnimatedMesh"));
	const FName FeedbackSettingsPropertyName(TEXT("M_FeedbackSettings"));
	const FName AxesSettingsPropertyName(TEXT("M_AnimMeshRecoilAxesSettings"));
	// A kick that never settles is logged after this long so a stuck spring still shows up.
	constexpr float MaxKickSeconds = 3.f;

	USkeletalMeshComponent* GetAnimatedMesh(const UAnimMeshFeedbackComponent& Feedback)
	{
		const FObjectPropertyBase* MeshProperty = FindFProperty<FObjectPropertyBase>(
			Feedback.GetClass(), AnimatedMeshPropertyName);
		if (MeshProperty == nullptr)
		{
			return nullptr;
		}
		return Cast<USkeletalMeshComponent>(MeshProperty->GetObjectPropertyValue_InContainer(&Feedback));
	}

	FString ExportStructProperty(const UObject& Object, const FName PropertyName)
	{
		const FStructProperty* StructProperty = FindFProperty<FStructProperty>(Object.GetClass(), PropertyName);
		if (StructProperty == nullptr)
		{
			return TEXT("missing");
		}
		FString Text;
		StructProperty->ExportTextItem_InContainer(Text, &Object, nullptr, nullptr, PPF_None);
		return Text;
	}


	FVector GetHorizontalOrForward(const FVector& Direction)
	{
		const FVector Horizontal = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
		return Horizontal.IsNearlyZero() ? FVector::ForwardVector : Horizontal;
	}

	void KeepPeak(float& InOutPeak, const float Value)
	{
		if (FMath::Abs(Value) > FMath::Abs(InOutPeak))
		{
			InOutPeak = Value;
		}
	}
}

void FTeamWeaponRecoilProbe::Tick(const FString& SubjectName, ATeamWeapon& Weapon, const float GameSeconds)
{
	using namespace TeamWeaponRecoilProbePrivate;
	const UAnimMeshFeedbackComponent* Feedback = Weapon.FindComponentByClass<UAnimMeshFeedbackComponent>();
	if (not IsValid(Feedback))
	{
		if (not bM_LoggedSetup)
		{
			UE_LOG(LogRTSTeamWeaponRecoilProbe, Display, TEXT("RTS_TWQC recoil_setup subject=%s feedback=none"),
			       *SubjectName);
			bM_LoggedSetup = true;
		}
		return;
	}
	USkeletalMeshComponent* Mesh = GetAnimatedMesh(*Feedback);
	if (not IsValid(Mesh))
	{
		Mesh = Weapon.FindComponentByClass<USkeletalMeshComponent>();
	}
	if (not IsValid(Mesh))
	{
		return;
	}
	if (not bM_LoggedSetup)
	{
		LogSetup(SubjectName, Weapon, *Feedback, *Mesh);
		bM_LoggedSetup = true;
	}

	// World space: the feedback mesh is usually the actor root, so the kick moves the actor itself.
	const FTransform MeshWorld = Mesh->GetComponentTransform();
	const FName FireSocket = GetFireSocket(Weapon);
	const FVector MuzzleWorld = Mesh->DoesSocketExist(FireSocket)
		                            ? Mesh->GetSocketLocation(FireSocket)
		                            : Mesh->GetComponentLocation();

	const bool bIsKicking = Feedback->IsComponentTickEnabled();
	if (bIsKicking && not bM_WasKicking)
	{
		StartKick(Weapon, *Mesh, GameSeconds);
	}
	if (bIsKicking)
	{
		SampleKick(MeshWorld, MuzzleWorld);
	}
	const bool bKickTooLong = bIsKicking && GameSeconds - M_Kick.StartSeconds > MaxKickSeconds;
	if ((bM_WasKicking && not bIsKicking) || bKickTooLong)
	{
		LogKick(SubjectName, Weapon, GameSeconds);
		if (bKickTooLong)
		{
			StartKick(Weapon, *Mesh, GameSeconds);
		}
	}
	bM_WasKicking = bIsKicking;
	M_LastMeshWorld = MeshWorld;
	M_LastMuzzleWorld = MuzzleWorld;
}

void FTeamWeaponRecoilProbe::LogSetup(const FString& SubjectName, const ATeamWeapon& Weapon,
                                      const UAnimMeshFeedbackComponent& Feedback,
                                      const USkeletalMeshComponent& Mesh) const
{
	using namespace TeamWeaponRecoilProbePrivate;
	const FTransform MeshInActorSpace = Mesh.GetComponentTransform().GetRelativeTransform(Weapon.GetActorTransform());
	const FName FireSocket = GetFireSocket(Weapon);
	FVector SocketForwardInActorSpace = FVector::ZeroVector;
	if (Mesh.DoesSocketExist(FireSocket))
	{
		SocketForwardInActorSpace = Weapon.GetActorTransform().InverseTransformVectorNoScale(
			Mesh.GetSocketTransform(FireSocket).GetUnitAxis(EAxis::X));
	}
	const USceneComponent* MeshParent = Mesh.GetAttachParent();
	UE_LOG(LogRTSTeamWeaponRecoilProbe, Display,
	       TEXT("RTS_TWQC recoil_setup subject=%s class=%s feedback=%s mesh=%s parent=%s mesh_rel_rot=%s "
		       "mesh_in_actor_rot=%s mesh_scale=%s fire_socket=%s socket_fwd_actor=%s feedback_settings=%s axes=%s"),
	       *SubjectName, *Weapon.GetClass()->GetName(), *Feedback.GetName(), *Mesh.GetName(),
	       IsValid(MeshParent) ? *MeshParent->GetName() : TEXT("none"), *Mesh.GetRelativeRotation().ToString(),
	       *MeshInActorSpace.Rotator().ToString(), *Mesh.GetRelativeScale3D().ToString(), *FireSocket.ToString(),
	       *SocketForwardInActorSpace.ToString(), *ExportStructProperty(Feedback, FeedbackSettingsPropertyName),
	       *ExportStructProperty(Feedback, AxesSettingsPropertyName));
}

void FTeamWeaponRecoilProbe::StartKick(const ATeamWeapon& Weapon, const USkeletalMeshComponent& Mesh,
                                       const float GameSeconds)
{
	using namespace TeamWeaponRecoilProbePrivate;
	M_RestMeshWorld = M_LastMeshWorld;
	M_RestMuzzleWorld = M_LastMuzzleWorld;
	M_Kick = FTWRecoilKick();
	M_Kick.StartSeconds = GameSeconds;
	M_KickCount++;

	const FName FireSocket = GetFireSocket(Weapon);
	if (not Mesh.DoesSocketExist(FireSocket))
	{
		M_BarrelForward = GetHorizontalOrForward(Weapon.GetActorForwardVector());
		return;
	}
	const FVector SocketForwardWorld = Mesh.GetSocketTransform(FireSocket).GetUnitAxis(EAxis::X);
	M_BarrelForward = GetHorizontalOrForward(SocketForwardWorld);
}

void FTeamWeaponRecoilProbe::SampleKick(const FTransform& MeshWorld, const FVector& MuzzleWorld)
{
	using namespace TeamWeaponRecoilProbePrivate;
	const FVector Lateral = FVector::CrossProduct(FVector::UpVector, M_BarrelForward).GetSafeNormal();
	const FVector Offset = MeshWorld.GetLocation() - M_RestMeshWorld.GetLocation();
	KeepPeak(M_Kick.PeakBackCm, -FVector::DotProduct(Offset, M_BarrelForward));
	KeepPeak(M_Kick.PeakLateralCm, FVector::DotProduct(Offset, Lateral));
	KeepPeak(M_Kick.PeakUpCm, Offset.Z);

	const FQuat Delta = MeshWorld.GetRotation() * M_RestMeshWorld.GetRotation().Inverse();
	const FVector RotationVectorDeg = FMath::RadiansToDegrees(Delta.ToRotationVector());
	KeepPeak(M_Kick.PeakPitchDeg, FVector::DotProduct(RotationVectorDeg, Lateral));
	KeepPeak(M_Kick.PeakYawDeg, RotationVectorDeg.Z);
	KeepPeak(M_Kick.PeakRollDeg, FVector::DotProduct(RotationVectorDeg, M_BarrelForward));

	const FVector MuzzleOffset = MuzzleWorld - M_RestMuzzleWorld;
	KeepPeak(M_Kick.PeakMuzzleBackCm, -FVector::DotProduct(MuzzleOffset, M_BarrelForward));
	KeepPeak(M_Kick.PeakMuzzleLateralCm, FVector::DotProduct(MuzzleOffset, Lateral));
	KeepPeak(M_Kick.PeakMuzzleUpCm, MuzzleOffset.Z);
}

void FTeamWeaponRecoilProbe::LogKick(const FString& SubjectName, const ATeamWeapon& Weapon,
                                     const float GameSeconds) const
{
	const float BarrelWorldYaw = FMath::RadiansToDegrees(FMath::Atan2(M_BarrelForward.Y, M_BarrelForward.X));
	UE_LOG(LogRTSTeamWeaponRecoilProbe, Display,
	       TEXT("RTS_TWQC recoil_kick subject=%s kick=%d t=%.2f dur=%.2f barrel_yaw_world=%.1f back_cm=%.2f "
		       "lateral_cm=%.2f up_cm=%.2f pitch_deg=%.2f yaw_deg=%.2f roll_deg=%.2f muzzle_back_cm=%.2f "
		       "muzzle_lateral_cm=%.2f muzzle_up_cm=%.2f"),
	       *SubjectName, M_KickCount, M_Kick.StartSeconds, GameSeconds - M_Kick.StartSeconds, BarrelWorldYaw,
	       M_Kick.PeakBackCm, M_Kick.PeakLateralCm, M_Kick.PeakUpCm, M_Kick.PeakPitchDeg, M_Kick.PeakYawDeg,
	       M_Kick.PeakRollDeg, M_Kick.PeakMuzzleBackCm, M_Kick.PeakMuzzleLateralCm, M_Kick.PeakMuzzleUpCm);
}

FName FTeamWeaponRecoilProbe::GetFireSocket(const ATeamWeapon& Weapon)
{
	const TArray<UWeaponState*> Weapons = const_cast<ATeamWeapon&>(Weapon).GetWeapons();
	if (Weapons.Num() == 0 || not IsValid(Weapons[0]))
	{
		return NAME_None;
	}
	return Weapons[0]->GetWeaponSocket();
}

#endif // RTS_WITH_MAP_TESTS
