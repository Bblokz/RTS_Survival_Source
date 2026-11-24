// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MegaphoneBroadcastComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MegaphoneLineSet.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"

// Verbose lines name every broadcast; tests switch it on with "log LogMegaphoneBroadcast Verbose".
DEFINE_LOG_CATEGORY_STATIC(LogMegaphoneBroadcast, Log, All);

namespace MegaphoneBroadcastConstants
{
	// Retry interval while the owning expansion is not standing (being placed, built or packed).
	constexpr float InactiveOwnerRetrySeconds = 5.f;
	// A sound without a known duration (for example a looping cue) is given this long before the next line.
	constexpr float FallbackLineSeconds = 8.f;
	constexpr float CutLineFadeOutSeconds = 0.15f;
}

UMegaphoneBroadcastComponent::UMegaphoneBroadcastComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMegaphoneBroadcastComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_BindToOwnerDeath();
	if (bStartOnBeginPlay)
	{
		StartBroadcast();
	}
}

void UMegaphoneBroadcastComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBroadcast();
	UnbindFromOwnerDeath();
	Super::EndPlay(EndPlayReason);
}

void UMegaphoneBroadcastComponent::BeginPlay_BindToOwnerDeath()
{
	IHealthBarOwner* HealthOwner = Cast<IHealthBarOwner>(GetOwner());
	if (HealthOwner == nullptr)
	{
		return;
	}
	if (FOnUnitDies* OnUnitDies = HealthOwner->GetOnUnitDiesDelegate())
	{
		OnUnitDies->AddUObject(this, &UMegaphoneBroadcastComponent::OnOwnerDies);
	}
}

void UMegaphoneBroadcastComponent::UnbindFromOwnerDeath()
{
	IHealthBarOwner* HealthOwner = Cast<IHealthBarOwner>(GetOwner());
	if (HealthOwner == nullptr)
	{
		return;
	}
	if (FOnUnitDies* OnUnitDies = HealthOwner->GetOnUnitDiesDelegate())
	{
		OnUnitDies->RemoveAll(this);
	}
}

void UMegaphoneBroadcastComponent::OnOwnerDies()
{
	UE_LOG(LogMegaphoneBroadcast, Verbose, TEXT("MEGAPHONE %s owner died, broadcast stops"), *GetNameSafe(GetOwner()));
	bM_OwnerDied = true;
	StopBroadcast();
}

void UMegaphoneBroadcastComponent::StartBroadcast()
{
	if (bM_OwnerDied)
	{
		return;
	}
	StopBroadcast();
	bM_IsBroadcasting = true;
	ScheduleNextLine(RandomInRange(FirstLineDelayRange));
}

void UMegaphoneBroadcastComponent::StopBroadcast()
{
	bM_IsBroadcasting = false;
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_NextLineTimer);
	}
	StopCurrentLine();
}

void UMegaphoneBroadcastComponent::SetLineSets(const TArray<UMegaphoneLineSet*>& NewLineSets)
{
	LineSets.Reset(NewLineSets.Num());
	for (UMegaphoneLineSet* LineSet : NewLineSets)
	{
		LineSets.Add(LineSet);
	}
	M_LastPick = FMegaphoneLinePick();
}

void UMegaphoneBroadcastComponent::ScheduleNextLine(const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	FTimerDelegate PlayNextDelegate;
	PlayNextDelegate.BindWeakLambda(this, [this]()
	{
		PlayNextLine();
	});
	World->GetTimerManager().SetTimer(M_NextLineTimer, PlayNextDelegate, FMath::Max(DelaySeconds, 0.01f), false);
}

void UMegaphoneBroadcastComponent::PlayNextLine()
{
	if (not bM_IsBroadcasting || bM_OwnerDied)
	{
		return;
	}
	if (not GetCanBroadcastNow())
	{
		ScheduleNextLine(MegaphoneBroadcastConstants::InactiveOwnerRetrySeconds);
		return;
	}
	FMegaphoneLinePick Pick;
	const FMegaphoneLine* Line = PickNextLine(Pick) ? GetLine(Pick) : nullptr;
	if (Line == nullptr)
	{
		// No playable line (empty sets): stay quiet without an error, an empty megaphone is a valid setup.
		bM_IsBroadcasting = false;
		return;
	}
	M_LastPick = Pick;
	M_CurrentLineAudio = UGameplayStatics::SpawnSoundAttached(
		Line->Sound, this, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true,
		VolumeMultiplier, 1.f, 0.f, Attenuation, nullptr, true);

	// Timed on the wave length rather than OnAudioFinished: a line out of earshot may be virtualised or culled
	// and never report finishing, but the loop must keep going so it is mid-broadcast when the player arrives.
	const float LineSeconds = Line->Sound->GetDuration();
	const bool bHasKnownLength = LineSeconds > 0.f && LineSeconds < INDEFINITELY_LOOPING_DURATION;
	const float PlaySeconds = bHasKnownLength ? LineSeconds : MegaphoneBroadcastConstants::FallbackLineSeconds;
	ScheduleNextLine(PlaySeconds + RandomInRange(PauseBetweenLinesRange));
	UE_LOG(LogMegaphoneBroadcast, Verbose, TEXT("MEGAPHONE %s plays %s (set %d line %d, %.2f s)"),
	       *GetNameSafe(GetOwner()), *GetNameSafe(Line->Sound), Pick.SetIndex, Pick.LineIndex, PlaySeconds);
}

void UMegaphoneBroadcastComponent::StopCurrentLine()
{
	UAudioComponent* Audio = M_CurrentLineAudio.Get();
	M_CurrentLineAudio.Reset();
	if (not IsValid(Audio))
	{
		return;
	}
	Audio->FadeOut(MegaphoneBroadcastConstants::CutLineFadeOutSeconds, 0.f);
}

bool UMegaphoneBroadcastComponent::GetCanBroadcastNow() const
{
	const ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(GetOwner());
	if (not IsValid(Expansion))
	{
		return true;
	}
	// Megaphones placed on a map never get an owner and keep their initial status; only the in-between states of a
	// player- or AI-built expansion silence it.
	switch (Expansion->GetBuildingExpansionStatus())
	{
	case EBuildingExpansionStatus::BXS_LookingForPlacement:
	case EBuildingExpansionStatus::BXS_BeingBuild:
	case EBuildingExpansionStatus::BXS_BeingPackedUp:
	case EBuildingExpansionStatus::BXS_PackedUp:
	case EBuildingExpansionStatus::BXS_BeingUnpacked:
		return false;
	default:
		return true;
	}
}

bool UMegaphoneBroadcastComponent::PickNextLine(FMegaphoneLinePick& OutPick) const
{
	TArray<FMegaphoneLinePick> Candidates;
	TArray<float> Weights;
	float TotalWeight = 0.f;
	for (int32 SetIndex = 0; SetIndex < LineSets.Num(); ++SetIndex)
	{
		const UMegaphoneLineSet* LineSet = LineSets[SetIndex];
		if (not IsValid(LineSet) || LineSet->SelectionWeight <= 0.f)
		{
			continue;
		}
		for (int32 LineIndex = 0; LineIndex < LineSet->Lines.Num(); ++LineIndex)
		{
			if (not IsValid(LineSet->Lines[LineIndex].Sound))
			{
				continue;
			}
			Candidates.Add({SetIndex, LineIndex});
			Weights.Add(LineSet->SelectionWeight);
			TotalWeight += LineSet->SelectionWeight;
		}
	}
	if (Candidates.IsEmpty())
	{
		return false;
	}
	const int32 LastIndex = Candidates.IndexOfByKey(M_LastPick);
	if (Candidates.Num() > 1 && LastIndex != INDEX_NONE)
	{
		TotalWeight -= Weights[LastIndex];
		Weights[LastIndex] = 0.f;
	}
	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		Roll -= Weights[CandidateIndex];
		if (Roll <= 0.f && Weights[CandidateIndex] > 0.f)
		{
			OutPick = Candidates[CandidateIndex];
			return true;
		}
	}
	// Float rounding left a sliver: take the last candidate that may play.
	for (int32 CandidateIndex = Candidates.Num() - 1; CandidateIndex >= 0; --CandidateIndex)
	{
		if (Weights[CandidateIndex] > 0.f)
		{
			OutPick = Candidates[CandidateIndex];
			return true;
		}
	}
	return false;
}

const FMegaphoneLine* UMegaphoneBroadcastComponent::GetLine(const FMegaphoneLinePick& Pick) const
{
	if (not LineSets.IsValidIndex(Pick.SetIndex))
	{
		return nullptr;
	}
	const UMegaphoneLineSet* LineSet = LineSets[Pick.SetIndex];
	if (not IsValid(LineSet) || not LineSet->Lines.IsValidIndex(Pick.LineIndex))
	{
		return nullptr;
	}
	return &LineSet->Lines[Pick.LineIndex];
}

float UMegaphoneBroadcastComponent::RandomInRange(const FVector2D& Range)
{
	const float Low = FMath::Min(Range.X, Range.Y);
	const float High = FMath::Max(Range.X, Range.Y);
	return FMath::FRandRange(Low, High);
}
