// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GunPitCrewComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GunPitCrewAnimInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace GunPitCrew
{
	// The drill ends this long before the weapon is ready, so the next shot never lands while it is still loading.
	constexpr float LoadingEndMarginSeconds = 0.25f;
	constexpr float MinimumDrillSeconds = 0.1f;
	constexpr int32 RoundCount = 2;
}

UGunPitCrewComponent::UGunPitCrewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGunPitCrewComponent::InitPitCrew(USkeletalMeshComponent* GunMesh)
{
	M_GunMesh = GunMesh;
	if (not GetIsValidGunMesh())
	{
		return;
	}
	InitPitCrew_GatherAndAttachCrew();
	InitPitCrew_AttachTaggedProps();
	if (M_CrewMeshes.Num() != ReloadSettings.CrewAnimations.Num())
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Gun pit on %s has %d crew meshes but %d crew animation entries; the extra crew stay still."),
			*GetNameSafe(GetOwner()), M_CrewMeshes.Num(), ReloadSettings.CrewAnimations.Num()));
	}
	SetCrewIdleAnimations();
	InitPitCrew_GatherRounds();
	const UWorld* World = GetWorld();
	if (GetHasShellCarry() && IsValid(World))
	{
		World->GetTimerManager().SetTimer(M_FirstRoundTimer, this, &UGunPitCrewComponent::PlaceFirstRound,
		                                  FMath::Max(ShellCarry.FirstRoundDelaySeconds, KINDA_SMALL_NUMBER), false);
	}
}

void UGunPitCrewComponent::InitPitCrew_GatherAndAttachCrew()
{
	M_CrewMeshes.Reset();
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	TArray<USkeletalMeshComponent*> TaggedCrew;
	Owner->GetComponents<USkeletalMeshComponent>(TaggedCrew);
	TaggedCrew.RemoveAll([this](const USkeletalMeshComponent* Mesh)
	{
		return not IsValid(Mesh) || not Mesh->ComponentHasTag(CrewTag);
	});
	// Crew1..CrewN match CrewAnimations by order.
	TaggedCrew.Sort([](const USkeletalMeshComponent& A, const USkeletalMeshComponent& B)
	{
		return A.GetName() < B.GetName();
	});
	const FAttachmentTransformRules Rules = bKeepCrewBlueprintOffset
		                                        ? FAttachmentTransformRules::KeepRelativeTransform
		                                        : FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	for (USkeletalMeshComponent* CrewMesh : TaggedCrew)
	{
		CrewMesh->AttachToComponent(M_GunMesh.Get(), Rules, CrewAttachSocket);
		M_CrewMeshes.Add(CrewMesh);
	}
}

void UGunPitCrewComponent::InitPitCrew_AttachTaggedProps() const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	TArray<USceneComponent*> SceneComponents;
	Owner->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Prop : SceneComponents)
	{
		if (not IsValid(Prop) || Prop == M_GunMesh.Get())
		{
			continue;
		}
		for (const FName& Tag : Prop->ComponentTags)
		{
			const FString TagString = Tag.ToString();
			if (not TagString.StartsWith(PropSocketTagPrefix))
			{
				continue;
			}
			const FName Socket(*TagString.RightChop(PropSocketTagPrefix.Len()));
			// The relative transform set in the blueprint is the offset from the socket.
			Prop->AttachToComponent(M_GunMesh.Get(), FAttachmentTransformRules::KeepRelativeTransform, Socket);
			break;
		}
	}
}

void UGunPitCrewComponent::InitPitCrew_GatherRounds()
{
	M_Rounds.Reset();
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner) || ShellCarry.RoundTag.IsNone())
	{
		return;
	}
	TArray<UStaticMeshComponent*> Rounds;
	Owner->GetComponents<UStaticMeshComponent>(Rounds);
	Rounds.RemoveAll([this](const UStaticMeshComponent* Round)
	{
		return not IsValid(Round) || not Round->ComponentHasTag(ShellCarry.RoundTag);
	});
	if (Rounds.IsEmpty())
	{
		return;
	}
	Rounds.Sort([](const UStaticMeshComponent& A, const UStaticMeshComponent& B)
	{
		return A.GetName() < B.GetName();
	});
	for (UStaticMeshComponent* Round : Rounds)
	{
		Round->SetVisibility(false);
		Round->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_Rounds.Add(Round);
	}
	const bool bValidCrew = M_CrewMeshes.IsValidIndex(ShellCarry.HandlerCrewIndex) &&
		M_CrewMeshes.IsValidIndex(ShellCarry.LoaderCrewIndex);
	// The handler fetches the next round while the loader still carries this one; the swap in OnRoundRammed
	// relies on that order.
	const bool bValidOrder = ShellCarry.HandoverFraction < ShellCarry.RamFraction &&
		ShellCarry.PickFraction < ShellCarry.RamFraction;
	if (M_Rounds.Num() != GunPitCrew::RoundCount || not bValidCrew || not bValidOrder)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Gun pit on %s has %d rounds tagged %s (needs %d), handler/loader indices %d/%d or carry fractions"
				" handover %.2f pick %.2f ram %.2f that do not fit; the rounds stay hidden."),
			*GetNameSafe(GetOwner()), M_Rounds.Num(), *ShellCarry.RoundTag.ToString(), GunPitCrew::RoundCount,
			ShellCarry.HandlerCrewIndex, ShellCarry.LoaderCrewIndex, ShellCarry.HandoverFraction,
			ShellCarry.PickFraction, ShellCarry.RamFraction));
		M_Rounds.Reset();
	}
}

void UGunPitCrewComponent::OnGunFired()
{
	// A gun reload montage starts with its own recoil; a second recoil on top would restart the slot.
	if (bM_IsLoading && IsValid(ReloadSettings.GunReloadMontage))
	{
		return;
	}
	PlayGunMontage(ReloadSettings.GunFireMontage, 1.f, 0.f);
	if (not bM_IsLoading)
	{
		PlayCrewFireMontages();
	}
}

void UGunPitCrewComponent::PlayCrewFireMontages() const
{
	const int32 CrewCount = FMath::Min(M_CrewMeshes.Num(), ReloadSettings.CrewAnimations.Num());
	for (int32 CrewIndex = 0; CrewIndex < CrewCount; ++CrewIndex)
	{
		UAnimMontage* FireMontage = ReloadSettings.CrewAnimations[CrewIndex].FireMontage;
		const USkeletalMeshComponent* CrewMesh = M_CrewMeshes[CrewIndex].Get();
		const UAnimInstance* AnimInstance = IsValid(CrewMesh) ? CrewMesh->GetAnimInstance() : nullptr;
		if (not IsValid(FireMontage) || not IsValid(AnimInstance) || AnimInstance->Montage_IsPlaying(FireMontage))
		{
			continue;
		}
		PlayMontageOnMesh(CrewMesh, FireMontage, 1.f, 0.f);
	}
}

void UGunPitCrewComponent::OnReloadStarted(const float ReloadTime)
{
	const UWorld* World = GetWorld();
	if (ReloadTime < ReloadSettings.MinReloadTimeForLoadingSequence || not IsValid(World) || not GetIsValidGunMesh())
	{
		return;
	}
	UAnimMontage* GunReloadMontage = ReloadSettings.GunReloadMontage;
	if (not IsValid(GunReloadMontage) && GetLongestCrewMontageLength() <= 0.f)
	{
		return;
	}
	bM_IsLoading = true;
	// The reload starts a moment after the shot; a gun drill opens with the same recoil, so it picks up where the
	// recoil already is instead of restarting it.
	const float RecoilAlreadyPlayed = IsValid(GunReloadMontage) ? GetPlayingFireMontagePosition() : 0.f;
	const float DrillSeconds = FMath::Max(ReloadTime - GunPitCrew::LoadingEndMarginSeconds, GunPitCrew::MinimumDrillSeconds);
	if (IsValid(GunReloadMontage))
	{
		// Stretch the drill so the breech closes just before the weapon finishes reloading.
		PlayGunMontage(GunReloadMontage, GetStretchedPlayRate(*GunReloadMontage, RecoilAlreadyPlayed, DrillSeconds),
		               RecoilAlreadyPlayed);
	}
	PlayCrewLoadingMontages(DrillSeconds, RecoilAlreadyPlayed);
	ScheduleShellCarry(DrillSeconds);
	World->GetTimerManager().SetTimer(M_LoadingFinishedTimer, this, &UGunPitCrewComponent::OnLoadingFinished,
	                                  DrillSeconds, false);
}

void UGunPitCrewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		for (FTimerHandle* Timer : {&M_LoadingFinishedTimer, &M_FirstRoundTimer, &M_HandoverTimer, &M_RamTimer, &M_PickTimer})
		{
			TimerManager.ClearTimer(*Timer);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UGunPitCrewComponent::PlayGunMontage(UAnimMontage* Montage, const float PlayRate, const float StartPosition) const
{
	if (not GetIsValidGunMesh())
	{
		return;
	}
	PlayMontageOnMesh(M_GunMesh.Get(), Montage, PlayRate, StartPosition);
}

float UGunPitCrewComponent::GetPlayingFireMontagePosition() const
{
	const USkeletalMeshComponent* GunMesh = M_GunMesh.Get();
	const UAnimInstance* AnimInstance = IsValid(GunMesh) ? GunMesh->GetAnimInstance() : nullptr;
	const UAnimMontage* FireMontage = ReloadSettings.GunFireMontage;
	if (not IsValid(AnimInstance) || not IsValid(FireMontage) || not AnimInstance->Montage_IsPlaying(FireMontage))
	{
		return 0.f;
	}
	return AnimInstance->Montage_GetPosition(FireMontage);
}

float UGunPitCrewComponent::GetStretchedPlayRate(const UAnimMontage& Montage, const float StartPosition,
                                                 const float Seconds)
{
	const float RemainingLength = FMath::Max(Montage.GetPlayLength() - StartPosition, GunPitCrew::MinimumDrillSeconds);
	return RemainingLength / Seconds;
}

float UGunPitCrewComponent::GetLongestCrewMontageLength() const
{
	float Longest = 0.f;
	for (const FGunPitCrewMemberAnimations& Member : ReloadSettings.CrewAnimations)
	{
		Longest = IsValid(Member.ReloadMontage) ? FMath::Max(Longest, Member.ReloadMontage->GetPlayLength()) : Longest;
	}
	return Longest;
}

void UGunPitCrewComponent::PlayCrewLoadingMontages(const float DrillSeconds, const float StartPosition) const
{
	const int32 CrewCount = FMath::Min(M_CrewMeshes.Num(), ReloadSettings.CrewAnimations.Num());
	for (int32 CrewIndex = 0; CrewIndex < CrewCount; ++CrewIndex)
	{
		UAnimMontage* LoadingMontage = ReloadSettings.CrewAnimations[CrewIndex].ReloadMontage;
		if (not IsValid(LoadingMontage))
		{
			continue;
		}
		PlayMontageOnMesh(M_CrewMeshes[CrewIndex].Get(), LoadingMontage,
		                  GetStretchedPlayRate(*LoadingMontage, StartPosition, DrillSeconds), StartPosition);
	}
}

void UGunPitCrewComponent::SetCrewIdleAnimations() const
{
	const int32 CrewCount = FMath::Min(M_CrewMeshes.Num(), ReloadSettings.CrewAnimations.Num());
	for (int32 CrewIndex = 0; CrewIndex < CrewCount; ++CrewIndex)
	{
		const USkeletalMeshComponent* CrewMesh = M_CrewMeshes[CrewIndex].Get();
		if (not IsValid(CrewMesh))
		{
			continue;
		}
		UGunPitCrewAnimInstance* CrewAnimInstance = Cast<UGunPitCrewAnimInstance>(CrewMesh->GetAnimInstance());
		if (not IsValid(CrewAnimInstance))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Gun pit crew mesh %s on %s does not run a UGunPitCrewAnimInstance; it cannot idle."),
				*CrewMesh->GetName(), *GetNameSafe(GetOwner())));
			continue;
		}
		CrewAnimInstance->SetIdleAnimation(ReloadSettings.CrewAnimations[CrewIndex].IdleAnimation);
	}
}

void UGunPitCrewComponent::PlayMontageOnMesh(const USkeletalMeshComponent* Mesh, UAnimMontage* Montage,
                                             const float PlayRate, const float StartPosition)
{
	// A crew member removed at runtime (e.g. hidden by a destruction effect) simply stops animating.
	if (not IsValid(Mesh) || not IsValid(Montage))
	{
		return;
	}
	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (not IsValid(AnimInstance))
	{
		return;
	}
	AnimInstance->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength,
	                           FMath::Min(StartPosition, Montage->GetPlayLength()));
}

void UGunPitCrewComponent::OnLoadingFinished()
{
	bM_IsLoading = false;
}

UStaticMeshComponent* UGunPitCrewComponent::GetRoundInFlight() const
{
	return M_Rounds.IsValidIndex(M_RoundInFlightIndex) ? M_Rounds[M_RoundInFlightIndex].Get() : nullptr;
}

bool UGunPitCrewComponent::GetHasShellCarry() const
{
	return M_Rounds.Num() == GunPitCrew::RoundCount;
}

void UGunPitCrewComponent::ScheduleShellCarry(const float DrillSeconds)
{
	const UWorld* World = GetWorld();
	if (not GetHasShellCarry() || not IsValid(World))
	{
		return;
	}
	FTimerManager& TimerManager = World->GetTimerManager();
	const auto Schedule = [this, &TimerManager, DrillSeconds](FTimerHandle& Timer, void (UGunPitCrewComponent::*Event)(),
	                                                           const float Fraction)
	{
		TimerManager.SetTimer(Timer, this, Event, FMath::Max(DrillSeconds * Fraction, KINDA_SMALL_NUMBER), false);
	};
	Schedule(M_HandoverTimer, &UGunPitCrewComponent::OnRoundHandedOver, ShellCarry.HandoverFraction);
	Schedule(M_RamTimer, &UGunPitCrewComponent::OnRoundRammed, ShellCarry.RamFraction);
	Schedule(M_PickTimer, &UGunPitCrewComponent::OnNextRoundPicked, ShellCarry.PickFraction);
}

void UGunPitCrewComponent::PlaceFirstRound()
{
	UStaticMeshComponent* Round = GetRoundInFlight();
	if (not IsValid(Round))
	{
		return;
	}
	PlaceRoundOnGun(*Round, ShellCarry.IdleRound);
	AttachRoundToCrewHand(*Round, ShellCarry.HandlerCrewIndex);
	Round->SetVisibility(true);
}

void UGunPitCrewComponent::OnRoundHandedOver()
{
	UStaticMeshComponent* Round = GetRoundInFlight();
	if (IsValid(Round))
	{
		// The loader's hands close where the handler holds the round; it keeps its place as it changes hands.
		AttachRoundToCrewHand(*Round, ShellCarry.LoaderCrewIndex);
	}
}

void UGunPitCrewComponent::OnRoundRammed()
{
	UStaticMeshComponent* Round = GetRoundInFlight();
	if (IsValid(Round))
	{
		Round->SetVisibility(false);
	}
	// The other round, picked from the crate meanwhile, is the next one to hand over.
	M_RoundInFlightIndex = (M_RoundInFlightIndex + 1) % GunPitCrew::RoundCount;
}

void UGunPitCrewComponent::OnNextRoundPicked()
{
	const int32 NextIndex = (M_RoundInFlightIndex + 1) % GunPitCrew::RoundCount;
	UStaticMeshComponent* NextRound = M_Rounds[NextIndex].Get();
	if (not IsValid(NextRound))
	{
		return;
	}
	PlaceRoundOnGun(*NextRound, ShellCarry.CrateRound);
	AttachRoundToCrewHand(*NextRound, ShellCarry.HandlerCrewIndex);
	NextRound->SetVisibility(true);
}

void UGunPitCrewComponent::PlaceRoundOnGun(UStaticMeshComponent& Round, const FGunPitRoundPose& Pose) const
{
	if (not GetIsValidGunMesh())
	{
		return;
	}
	Round.AttachToComponent(M_GunMesh.Get(), FAttachmentTransformRules::KeepRelativeTransform, CrewAttachSocket);
	Round.SetRelativeLocationAndRotation(Pose.Location, FRotationMatrix::MakeFromX(Pose.Axis.GetSafeNormal()).Rotator());
}

void UGunPitCrewComponent::AttachRoundToCrewHand(UStaticMeshComponent& Round, const int32 CrewIndex) const
{
	USkeletalMeshComponent* CrewMesh = M_CrewMeshes.IsValidIndex(CrewIndex) ? M_CrewMeshes[CrewIndex].Get() : nullptr;
	if (not IsValid(CrewMesh))
	{
		return;
	}
	// Keep the world pose: the round was placed where the hand holds it, so it rides in the hand from here on.
	Round.AttachToComponent(CrewMesh, FAttachmentTransformRules::KeepWorldTransform, ShellCarry.HandBone);
}

bool UGunPitCrewComponent::GetIsValidGunMesh() const
{
	if (M_GunMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_GunMesh"),
		TEXT("UGunPitCrewComponent::GetIsValidGunMesh"),
		GetOwner());
	return false;
}
