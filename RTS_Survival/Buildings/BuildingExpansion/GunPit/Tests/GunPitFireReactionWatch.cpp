// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GunPitFireReactionWatch.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "RTS_Survival/Buildings/BuildingExpansion/GunPit/GunPitAnimInstance.h"

namespace GunPitFireReactionWatchPrivate
{
	const FName CrewHeadBone = TEXT("head");
	const FName CrewPelvisBone = TEXT("pelvis");

	FVector GetHeadFromPelvis(const USkeletalMeshComponent* CrewMesh)
	{
		return IsValid(CrewMesh)
			       ? CrewMesh->GetBoneLocation(CrewHeadBone, EBoneSpaces::ComponentSpace)
			       - CrewMesh->GetBoneLocation(CrewPelvisBone, EBoneSpaces::ComponentSpace)
			       : FVector::ZeroVector;
	}
}

void FGunPitFireReactionWatch::Start(const float WorldSeconds, const USkeletalMeshComponent& GunMesh,
                                     const TArray<TWeakObjectPtr<USkeletalMeshComponent>>& CrewMeshes)
{
	const UGunPitAnimInstance* GunAnimInstance = Cast<UGunPitAnimInstance>(GunMesh.GetAnimInstance());
	KickBone = IsValid(GunAnimInstance) ? GunAnimInstance->GetFireKickBone() : NAME_None;
	KickBoneAtShot = KickBone.IsNone() ? FVector::ZeroVector : GunMesh.GetBoneLocation(KickBone, EBoneSpaces::ComponentSpace);
	CrewHeadsAtShot.Reset();
	for (const TWeakObjectPtr<USkeletalMeshComponent>& CrewMesh : CrewMeshes)
	{
		CrewHeadsAtShot.Add(GunPitFireReactionWatchPrivate::GetHeadFromPelvis(CrewMesh.Get()));
	}
	StartSeconds = WorldSeconds;
}

void FGunPitFireReactionWatch::Sample(const USkeletalMeshComponent& GunMesh,
                                      const TArray<TWeakObjectPtr<USkeletalMeshComponent>>& CrewMeshes)
{
	if (not KickBone.IsNone())
	{
		MaxKickBoneMove = FMath::Max(MaxKickBoneMove, FVector::Dist(
			                             GunMesh.GetBoneLocation(KickBone, EBoneSpaces::ComponentSpace), KickBoneAtShot));
	}
	for (int32 CrewIndex = FirstIdleCrewIndex; CrewIndex < CrewHeadsAtShot.Num() && CrewIndex < CrewMeshes.Num(); ++CrewIndex)
	{
		const USkeletalMeshComponent* CrewMesh = CrewMeshes[CrewIndex].Get();
		if (not IsValid(CrewMesh))
		{
			continue;
		}
		MaxCrewHeadMove = FMath::Max(MaxCrewHeadMove, FVector::Dist(
			                             GunPitFireReactionWatchPrivate::GetHeadFromPelvis(CrewMesh), CrewHeadsAtShot[CrewIndex]));
	}
}

FString FGunPitFireReactionWatch::Describe() const
{
	return FString::Printf(TEXT("kick_bone=%s max_move=%.1f max_head_move=%.1f"), *KickBone.ToString(), MaxKickBoneMove,
	                       MaxCrewHeadMove);
}

#endif
