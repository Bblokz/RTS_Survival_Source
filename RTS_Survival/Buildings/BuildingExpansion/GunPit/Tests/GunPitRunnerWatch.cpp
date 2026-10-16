// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GunPitRunnerWatch.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

bool FGunPitRunnerWatch::Find(const AActor& Turret)
{
	TArray<USkeletalMeshComponent*> Meshes;
	Turret.GetComponents<USkeletalMeshComponent>(Meshes);
	USkeletalMeshComponent* const* Runner = Meshes.FindByPredicate([this](const USkeletalMeshComponent* Mesh)
	{
		return IsValid(Mesh) && Mesh->ComponentHasTag(RunnerTag);
	});
	M_Runner = Runner ? *Runner : nullptr;
	TArray<UStaticMeshComponent*> StaticMeshes;
	Turret.GetComponents<UStaticMeshComponent>(StaticMeshes);
	UStaticMeshComponent* const* Round = StaticMeshes.FindByPredicate([this](const UStaticMeshComponent* Mesh)
	{
		return IsValid(Mesh) && Mesh->GetFName() == RoundName;
	});
	M_Round = Round ? *Round : nullptr;
	return M_Runner.IsValid() && M_Round.IsValid();
}

bool FGunPitRunnerWatch::GetIsOnStaticSocket(const USkeletalMeshComponent& GunMesh) const
{
	return M_Runner.IsValid() && M_Runner->GetAttachParent() == &GunMesh && M_Runner->GetAttachSocketName() == StaticSocket
		&& M_Round.IsValid() && M_Round->GetAttachParent() == M_Runner.Get();
}

void FGunPitRunnerWatch::Sample(const float NowSeconds, const USkeletalMeshComponent& GunMesh)
{
	if (not M_Round.IsValid())
	{
		return;
	}
	if (M_StartSeconds < 0.f)
	{
		M_StartSeconds = NowSeconds;
	}
	if (M_Round->GetComponentScale().GetMax() < ShownScale)
	{
		++M_HiddenSamples;
		return;
	}
	++M_ShownSamples;
	FTransform Frame = GunMesh.GetSocketTransform(StaticSocket);
	Frame.SetScale3D(FVector::OneVector);
	const FVector RoundLocal = Frame.InverseTransformPosition(M_Round->GetComponentLocation());
	M_ClosestToNiche = FMath::Min(M_ClosestToNiche, FVector::Dist(RoundLocal, NicheRound));
	M_ClosestToDrop = FMath::Min(M_ClosestToDrop, FVector::Dist(RoundLocal, DropRound));
}

FString FGunPitRunnerWatch::Describe() const
{
	return FString::Printf(TEXT("closest_to_niche=%.1f closest_to_drop=%.1f shown=%d hidden=%d"), M_ClosestToNiche,
	                       M_ClosestToDrop, M_ShownSamples, M_HiddenSamples);
}

#endif
