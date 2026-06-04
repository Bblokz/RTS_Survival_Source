// Copyright (C) Bas Blokzijl - All rights reserved.


#include "SquadUnitSpatialVoiceLinePlayer.h"

#include "Components/AudioComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

USquadUnitSpatialVoiceLinePlayer::USquadUnitSpatialVoiceLinePlayer()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void USquadUnitSpatialVoiceLinePlayer::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupOwnerAsSquadUnit();
}

void USquadUnitSpatialVoiceLinePlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopIdleVoiceLineTimer();
	M_OwningSquadUnit.Reset();
	Super::EndPlay(EndPlayReason);
}

bool USquadUnitSpatialVoiceLinePlayer::ShouldPlaySpatialVoiceLine(ERTSVoiceLine VoiceLineType) const
{
	
	const auto& Probs = DeveloperSettings::GamePlay::Audio::SquadUnits::SpatialVoiceLinePlayProbability;
	if (const float* Chance = Probs.Find(VoiceLineType))
	{
		return FMath::FRand() <= *Chance;
	}
	if (const UEnum* EnumPtr = StaticEnum<ERTSVoiceLine>())
	{
		const FString EnumName = EnumPtr->GetNameStringByValue(static_cast<int64>(VoiceLineType));
		RTSFunctionLibrary::ReportError("Did not find probability for spatial audio in the mapping. Type: " + EnumName);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Did not find probability for spatial audio, and could not retrieve enum name.");
	}

	return true;
}

float USquadUnitSpatialVoiceLinePlayer::GetBaseIntervalBetweenSpatialVoiceLines(float& OutFluxPercentage) const
{
	OutFluxPercentage = DeveloperSettings::GamePlay::Audio::SquadUnits::FluxPercentageIntervalBetweenSpatialVoiceLines;
	return DeveloperSettings::GamePlay::Audio::SquadUnits::BaseIntervalBetweenSpatialVoiceLines;
}

bool USquadUnitSpatialVoiceLinePlayer::BeginPlay_CheckEnabledForSpatialAudio()
{
	bIsEnabledForSpatialAudio = false;
	if (not IsValid(GetOwner()))
	{
		return bIsEnabledForSpatialAudio;
	}
	URTSComponent* RTSComp = GetOwner()->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComp))
	{
		return bIsEnabledForSpatialAudio;
	}
	// Enable for both player and enemy.
	if (RTSComp->GetOwningPlayer() > 0)
	{
		bIsEnabledForSpatialAudio = true;
	}
	return bIsEnabledForSpatialAudio;
}

void USquadUnitSpatialVoiceLinePlayer::OnSquadSet_SetupIdleVoiceLineCheck()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not GetIsValidOwningSquadUnit())
	{
		return;
	}

	constexpr float Base = DeveloperSettings::GamePlay::Audio::SquadUnits::TimeBetweenIdleVoiceLines;
	constexpr float FluxPct = DeveloperSettings::GamePlay::Audio::SquadUnits::FluxPercentageIntervalBetweenSpatialVoiceLines;
	const float RandPct = FMath::FRandRange(-FluxPct, FluxPct);
	const float Multi = 1.f + RandPct * 0.01f;
	const float CooldownTime = FMath::Max(0.01f, Base * Multi * GetSquadIndex());
	World->GetTimerManager().SetTimer(
		M_SquadIdleTimer,
		this,
		&USquadUnitSpatialVoiceLinePlayer::CheckPlayIdleOrCombatVoiceLine,
		CooldownTime,
		false);
}

void USquadUnitSpatialVoiceLinePlayer::BeginPlay_SetupOwnerAsSquadUnit()
{
	if (not IsValid(GetOwner()))
	{
		return;
	}
	ASquadUnit* SquadUnit = Cast<ASquadUnit>(GetOwner());
	M_OwningSquadUnit = SquadUnit;
	if (not GetIsValidOwningSquadUnit())
	{
		return;
	}
}


void USquadUnitSpatialVoiceLinePlayer::CheckPlayIdleOrCombatVoiceLine()
{
	if (not GetIsValidOwningSquadUnit())
	{
		StopIdleVoiceLineTimer();
		return;
	}
	const bool bIsInCombat = M_OwningSquadUnit->GetIsUnitInCombat();
	if (M_OwningSquadUnit->GetIsUnitIdle() && not bIsInCombat)
	{
		PlaySpatialVoiceLine(ERTSVoiceLine::IdleTalk, M_OwningSquadUnit->GetActorLocation(), false);
	}
	else if (bIsInCombat)
	{
		PlaySpatialVoiceLine(ERTSVoiceLine::InCombatTalk, M_OwningSquadUnit->GetActorLocation(), false);
	}
}

void USquadUnitSpatialVoiceLinePlayer::StopIdleVoiceLineTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SquadIdleTimer);
	}
}

bool USquadUnitSpatialVoiceLinePlayer::GetIsValidOwningSquadUnit() const
{
	if (M_OwningSquadUnit.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_OwningSquadUnit"),
		TEXT("GetIsValidOwningSquadUnit"),
		this);
	return false;
}

int32 USquadUnitSpatialVoiceLinePlayer::GetSquadIndex() const
{
	if (not GetIsValidOwningSquadUnit())
	{
		return 1;
	}
	if (M_OwningSquadUnit->GetSquadControllerChecked())
	{
		return M_OwningSquadUnit->GetSquadControllerChecked()->GetSquadUnitIndex(M_OwningSquadUnit.Get());
	}
	return 1;
}

